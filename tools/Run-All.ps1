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
    [int]$LaunchTimeout = 150,
    [switch]$Resume   # keep samples already marked PASS* in an existing ReportPath; re-run the rest
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
    # Lightweight reachability check only -- a bare TCP connect to the xbdm control
    # port. Do NOT open a full xbdm (threads) connection here: this runs before every
    # launch, and an extra xbdm connection per sample triggers the kit's
    # EnsurePersistentSockets reverse-connection wedge. A logo-hung kit (port open but
    # can't run titles) is caught instead by the launch itself timing out at the title's
    # initial break, which the retry/WEDGED path handles.
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

# Write the grouped pass/fail report. Called after every sample (so an interrupted
# multi-hour run still leaves a usable report and can be -Resume'd) and at the end.
function Write-Report {
    param($res, $path, $console, $config)
    $pass = @($res | Where-Object { $_.Status -like "PASS*" }).Count
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.AppendLine("RXDK-360 sample RUN report  (console $console, config $config)")
    [void]$sb.AppendLine("total $($res.Count)  pass $pass  other $($res.Count - $pass)")
    [void]$sb.AppendLine("")
    foreach ($grp in ($res | Group-Object Status | Sort-Object Name)) {
        [void]$sb.AppendLine("== $($grp.Name) ($($grp.Count)) ==")
        foreach ($r in ($grp.Group | Sort-Object Name)) {
            [void]$sb.AppendLine(("{0,-34} {1}" -f $r.Name, $r.Detail))
        }
        [void]$sb.AppendLine("")
    }
    Set-Content -LiteralPath $path -Value $sb.ToString() -Encoding UTF8
}

# A hard kit wedge (debug monitor won't reopen its reverse socket, title never reaches
# entry) can't be cleared by a cold reboot -- it needs a physical power-cycle. Rather
# than churn (or infinitely re-wedge on the same title), mark the culprit WEDGED, save
# the report, and exit so the user can power-cycle and re-run with -Resume. WEDGED is
# skipped on -Resume, so the sweep steps PAST the offending sample instead of looping.
function Stop-Wedged {
    param($res, $path, $console, $config, $culprit)
    Write-Report $res $path $console $config
    Write-Host ""
    Write-Host "*** KIT WEDGED on '$culprit' -- marked WEDGED (skipped on -Resume)."
    Write-Host "*** Power-cycle the kit, then re-run the SAME command with -Resume to continue."
    Write-Host "*** $($res.Count) result(s) saved to $path"
    exit 3
}

# A sample that faults reboots the kit, and the debug reverse connection often does
# NOT recover -- every launch after it reaches "pending" (exec state 4) but never the
# title's entry, so they pile up as bogus NOSTARTs. Detect that run and stop, so the
# NOSTARTs are not recorded as real results: the user power-cycles + re-adds the
# console in Neighborhood, then -Resume re-runs them (NOSTART is not kept on resume).
function Stop-ConnDead {
    param($res, $path, $console, $config, $n)
    Write-Report $res $path $console $config
    Write-Host ""
    Write-Host "*** DEBUG CONNECTION LIKELY DEAD ($n samples in a row never reached entry)."
    Write-Host "*** A prior fault rebooted the kit and the reverse connection did not recover."
    Write-Host "*** Power-cycle the kit + remove/re-add it in Neighborhood, then re-run with -Resume."
    Write-Host "*** (those NOSTART samples are not kept -- they re-run on resume.)"
    exit 4
}

# Every built .xex for the requested config (its parent dir is named for the config).
# -Filter *.xex also matches "Foo.xex.unsigned" (a Windows filter quirk), so pin the
# extension to exactly .xex -- those unsigned artifacts are not loadable titles.
$xexes = @(Get-ChildItem -LiteralPath $rootFull -Recurse -Filter *.xex -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension -eq ".xex" } |
    Where-Object { $_.Name -notmatch "(?i)\.unsigned\." } |   # unsigned pack intermediates (<name>.xex.unsigned.xex) won't launch
    Where-Object { $_.Directory.Name -eq $Config } |
    Where-Object { -not $onlySet -or $onlySet -contains $_.BaseName } |
    Sort-Object BaseName)

$results = @()
# -Resume: carry forward samples already marked PASS* in an existing report and don't
# re-run them (a prior run's confirmed passes are trustworthy). Everything else -- and
# anything not yet in the report -- is (re-)run.
if ($Resume -and (Test-Path -LiteralPath $ReportPath)) {
    $cur = ""
    $keep = @{}
    foreach ($line in (Get-Content -LiteralPath $ReportPath)) {
        if ($line -match "^== (.+?) \(\d+\) ==") { $cur = $matches[1]; continue }
        # Keep EVERY already-classified result (PASS/ALIVE/INIT/RENDER/FAIL/WEDGED/SKIP);
        # only NOSTART* is re-run, because those are the bogus results produced after a
        # fault killed the debug connection. This way a cataloged fault is recorded once
        # and auto-skipped on resume instead of re-crashing and re-killing the kit.
        if ($cur -and $cur -notlike "NOSTART*" -and $line -match "^(\S+)\s") { $keep[$matches[1]] = $cur }
    }
    foreach ($nm in $keep.Keys) { $results += [pscustomobject]@{ Name = $nm; Status = $keep[$nm]; Detail = "(prior run)" } }
    $xexes = @($xexes | Where-Object { -not $keep.ContainsKey($_.BaseName) })
    Write-Host "resume: keeping $($keep.Count) prior PASS; re-running $($xexes.Count)"
}

Write-Host "Run sweep: $($xexes.Count) sample(s)  config=$Config  console=$Console  listen=${ListenSeconds}s"
$n = 0
$consecNostart = 0
foreach ($x in $xexes) {
    $n++
    $name = $x.BaseName
    Write-Host ("[{0}/{1}] {2} ... " -f $n, $xexes.Count, $name) -NoNewline

    if (-not (Test-KitUp $Console)) {
        if (-not (Reset-Kit $Console)) { Stop-Wedged $results $ReportPath $Console $Config $name }
    }

    # Run debuglaunch inside a job so a hung/wedged launch hits a hard timeout instead
    # of stalling the whole sweep. The preceding title can bugcheck and reboot the kit,
    # so a launch may hit a box that isn't up yet: it comes back near-empty with
    # "waiting for debugger connection" / "DVD Emulator Not Running" and no modload.
    # Detect that and retry (after settling the kit) rather than mis-recording NOSTART.
    $wedged = $false
    $log = ""
    for ($try = 0; $try -lt 2; $try++) {   # one retry: handles a transient post-fault reboot; a hard hang then aborts
        $job = Start-Job -ScriptBlock {
            param($xbdm, $c, $xex, $ls)
            $env:RXDK_LISTEN = "$ls"
            $env:RXDK_XBDM_REGISTER = "1"
            & $xbdm debuglaunch $c $xex 2>&1
        } -ArgumentList $Xbdm, $Console, $x.FullName, $ListenSeconds
        if (Wait-Job $job -Timeout $LaunchTimeout) { $log = (Receive-Job $job) -join "`n"; $wedged = $false }
        else { Stop-Job $job; $log = "LAUNCH TIMEOUT (kit wedged?)"; $wedged = $true }
        Remove-Job $job -Force -ErrorAction SilentlyContinue
        # Retry only when the title never reached its entry AND the kit shows a boot /
        # reverse-connection problem (a launch into a still-rebooting kit). "exec state"
        # transitions and bootanim appear in EVERY launch's reboot phase, so they are
        # not failure signals on their own -- gate on the title not being reached.
        $reached = ($log -match "initial break" -or $log -match "\[RXDK\]" -or $log -match "modload name=" `
            -or $log -match "\[XAPI RETURN VALUE\]" -or $log -match "module\s+\S+\.xex\s+base=")
        $notReady = (-not $reached) -and ($wedged -or $log -match "timed out waiting for .* initial break" `
            -or $log -match "waiting for debugger connection" -or $log -match "DVD Emulator Not Running")
        if (-not $notReady) { break }
        Write-Host "(kit not ready, retrying) " -NoNewline
        if (-not (Reset-Kit $Console)) {
            $results += [pscustomobject]@{ Name = $name; Status = "WEDGED"; Detail = "kit did not come back up" }
            Stop-Wedged $results $ReportPath $Console $Config $name
        }
    }
    # Retries exhausted and the kit still never reached this title's entry: this launch
    # is wedging the kit. Mark it WEDGED and stop so the user can power-cycle + resume.
    if ($notReady) {
        $results += [pscustomobject]@{ Name = $name; Status = "WEDGED"; Detail = "never reached title entry after retries" }
        Stop-Wedged $results $ReportPath $Console $Config $name
    }

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
    # A title announces its load as "notify modload name=<n>.xex" / "Loaded EXE <n>.xex"
    # while running; the "module <n>.xex base=" table only prints if it stops at an init
    # break. Match any of them (a sample that loads, runs and returns to the dashboard
    # never stops, so only the modload/Loaded form appears) -> it's ALIVE, not NOSTART.
    $loaded = $log -match ("(?:module\s+|modload name=|Loaded EXE\s+)" + [regex]::Escape($name) + "\.xex")

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
    Write-Report $results $ReportPath $Console $Config   # incremental: survive interruption / enable -Resume

    # Two NOSTARTs in a row == the reverse connection died (a prior fault rebooted the
    # kit and it never came back for the debugger). Stop cleanly for recovery rather
    # than scribbling bogus NOSTARTs over the rest of the catalog.
    if ($status -like "NOSTART*") { $consecNostart++ } else { $consecNostart = 0 }
    if ($consecNostart -ge 2) { Stop-ConnDead $results $ReportPath $Console $Config $consecNostart }

    # ATG samples loop forever, so the next debuglaunch title-reboots over the
    # running one -- a fast warm reboot, no clean exit needed. Only cold-boot (slow)
    # when the kit is actually wedged: a faulting title can bugcheck it, so the next
    # title-reboot would fail. Probe the control port; cold reboot only if it's down.
    if ($wedged -or $fault -or -not (Test-KitUp $Console)) {
        if (-not (Reset-Kit $Console)) {
            $results[-1].Status = "WEDGED"; $results[-1].Detail = "kit unrecoverable after run"
            Stop-Wedged $results $ReportPath $Console $Config $name
        }
    }
}

Write-Report $results $ReportPath $Console $Config
$pass = @($results | Where-Object { $_.Status -like "PASS*" }).Count
Write-Host "report -> $ReportPath  (pass $pass / $($results.Count))"
