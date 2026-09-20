<#
.SYNOPSIS
    Build every sample and report pass/fail. Needs RXDK-360 installed (the
    "Xbox 360" VS platform + XDK toolchain) and the assets unpacked.

.DESCRIPTION
    Builds Common once, then each sample's .sln (incremental, so Common is reused).
    A build PASSES when its .xex is produced; the XDK Deploy step's "X1001 Could
    not connect" (no console) is ignored - the .xex is still built.

.PARAMETER Root
    Repo root (default: current directory).

.PARAMETER Only
    Optional comma list of sample names to limit to.

.PARAMETER ReportPath
    Where to write the pass/fail report (default: build-report.txt at root).
#>
[CmdletBinding()]
param(
    [string]$Root = ".",
    [string]$Only = "",
    [string]$ReportPath = "",
    # Which configuration(s) to build. Default Release (a practical smoke test);
    # pass e.g. "Debug,Release" or "All" for the full stock set.
    [string]$Configs = "Release"
)
Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$AllConfigs = @("CodeAnalysis", "Debug", "Profile", "Profile_FastCap", "Release", "Release_LTCG")
$configList = @(if ($Configs -eq "All") { $AllConfigs } else { $Configs.Split(",") | ForEach-Object { $_.Trim() } })

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

function Invoke-Build([string]$sln, [string]$cfg) {
    $out = & $msb $sln /p:Configuration=$cfg "/p:Platform=Xbox 360" /nologo /v:q /clp:ErrorsOnly 2>&1
    $name = [IO.Path]::GetFileNameWithoutExtension($sln)
    $xex = Join-Path (Join-Path (Split-Path $sln -Parent) $cfg) "$name.xex"
    $err = ($out | Where-Object { $_ -match "error " -and $_ -notmatch "xbecopy|Could not connect|X1001" } |
        Select-Object -First 1)
    return [pscustomobject]@{ Name = $name; Config = $cfg; Ok = (Test-Path $xex); Error = "$err".Trim() }
}

$results = New-Object System.Collections.Generic.List[object]

$slns = @(Get-ChildItem -LiteralPath $rootFull -Recurse -Filter *.sln |
    Where-Object { $_.FullName -notmatch "\\Common\\" } | Sort-Object FullName)
if ($onlySet.Count -gt 0) {
    $slns = @($slns | Where-Object { $onlySet -contains [IO.Path]::GetFileNameWithoutExtension($_.Name) })
}

$total = $slns.Count * $configList.Count
$i = 0; $pass = 0
foreach ($cfg in $configList) {
    foreach ($s in $slns) {
        $i++
        $r = Invoke-Build $s.FullName $cfg
        if ($r.Ok) { $pass++; $tag = "PASS" } else { $tag = "FAIL" }
        $results.Add($r)
        Write-Host ("[{0,4}/{1}] {2}  {3}  {4}" -f $i, $total, $tag, $cfg, $r.Name)
    }
}

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

Write-Host ("`nDONE: {0}/{1} passed. Report: {2}" -f $pass, $results.Count, $ReportPath)
if ($pass -lt $results.Count) { exit 1 }
