<#
.SYNOPSIS
    Build every sample and report pass/fail. Needs RXDK-360 installed (the
    "Xbox 360" VS platform + XDK toolchain) and the assets unpacked.

.DESCRIPTION
    Builds the shared ATG Common framework ONCE, then builds every sample's .sln
    in parallel across the machine's cores (each sample is an incremental build,
    so the up-to-date Common is reused, not rebuilt). A build PASSES when its .xex
    is produced; the XDK Deploy step's "X1001 Could not connect" (no console) is
    ignored - the .xex is still built.

.PARAMETER Root
    Repo root (default: current directory).

.PARAMETER Only
    Optional comma list of sample names to limit to.

.PARAMETER ReportPath
    Where to write the pass/fail report (default: build-report.txt at root).

.PARAMETER Configs
    Configuration(s): default Release; "Debug,Release" or "All" for the full set.

.PARAMETER Parallel
    Max concurrent sample builds (default: logical processor count, capped at 14).
    Pass 1 for the old serial behaviour.
#>
[CmdletBinding()]
param(
    [string]$Root = ".",
    [string]$Only = "",
    [string]$ReportPath = "",
    [string]$Configs = "Release",
    [int]$Parallel = 0
)
Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$AllConfigs = @("CodeAnalysis", "Debug", "Profile", "Profile_FastCap", "Release", "Release_LTCG")
$configList = @(if ($Configs -eq "All") { $AllConfigs } else { $Configs.Split(",") | ForEach-Object { $_.Trim() } })
if ($Parallel -le 0) {
    $Parallel = [Math]::Min([int]$env:NUMBER_OF_PROCESSORS, 14)
    if ($Parallel -le 0) { $Parallel = 8 }
}

function Find-MSBuild {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $p = & $vswhere -latest -prerelease -products * -requires Microsoft.Component.MSBuild `
            -find "MSBuild\**\Bin\MSBuild.exe" 2>$null | Select-Object -First 1
        if ($p) { return $p }
    }
    $c = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($c) { return $c.Source }
    throw "MSBuild.exe not found - is Visual Studio installed?"
}

$rootFull = (Resolve-Path -LiteralPath $Root).Path
if (-not $ReportPath) { $ReportPath = Join-Path $rootFull "build-report.txt" }
$msb = Find-MSBuild
$onlySet = @()
if ($Only) { $onlySet = @($Only.Split(",") | ForEach-Object { $_.Trim() }) }

# Common args every build shares. /m:1 keeps each sample a single-process build so
# the outer parallelism (one process per sample) owns the cores; DeploymentType=Xenia
# makes the Deploy target a no-op (no xbecopy console connect, which just times out).
# The Platform value has a space, so it is pre-quoted (Start-Process -ArgumentList
# joins array elements with spaces and does NOT add its own quotes).
$common = @('"/p:Platform=Xbox 360"', "/p:DeploymentType=Xenia", "/m:1", "/nologo", "/v:q", "/clp:ErrorsOnly")
$commonProj = Join-Path $rootFull "Common\Common.vcxproj"
$tmpDir = Join-Path ([IO.Path]::GetTempPath()) ("rxdk_sweep_" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tmpDir -Force | Out-Null

$slns = @(Get-ChildItem -LiteralPath $rootFull -Recurse -Filter *.sln |
    Where-Object { $_.FullName -notmatch "\\Common\\" } | Sort-Object FullName)
if ($onlySet.Count -gt 0) {
    $slns = @($slns | Where-Object { $onlySet -contains [IO.Path]::GetFileNameWithoutExtension($_.Name) })
}

$results = New-Object System.Collections.Generic.List[object]
$total = $slns.Count * $configList.Count
$done = 0; $pass = 0

foreach ($cfg in $configList) {
    # 1. Build the shared Common framework once (serial) so the parallel sample
    #    builds below find it up-to-date and never race to rebuild Common.lib.
    if (Test-Path $commonProj) {
        Write-Host ("Building Common ({0}) ..." -f $cfg)
        # Call operator quotes for us, so pass the space-bearing Platform unquoted.
        & $msb $commonProj "/p:Configuration=$cfg" "/p:Platform=Xbox 360" `
            "/p:DeploymentType=Xenia" /m:1 /nologo /v:q /clp:ErrorsOnly /t:Build 2>&1 | Out-Null
    }

    # 2. Build the samples in parallel: keep up to $Parallel msbuild processes in
    #    flight, each redirected to its own log; a sample passes if its .xex exists.
    $queue = [System.Collections.Queue]::new()
    foreach ($s in $slns) { $queue.Enqueue($s) }
    $active = @{}   # process id -> info

    function Reap($proc, $info) {
        $script:done++
        $out = ""
        if (Test-Path $info.Log) { $out = Get-Content -LiteralPath $info.Log -Raw -ErrorAction SilentlyContinue }
        $err = $null
        foreach ($line in ($out -split "`r?`n")) {
            if ($line -match "error " -and $line -notmatch "xbecopy|Could not connect|X1001") { $err = $line; break }
        }
        $ok = Test-Path $info.Xex
        if ($ok) { $script:pass++; $tag = "PASS" } else { $tag = "FAIL" }
        $script:results.Add([pscustomobject]@{ Name = $info.Name; Config = $cfg; Ok = $ok; Error = "$err".Trim() })
        Write-Host ("[{0,4}/{1}] {2}  {3}  {4}" -f $script:done, $total, $tag, $cfg, $info.Name)
        Remove-Item -LiteralPath $info.Log -ErrorAction SilentlyContinue
    }

    while ($queue.Count -gt 0 -or $active.Count -gt 0) {
        while ($active.Count -lt $Parallel -and $queue.Count -gt 0) {
            $s = $queue.Dequeue()
            $name = [IO.Path]::GetFileNameWithoutExtension($s.Name)
            $dir = Split-Path $s.FullName -Parent
            $xex = Join-Path (Join-Path $dir $cfg) "$name.xex"
            if (Test-Path $xex) { Remove-Item -LiteralPath $xex -ErrorAction SilentlyContinue }
            $log = Join-Path $tmpDir ($name + "_" + $cfg + ".log")
            # Build the sample's .vcxproj directly with BuildProjectReferences=false:
            # the shared Common was already built once above, so the parallel sample
            # builds must NOT rebuild it -- building the .sln would, and 14 of them
            # racing to rewrite Common\Release\Common.lib fails most of them (MSB3061
            # "access denied" / "Common.lib not found"). Fall back to the .sln only if
            # a same-named .vcxproj is not beside it.
            $vcx = Join-Path $dir "$name.vcxproj"
            $proj = if (Test-Path $vcx) { $vcx } else { $s.FullName }
            $args = @(('"' + $proj + '"'), "/p:Configuration=$cfg", "/p:BuildProjectReferences=false") + $common
            $p = Start-Process -FilePath $msb -ArgumentList $args -NoNewWindow -PassThru `
                -RedirectStandardOutput $log -RedirectStandardError ($log + ".err")
            $active[$p.Id] = [pscustomobject]@{ Proc = $p; Name = $name; Xex = $xex; Log = $log }
        }
        Start-Sleep -Milliseconds 150
        foreach ($id in @($active.Keys)) {
            if ($active[$id].Proc.HasExited) { $info = $active[$id]; $active.Remove($id); Reap $info.Proc $info }
        }
    }
}

Remove-Item -LiteralPath $tmpDir -Recurse -Force -ErrorAction SilentlyContinue

# Report: summary + failures grouped, so gaps are easy to categorise.
$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("RXDK-360 samples build report")
$lines.Add(("total {0}  pass {1}  fail {2}" -f $results.Count, $pass, ($results.Count - $pass)))
$lines.Add("")
$lines.Add("== FAIL ==")
foreach ($r in ($results | Where-Object { -not $_.Ok } | Sort-Object Config, Name)) {
    $lines.Add(("{0,-16} {1,-30} {2}" -f $r.Config, $r.Name, $r.Error))
}
$lines.Add("")
$lines.Add("== PASS ==")
foreach ($r in ($results | Where-Object { $_.Ok } | Sort-Object Config, Name)) {
    $lines.Add(("{0,-16} {1}" -f $r.Config, $r.Name))
}
[IO.File]::WriteAllText($ReportPath, ($lines -join "`r`n") + "`r`n", (New-Object System.Text.UTF8Encoding($false)))

Write-Host ("`nDONE: {0}/{1} passed ({2}-way parallel). Report: {3}" -f $pass, $results.Count, $Parallel, $ReportPath)
if ($pass -lt $results.Count) { exit 1 }
