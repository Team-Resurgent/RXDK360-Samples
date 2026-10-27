<#
.SYNOPSIS
    Deploy + run every built sample .xex on a real devkit and report run pass/fail,
    using the [RXDK] lifecycle markers ATG::Application emits (see Common\AtgApp.cpp).

.DESCRIPTION
    For each <Root>\**\<Config>\<name>.xex, debuglaunch it on the kit, stream the
    title's DbgPrint / exception notifications for -ListenSeconds, then classify by
    which lifecycle markers appeared:

        PASS             saw "[RXDK] rendering"        -- launched, initialized, drew a frame
        INIT(no-frame)   saw "[RXDK] initialized" only -- init ok, no frame in the window
        FAIL(init)       saw "[RXDK] entered" only      -- faulted during device create / Initialize()
        NOSTART          no marker (or launch timeout)  -- crashed at load, or the kit wedged

    Any CPU exception / bugcheck is recorded with its address + code (e.g. the
    STATUS_DATATYPE_MISALIGNMENT 0x80000002 faults we chased). After any non-clean
    run the kit is rebooted before the next sample so a wedged title can't poison
    the rest of the sweep. A per-sample log is written next to the report.

.PARAMETER Root           Sample checkout root (default: current directory).
.PARAMETER Console        Devkit IP / name (required), e.g. 192.168.1.123.
.PARAMETER Config         Configuration whose .xex to run (default: Release).
.PARAMETER Only           Optional comma list of sample names to limit to.
.PARAMETER ReportPath     Report path (default: <Root>\run-report.txt).
.PARAMETER ListenSeconds  Per-sample watch window after launch (default: 30).
.PARAMETER LaunchTimeout  Hard cap per sample before treating it as a wedge (default: 150).
#>
[CmdletBinding()]
param(
    [string]$Root = ".",
    [Parameter(Mandatory = $true)][string]$Console,
    [string]$Config = "Release",
    [string]$Only = "",
    [string]$ReportPath = "",
    [int]$ListenSeconds = 30,
    [int]$LaunchTimeout = 150
)
Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$Xbdm = "D:\Git\RXDK-360\vs20xx\debugger\Rxdk.Xbox360.Xbdm\bin\Release\net10.0-windows\win-x86\Rxdk.Xbox360.Xbdm.exe"
if (-not (Test-Path $Xbdm)) { throw "Xbdm CLI not found: $Xbdm" }

# The default shared/NoRegister connect reuses a stale name cache and fails
# (XBDM_CANNOTCONNECT) after a reboot or IP change; RXDK_XBDM_REGISTER=1 re-resolves
# the name. Set it for every xbdm call (debuglaunch/reboot), including the launch jobs.
$env:RXDK_XBDM_REGISTER = "1"

$rootFull = (Resolve-Path -LiteralPath $Root).Path
if (-not $ReportPath) { $ReportPath = Join-Path $rootFull "run-report.txt" }
$reportDir = [IO.Path]::GetDirectoryName($ReportPath)
$onlySet = if ($Only) { @($Only.Split(",") | ForEach-Object { $_.Trim() }) } else { $null }

# The Xbdm `info` verb ignores its arg and uses a connect path that does not honour
# RXDK_XBDM_REGISTER, so it mis-reports CANNOTCONNECT. Probe the xbdm control port
# (730) directly -- open means the kit's debug service is up.
function Test-KitUp {
    param($c)
    try { $t = New-Object Net.Sockets.TcpClient; $t.Connect($c, 730); $ok = $t.Connected; $t.Close(); return $ok }
    catch { return $false }
}

function Reset-Kit {
    param($c, $secs = 180)
    & $Xbdm reboot $c *>&1 | Out-Null
    $until = (Get-Date).AddSeconds($secs)
    while ((Get-Date) -lt $until) { Start-Sleep -Seconds 3; if (Test-KitUp $c) { return $true } }
    return $false
}

# Every built .xex for the requested config (its parent dir is named for the config).
$xexes = @(Get-ChildItem -LiteralPath $rootFull -Recurse -Filter *.xex -ErrorAction SilentlyContinue |
    Where-Object { $_.Directory.Name -eq $Config } |
    Where-Object { -not $onlySet -or $onlySet -contains $_.BaseName } |
    Sort-Object BaseName)

Write-Host "Run sweep: $($xexes.Count) sample(s)  config=$Config  console=$Console  listen=${ListenSeconds}s"
$results = @()
$n = 0
foreach ($x in $xexes) {
    $n++
    $name = $x.BaseName
    Write-Host ("[{0}/{1}] {2} ... " -f $n, $xexes.Count, $name) -NoNewline

    if (-not (Test-KitUp $Console)) { Reset-Kit $Console | Out-Null }

    # Run debuglaunch inside a job so a hung/wedged launch hits a hard timeout
    # instead of stalling the whole sweep.
    $job = Start-Job -ScriptBlock {
        param($xbdm, $c, $xex, $ls)
        $env:RXDK_LISTEN = "$ls"
        $env:RXDK_XBDM_REGISTER = "1"
        & $xbdm debuglaunch $c $xex 2>&1
    } -ArgumentList $Xbdm, $Console, $x.FullName, $ListenSeconds

    $wedged = $false
    if (Wait-Job $job -Timeout $LaunchTimeout) { $log = (Receive-Job $job) -join "`n" }
    else { Stop-Job $job; $log = "LAUNCH TIMEOUT (kit wedged?)"; $wedged = $true }
    Remove-Job $job -Force -ErrorAction SilentlyContinue

    $passed    = $log -match "\[RXDK\] pass"          # sustained RXDK_PASS_FRAMES frames (ATG loop samples)
    $rendering = $log -match "\[RXDK\] rendering"      # drew a frame, but faulted/hung before the threshold
    $initdone  = $log -match "\[RXDK\] initialized"
    $entered   = $log -match "\[RXDK\] entered"
    $fm = [regex]::Match($log, "exception addr=0x[0-9A-Fa-f]+ thread=0x[0-9A-Fa-f]+ code=0x[0-9A-Fa-f]+")
    $fault = if ($fm.Success) { $fm.Value } else { "" }
    # Samples that run to completion and return from main() (console/System samples)
    # don't loop, so they emit no frame marker -- but the CRT already logs their exit
    # code: "[XAPI RETURN VALUE] N". 0 = ran clean; non-zero = it failed (e.g. an asset
    # load failed and it exit()ed) -- exactly the case we want to catch, not call ok.
    $xm = [regex]::Match($log, "\[XAPI RETURN VALUE\]\s+(-?\d+)")
    $exitCode = if ($xm.Success) { [int]$xm.Groups[1].Value } else { $null }
    # Many System/non-ATG samples do their work then WAIT FOR A CONTROLLER BUTTON to
    # exit (no auto-return) and don't use the ATG render loop -- so no frame marker and
    # no exit code. But the launch log shows the title image loaded, and it survived the
    # whole listen window without a fault. That's a real "it ran without crashing"
    # signal: classify it ALIVE (ran; correctness needs an eyeball), distinct from a
    # title that never launched (NOSTART) or crashed (FAIL(fault)).
    $loaded = $log -match ("module\s+" + [regex]::Escape($name) + "\.xex")

    $status =
        if ($passed) { "PASS" }                                   # sustained render (ATG/XUI)
        elseif ($null -ne $exitCode -and $exitCode -eq 0) { "PASS(exit)" }   # ran to completion, returned 0
        elseif ($null -ne $exitCode) { "FAIL(exit=$exitCode)" }   # returned non-zero (e.g. asset load failed)
        elseif ($fault) { "FAIL(fault)" }                         # CPU exception / bugcheck
        elseif ($rendering) { "RENDER(partial)" }                 # drew a frame then hung before the threshold
        elseif ($initdone) { "INIT(no-frame)" }                   # ATG: initialized, never rendered
        elseif ($entered) { "FAIL(init)" }                        # ATG: entered, Initialize() failed
        elseif ($wedged) { "NOSTART(timeout)" }
        elseif ($loaded) { "ALIVE" }                              # launched, ran the window, no fault (e.g. waits for input)
        else { "NOSTART" }                                        # never launched
    if (-not $fault -and $null -ne $exitCode -and $exitCode -ne 0) { $fault = "exit=$exitCode" }

    $results += [pscustomobject]@{ Name = $name; Status = $status; Detail = $fault }
    Write-Host ("{0} {1}" -f $status, $fault)

    Set-Content -LiteralPath (Join-Path $reportDir ("runlog_" + $name + ".txt")) -Value $log -Encoding UTF8

    # ATG samples loop forever, so the next debuglaunch title-reboots over the
    # running one -- a fast warm reboot, no clean exit needed. Only cold-boot (slow)
    # when the kit is actually wedged: a faulting title can bugcheck it, so the next
    # title-reboot would fail. Probe the control port; cold reboot only if it's down.
    if ($wedged -or -not (Test-KitUp $Console)) { Reset-Kit $Console | Out-Null }
}

$pass = @($results | Where-Object { $_.Status -eq "PASS" }).Count
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("RXDK-360 sample RUN report  (console $Console, config $Config)")
[void]$sb.AppendLine("total $($results.Count)  pass $pass  other $($results.Count - $pass)")
[void]$sb.AppendLine("")
foreach ($grp in ($results | Group-Object Status | Sort-Object Name)) {
    [void]$sb.AppendLine("== $($grp.Name) ($($grp.Count)) ==")
    foreach ($r in ($grp.Group | Sort-Object Name)) {
        [void]$sb.AppendLine(("{0,-34} {1}" -f $r.Name, $r.Detail))
    }
    [void]$sb.AppendLine("")
}
Set-Content -LiteralPath $ReportPath -Value $sb.ToString() -Encoding UTF8
Write-Host "report -> $ReportPath  (pass $pass / $($results.Count))"
