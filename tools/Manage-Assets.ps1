<#
.SYNOPSIS
    Pack / unpack the samples' binary assets as sub-100 MB git-friendly parts.

.DESCRIPTION
    The samples ship large binary runtime assets (textures, scenes, audio, video:
    ~800 MB). Committing them raw hits GitHub's 100 MB/file limit and bloats
    clones; Git LFS needs a paid quota on a public repo. Instead the assets are
    kept OUT of git (see .gitignore) and a single deterministic .zip of them is
    committed, split into fixed-size parts under assets\. After cloning:

        pwsh tools\Manage-Assets.ps1 unpack     # materialise every asset in place

    Maintainers re-pack after changing assets:

        pwsh tools\Manage-Assets.ps1 pack
        pwsh tools\Manage-Assets.ps1 check      # verify parts match the manifest

    Uses only .NET (System.IO.Compression) - no external tar, works on Windows
    PowerShell 5.1 and PowerShell 7+.

.PARAMETER Action
    pack | unpack | check

.PARAMETER Root
    Repo root (default: current directory).

.PARAMETER PartSizeMB
    Split size in MB (default 45 - safely under GitHub's 50 MB warning).
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][ValidateSet("pack", "unpack", "check")][string]$Action,
    [string]$Root = ".",
    [int]$PartSizeMB = 45
)
Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem | Out-Null

# Binary asset extensions to pack. Keep in sync with .gitignore. Source stays in
# git: .cpp .h .hlsl .fx .rdf .xml .txt .inl ...
$AssetExt = @(
    ".xpr", ".xpr2", ".xzp", ".tga", ".bmp", ".dds", ".png", ".jpg", ".jpeg",
    ".gif", ".wav", ".xma", ".xma2", ".wmv", ".mov", ".mp4", ".bik", ".ttf",
    ".x", ".xed", ".oc", ".pmem", ".xbg", ".xatg", ".abc", ".bin", ".xpv",
    ".xsb", ".xwb", ".xgs", ".wma", ".ico"
)
$AssetsDir = "assets"
$BaseName  = "samples-assets.zip"     # logical; real files are BaseName.part.NNN
$Manifest  = "assets.manifest"
$PartSize  = $PartSizeMB * 1024 * 1024
$FixedTime = New-Object System.DateTimeOffset(1980, 1, 1, 0, 0, 0, [TimeSpan]::Zero)

$rootFull  = (Resolve-Path -LiteralPath $Root).Path
$outDir    = Join-Path $rootFull $AssetsDir

function Get-AssetFiles {
    $sep = [IO.Path]::DirectorySeparatorChar
    $skip = (Join-Path $rootFull $AssetsDir)
    $list = New-Object System.Collections.Generic.List[string]
    foreach ($f in Get-ChildItem -LiteralPath $rootFull -Recurse -File) {
        if ($f.FullName.StartsWith($skip, [StringComparison]::OrdinalIgnoreCase)) { continue }
        if ($f.FullName -match "\\\.git\\") { continue }
        if ($AssetExt -contains $f.Extension.ToLowerInvariant()) {
            $rel = $f.FullName.Substring($rootFull.Length).TrimStart($sep).Replace("\", "/")
            $list.Add($rel)
        }
    }
    $arr = $list.ToArray()
    [Array]::Sort($arr, [System.StringComparer]::Ordinal)
    return $arr
}

function Get-Sha256([string]$path) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $fs = [IO.File]::OpenRead($path)
        try { return ([BitConverter]::ToString($sha.ComputeHash($fs))).Replace("-", "").ToLowerInvariant() }
        finally { $fs.Dispose() }
    } finally { $sha.Dispose() }
}

function Get-PartPaths {
    Get-ChildItem -LiteralPath $outDir -Filter "$BaseName.part.*" -File -ErrorAction SilentlyContinue |
        Sort-Object Name | ForEach-Object { $_.FullName }
}

function Split-File([string]$src, [string]$dstDir) {
    $buf = New-Object byte[] (4 * 1024 * 1024)
    $in = [IO.File]::OpenRead($src)
    $parts = New-Object System.Collections.Generic.List[string]
    try {
        $idx = 0; $out = $null; $inPart = 0
        while (($n = $in.Read($buf, 0, $buf.Length)) -gt 0) {
            $off = 0
            while ($off -lt $n) {
                if ($null -eq $out -or $inPart -ge $PartSize) {
                    if ($out) { $out.Dispose() }
                    $name = Join-Path $dstDir ("{0}.part.{1:D3}" -f $BaseName, $idx); $idx++
                    $out = [IO.File]::Create($name); $parts.Add($name); $inPart = 0
                }
                $room = [Math]::Min($PartSize - $inPart, $n - $off)
                $out.Write($buf, $off, $room); $off += $room; $inPart += $room
            }
        }
        if ($out) { $out.Dispose() }
    } finally { $in.Dispose() }
    return $parts
}

function Join-Parts([string]$dst, [string[]]$parts) {
    $buf = New-Object byte[] (4 * 1024 * 1024)
    $out = [IO.File]::Create($dst)
    try {
        foreach ($p in $parts) {
            $in = [IO.File]::OpenRead($p)
            try { while (($n = $in.Read($buf, 0, $buf.Length)) -gt 0) { $out.Write($buf, 0, $n) } }
            finally { $in.Dispose() }
        }
    } finally { $out.Dispose() }
}

function Invoke-Pack {
    $files = Get-AssetFiles
    if (-not (Test-Path -LiteralPath $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }
    Get-ChildItem -LiteralPath $outDir -Filter "$BaseName.part.*" -File -ErrorAction SilentlyContinue | Remove-Item -Force
    $tmp = Join-Path ([IO.Path]::GetTempPath()) ("rxdk_assets_" + [Guid]::NewGuid().ToString("N") + ".zip")
    try {
        $zip = [IO.Compression.ZipFile]::Open($tmp, [IO.Compression.ZipArchiveMode]::Create)
        try {
            foreach ($rel in $files) {
                $full = Join-Path $rootFull ($rel.Replace("/", "\"))
                # Create the entry, stamp a fixed time BEFORE writing (Create mode
                # forbids changing it once the entry stream is opened), then copy.
                $entry = $zip.CreateEntry($rel, [IO.Compression.CompressionLevel]::Optimal)
                $entry.LastWriteTime = $FixedTime      # determinism: no wall-clock mtime
                $es = $entry.Open()
                try {
                    $fs = [IO.File]::OpenRead($full)
                    try { $fs.CopyTo($es) } finally { $fs.Dispose() }
                } finally { $es.Dispose() }
            }
        } finally { $zip.Dispose() }

        $parts = Split-File $tmp $outDir
        $man = Join-Path $outDir $Manifest
        $sw = New-Object System.IO.StreamWriter($man, $false, (New-Object System.Text.UTF8Encoding($false)))
        $sw.NewLine = "`n"
        try {
            $sw.WriteLine("# RXDK-360 sample assets - reassemble with tools\Manage-Assets.ps1 unpack")
            $sw.WriteLine("files $($files.Count)")
            $sw.WriteLine("parts $($parts.Count)")
            foreach ($p in $parts) { $sw.WriteLine("$([IO.Path]::GetFileName($p)) $(Get-Sha256 $p)") }
        } finally { $sw.Dispose() }

        $total = ($parts | ForEach-Object { (Get-Item $_).Length } | Measure-Object -Sum).Sum
        Write-Host ("packed {0} asset(s) -> {1} part(s), {2:N1} MB" -f $files.Count, $parts.Count, ($total / 1MB))
    } finally { if (Test-Path $tmp) { Remove-Item $tmp -Force } }
    return 0
}

function Invoke-Unpack {
    $parts = @(Get-PartPaths)
    if ($parts.Count -eq 0) { Write-Error "no asset parts under $outDir"; return 2 }
    $tmp = Join-Path ([IO.Path]::GetTempPath()) ("rxdk_assets_" + [Guid]::NewGuid().ToString("N") + ".zip")
    $n = 0
    try {
        Join-Parts $tmp $parts
        $zip = [IO.Compression.ZipFile]::OpenRead($tmp)
        try {
            foreach ($entry in $zip.Entries) {
                if ($entry.FullName.EndsWith("/")) { continue }
                $safe = $entry.FullName.Replace("/", "\")
                if ($safe.StartsWith("\") -or $safe.Contains("..")) { continue }  # our tar, but be safe
                $dest = Join-Path $rootFull $safe
                $dir = Split-Path $dest -Parent
                if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $dest, $true)
                $n++
            }
        } finally { $zip.Dispose() }
    } finally { if (Test-Path $tmp) { Remove-Item $tmp -Force } }
    Write-Host ("unpacked {0} asset(s)" -f $n)
    return 0
}

function Invoke-Check {
    $man = Join-Path $outDir $Manifest
    if (-not (Test-Path -LiteralPath $man)) { Write-Error "no manifest - run: Manage-Assets.ps1 pack"; return 1 }
    $want = @{}
    foreach ($line in [IO.File]::ReadAllLines($man)) {
        if ($line -match "^([^#\s]\S*\.part\.\d+)\s+([0-9a-f]{64})$") { $want[$Matches[1]] = $Matches[2] }
    }
    $bad = 0
    foreach ($p in (Get-PartPaths)) {
        $base = [IO.Path]::GetFileName($p)
        if (-not $want.ContainsKey($base)) { Write-Host "unexpected part: $base"; $bad++; continue }
        if ((Get-Sha256 $p) -ne $want[$base]) { Write-Host "sha mismatch: $base"; $bad++ }
        $want.Remove($base) | Out-Null
    }
    foreach ($k in $want.Keys) { Write-Host "missing part: $k"; $bad++ }
    if ($bad -gt 0) { Write-Error "$bad asset part problem(s) - re-pack"; return 1 }
    Write-Host "asset parts verified"
    return 0
}

switch ($Action) {
    "pack"   { exit (Invoke-Pack) }
    "unpack" { exit (Invoke-Unpack) }
    "check"  { exit (Invoke-Check) }
}
