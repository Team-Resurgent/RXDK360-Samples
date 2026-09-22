<#
.SYNOPSIS
    Generate modern MSBuild projects (.vcxproj + .sln) for the Xbox 360 XDK samples.

.DESCRIPTION
    The stock XDK samples ship as source + media only; this writes, for each
    sample, a project on the installed "Xbox 360" VS platform (toolset 2010-01)
    with the full stock XDK configuration set - CodeAnalysis, Debug, Profile,
    Profile_FastCap, Release, Release_LTCG - so they open and build in VS2022 /
    VS "18" via the RXDK-360 integration. The ATG framework under Common\ is
    emitted once as a static library every sample references.

    Layout expected (and produced) at -Root:
        <Root>\Common\<AtgXxx.cpp...>      + Common.vcxproj
        <Root>\<Area>\<Sample>\<src...>    + <Sample>.vcxproj + <Sample>.sln

    Settings mirror the stock XDK vcxproj (see README "Why these settings"):
    platform literally "Xbox 360"; ANSI/MultiByte (platform default); _XBOX
    defined explicitly (StaticLibrary configs miss it, breaking ATG xnamath.h);
    per-config optimization/runtime + the XDK title link set with the right
    per-config library variant (Debug -> 'd', Profile -> instrumented 'i',
    Release_LTCG -> 'ltcg'), which the platform does not import. Per-sample
    category libraries are detected from source, and samples with a .gameconfig
    get a spac pre-build step to generate their <name>.spa.h.

.PARAMETER Root
    Samples root (default: current directory).

.PARAMETER Check
    Report out-of-date projects and write nothing (exit 1 if any). For CI.
#>
[CmdletBinding()]
param(
    [string]$Root = ".",
    [switch]$Check
)
Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$Platform    = "Xbox 360"
$Toolset     = "clang"
$VcxprojType = "8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942"
$CommonName  = "Common"
$CompileExt  = @(".cpp", ".cxx", ".cc", ".c")
$HeaderExt   = @(".h", ".hpp", ".inl")
$IgnoreTop   = @(".git", ".github", "tools", "Common", "assets")

# The stock XDK title link sets, per configuration, verbatim from a stock XDK
# vcxproj. Each config uses a specific library variant for the core libs
# (Debug -> 'd', Profile -> instrumented 'i', Release_LTCG -> 'ltcg').
$LibsDebug   = @("xapilibd.lib","d3d9d.lib","d3dx9d.lib","xgraphicsd.lib","xboxkrnl.lib","xnetd.lib","xaudiod2.lib","xactd3.lib","x3daudiod.lib","xmcored.lib","xbdm.lib","vcompd.lib")
$LibsProfile = @("xapilibi.lib","d3d9i.lib","d3dx9.lib","xgraphics.lib","xboxkrnl.lib","xnet.lib","xaudio2.lib","xact3i.lib","x3daudioi.lib","xmcorei.lib","xbdm.lib","vcomp.lib")
$LibsFast    = @("xapilib.lib","d3d9.lib","d3dx9.lib","xgraphics.lib","xboxkrnl.lib","xnet.lib","xaudio2.lib","xact3.lib","x3daudio.lib","xmcore.lib","vcomp.lib","xbdm.lib")
$LibsRelease = @("xapilib.lib","d3d9.lib","d3dx9.lib","xgraphics.lib","xboxkrnl.lib","xnet.lib","xaudio2.lib","xact3.lib","x3daudio.lib","xmcore.lib","vcomp.lib")
$LibsLtcg    = @("xapilib.lib","d3d9ltcg.lib","d3dx9.lib","xgraphics.lib","xboxkrnl.lib","xnet.lib","xaudio2.lib","xact3ltcg.lib","x3daudioltcg.lib","xmcoreltcg.lib","vcomp.lib")

# The six stock configurations. CatVariant selects the per-config suffix applied
# to detected *category* libraries (below). Ignore adds /NODEFAULTLIB (Profile
# links xapilibi and must ignore the plain xapilib a #pragma may pull).
$Configs = @(
    @{ Name = "CodeAnalysis";    Opt = "Disabled"; Rt = "MultiThreadedDebug"; Defs = "_DEBUG;_XBOX";               Base = $LibsDebug;   CatVariant = "debug"; Wpo = $false; Ignore = @() },
    @{ Name = "Debug";           Opt = "Disabled"; Rt = "MultiThreadedDebug"; Defs = "_DEBUG;_XBOX";               Base = $LibsDebug;   CatVariant = "debug"; Wpo = $false; Ignore = @() },
    @{ Name = "Profile";         Opt = "Full";     Rt = "MultiThreaded";      Defs = "NDEBUG;_XBOX;PROFILE";       Base = $LibsProfile; CatVariant = "plain"; Wpo = $false; Ignore = @("xapilib.lib") },
    @{ Name = "Profile_FastCap"; Opt = "Full";     Rt = "MultiThreaded";      Defs = "NDEBUG;_XBOX;PROFILE;FASTCAP"; Base = $LibsFast;  CatVariant = "plain"; Wpo = $false; Ignore = @() },
    @{ Name = "Release";         Opt = "Full";     Rt = "MultiThreaded";      Defs = "NDEBUG;_XBOX";               Base = $LibsRelease; CatVariant = "plain"; Wpo = $false; Ignore = @() },
    @{ Name = "Release_LTCG";    Opt = "Full";     Rt = "MultiThreaded";      Defs = "NDEBUG;_XBOX;LTCG";          Base = $LibsLtcg;    CatVariant = "ltcg";  Wpo = $true;  Ignore = @() }
)

# Release category library -> Debug / LTCG variant (irregular XDK naming).
$DebugLib = @{
    "xhv2.lib"="xhvd2.lib"; "nuiapi.lib"="nuiapid.lib"; "nuihandles.lib"="nuihandlesd.lib";
    "st.lib"="std.lib"; "nuispeech.lib"="nuispeechd.lib"; "nuifitnessapi.lib"="nuifitnessapid.lib";
    "xonline.lib"="xonlined.lib"; "xparty.lib"="xpartyd.lib"; "xavatar2.lib"="xavatar2d.lib";
    "xmic.lib"="xmicd.lib"; "xuirun.lib"="xuirund.lib"; "xuirender.lib"="xuirenderd.lib";
    "xuihtml.lib"="xuihtmld.lib"; "xuivideo.lib"="xuivideod.lib"; "xav.lib"="xavd.lib";
    "xime.lib"="ximed.lib"; "xhttp.lib"="xhttpd.lib"; "xauth.lib"="xauthd.lib";
    "xmp.lib"="xmpd.lib"; "xffb.lib"="xffbd.lib"; "xcam.lib"="xcamd.lib"; "xjson.lib"="xjsond.lib";
    "xinput2.lib"="xinput2d.lib"; "xmedia2.lib"="xmediad2.lib"; "xwmadecode.lib"="xwmadecoded.lib";
    "dxerr9.lib"="dxerr9.lib"; "tracerecording.lib"="tracerecordingd.lib"; "xrnm.lib"="xrnmd.lib";
    "NuiAudio.lib"="NuiAudiod.lib"; "xmahal.lib"="xmahald.lib"; "xinputremap.lib"="xinputremapd.lib";
    "xgetserviceendpoint.lib"="xgetserviceendpointd.lib"; "multidisc.lib"="multidiscd.lib";
    "xsocialpost.lib"="xsocialpostd.lib"; "xtms.lib"="xtmsd.lib"; "qnetxaudio2.lib"="qnetxaudio2d.lib"
}
$LtcgLib = @{ "xuirender.lib"="xuirenderltcg.lib"; "xavatar2.lib"="xavatar2ltcg.lib"; "st.lib"="stltcg.lib" }

# Category libraries keyed by a signal in the sample's source.
$LibTriggers = @(
    @{ t = @("xhv2.h", "XHV2");                                          libs = @("xhv2.lib") },
    @{ t = @("nuiapi.h", "AtgNui", "NuiImageStream", "NuiSkeleton", "NuiInitialize", "NUI_"); libs = @("nuiapi.lib", "nuihandles.lib", "st.lib") },
    @{ t = @("nuispeech.h", "NuiSpeech");                                libs = @("nuispeech.lib", "NuiAudio.lib") },
    @{ t = @("nuifitness", "NuiFitness");                                libs = @("nuifitnessapi.lib") },
    @{ t = @("xonline.h", "XOnline", "XSessionCreate", "XSessionSearch"); libs = @("xonline.lib") },
    @{ t = @("xparty.h", "XParty", "XShowCommunitySessions");            libs = @("xparty.lib") },
    @{ t = @("XStudioApi.h", "XStudio", "XStudioStart", "XStudioMapStreams"); libs = @("xstudio.lib") },
    @{ t = @("xavatar", "XAvatar");                                      libs = @("xavatar2.lib") },
    @{ t = @("xmic.h", "XMic");                                          libs = @("xmic.lib") },
    @{ t = @("xui.h", "xuiapp.h", "XuiInit", "XuiRender", "XuiElement", "XuiDrawText"); libs = @("xuirun.lib", "xuirender.lib") },
    @{ t = @("xuihtml.h", "XuiHtml");                                    libs = @("xuihtml.lib") },
    @{ t = @("xuivideo.h", "XuiVideo");                                  libs = @("xuivideo.lib", "xmedia2.lib") },
    @{ t = @("xav.h", "XAVCreate", "XAV_", "CreateXAV", "IXAVPlayer", "AsfWriter", "IAsfWriter"); libs = @("xav.lib", "xhttp.lib", "xauth.lib") },
    @{ t = @("qnet.h", "QNetCreate", "IQNet");                           libs = @("qnetxaudio2.lib", "xcam.lib", "xhv2.lib") },
    @{ t = @("xime.h", "XimeXui", "XIME");                               libs = @("xime.lib") },
    @{ t = @("xhttp.h", "XHttp", "AtgHttp", "AtgRest");                  libs = @("xhttp.lib", "xauth.lib") },
    @{ t = @("xauth.h", "XAuth");                                        libs = @("xauth.lib") },
    @{ t = @("xmp.h", "XMPGet", "XMP_");                                 libs = @("xmp.lib") },
    @{ t = @("xffb.h", "XFFB");                                          libs = @("xffb.lib") },
    @{ t = @("xcam.h", "XCamera", "XCAMERA");                            libs = @("xcam.lib", "xav.lib", "xhttp.lib", "xauth.lib") },
    @{ t = @("xjson.h", "XJSON", "AtgJson", "AtgRest");                  libs = @("xjson.lib") },
    @{ t = @("xinput2.h", "XInput2", "XINPUTID_");                       libs = @("xinput2.lib") },
    @{ t = @("xmedia2.h", "xmedia.h", "IXMedia2", "XmvPlayer", "XMedia2", "AtgMediaLocator"); libs = @("xmedia2.lib") },
    @{ t = @("xwmadecode.h", "XWMA");                                    libs = @("xwmadecode.lib") },
    @{ t = @("dxerr9.h", "DXGetErrorString9");                          libs = @("dxerr9.lib") },
    @{ t = @("tracerecording.h", "XTrace");                             libs = @("tracerecording.lib") },
    @{ t = @("xrnm.h", "Xrnm", "XRNM");                                  libs = @("xrnm.lib") },
    @{ t = @("NuiAudio", "nuiaudio");                                    libs = @("NuiAudio.lib") },
    @{ t = @("xmahal.h", "XMAHal", "XMAPlayback", "XMACreate");          libs = @("xmahal.lib") },
    @{ t = @("xinputremap.h", "XInputRemap");                           libs = @("xinputremap.lib") },
    @{ t = @("xgetserviceendpoint.h", "XGetServiceEndpoint");           libs = @("xgetserviceendpoint.lib") },
    @{ t = @("multidisc.h", "XSwapDisc", "XMultiDisc");                 libs = @("multidisc.lib") },
    @{ t = @("xsocialpost.h", "XSocialPost", "XShowSocialNetwork");     libs = @("xsocialpost.lib") },
    @{ t = @("xtms.h", "XReportData", "XTitleServer");                  libs = @("xtms.lib") }
)

# A sample whose sources include <windows.h>/<winsock2.h> but not <xtl.h> is a
# PC-side host tool (debugger/automation helper), not an Xbox title - skip it.
function Test-HostTool([string]$blob) {
    $win = $blob.Contains("<windows.h>") -or $blob.Contains("<Windows.h>") -or $blob.Contains("<winsock2.h>")
    return ($win -and -not $blob.Contains("<xtl.h>"))
}

$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Get-StableGuid([string]$key) {
    $md5 = [System.Security.Cryptography.MD5]::Create()
    try { $h = $md5.ComputeHash([System.Text.Encoding]::UTF8.GetBytes("RXDK360:" + $key)) }
    finally { $md5.Dispose() }
    return ('{0:X2}{1:X2}{2:X2}{3:X2}-{4:X2}{5:X2}-{6:X2}{7:X2}-{8:X2}{9:X2}-{10:X2}{11:X2}{12:X2}{13:X2}{14:X2}{15:X2}' -f `
        $h[0], $h[1], $h[2], $h[3], $h[4], $h[5], $h[6], $h[7], $h[8], $h[9], $h[10], $h[11], $h[12], $h[13], $h[14], $h[15])
}

function ConvertTo-Xml([string]$s) {
    return $s.Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;").Replace('"', "&quot;")
}

function Get-RelPath([string]$fromDir, [string]$to) {
    $f = (Resolve-Path -LiteralPath $fromDir).Path
    if (-not $f.EndsWith([IO.Path]::DirectorySeparatorChar)) { $f += [IO.Path]::DirectorySeparatorChar }
    $fromUri = New-Object System.Uri($f)
    $toUri = New-Object System.Uri((Resolve-Path -LiteralPath $to).Path)
    return [System.Uri]::UnescapeDataString($fromUri.MakeRelativeUri($toUri).ToString()).Replace("/", "\")
}

function Get-Sources([string]$dir) {
    $cpps = New-Object System.Collections.Generic.List[string]
    $hdrs = New-Object System.Collections.Generic.List[string]
    $base = (Resolve-Path -LiteralPath $dir).Path
    $files = @(Get-ChildItem -LiteralPath $dir -Recurse -File)
    # A subdirectory with its OWN main() is a separate tool program
    # (HOCDetectorTrainer, GestureDetectorTrainer/*) -- exclude its whole tree so
    # the title does not compile a second entry point. Guard: only do this when the
    # sample ROOT already has a main (the title). If the title itself lives in a
    # subdir (SimpleTexture -> TextureLoadDemo/, with a builder alongside), excluding
    # subdir-mains would drop the title, so leave those samples untouched.
    $mainRe = '(?m)^\s*(INT|int|VOID|void)\s+(__cdecl\s+|_cdecl\s+)?main\s*\('
    $rootHasMain = $false
    $subMainTops = @{}
    foreach ($f in $files) {
        if ($CompileExt -notcontains $f.Extension.ToLowerInvariant()) { continue }
        $rel = $f.FullName.Substring($base.Length).TrimStart([IO.Path]::DirectorySeparatorChar).Replace("/", "\")
        $parts = $rel.Split("\")
        $hasMain = $false
        try { $hasMain = ((Get-Content -LiteralPath $f.FullName -Raw) -match $mainRe) } catch {}
        if (-not $hasMain) { continue }
        if ($parts.Count -eq 1) { $rootHasMain = $true } else { $subMainTops[$parts[0]] = $true }
    }
    $excludeTop = @{}
    if ($rootHasMain) { $excludeTop = $subMainTops }
    foreach ($f in $files) {
        $ext = $f.Extension.ToLowerInvariant()
        $rel = $f.FullName.Substring($base.Length).TrimStart([IO.Path]::DirectorySeparatorChar).Replace("/", "\")
        $parts = $rel.Split("\")
        if ($parts.Count -gt 1 -and $excludeTop.ContainsKey($parts[0])) { continue }
        if ($CompileExt -contains $ext) { $cpps.Add($rel) }
        elseif ($HeaderExt -contains $ext) { $hdrs.Add($rel) }
    }
    $cmp = [System.StringComparer]::OrdinalIgnoreCase
    $ca = $cpps.ToArray(); [Array]::Sort($ca, $cmp)
    $ha = $hdrs.ToArray(); [Array]::Sort($ha, $cmp)
    return @{ cpps = $ca; hdrs = $ha }
}

function Read-Blob([string]$dir, [string[]]$rels) {
    $sb = New-Object System.Text.StringBuilder
    foreach ($r in $rels) { try { [void]$sb.AppendLine([IO.File]::ReadAllText((Join-Path $dir $r))) } catch { } }
    return $sb.ToString()
}

# Category (extra) libraries only - the base title set is per-config.
function Get-CategoryLibs([string]$blob) {
    $libs = New-Object System.Collections.Generic.List[string]
    foreach ($rule in $LibTriggers) {
        $hit = $false
        foreach ($tok in $rule.t) { if ($blob.Contains($tok)) { $hit = $true; break } }
        if ($hit) { foreach ($lib in $rule.libs) { if (-not $libs.Contains($lib)) { $libs.Add($lib) } } }
    }
    return $libs.ToArray()
}

function ConvertTo-Variant([string]$lib, [string]$variant) {
    switch ($variant) {
        "debug" { if ($DebugLib.ContainsKey($lib)) { return $DebugLib[$lib] } return ($lib -replace '\.lib$', 'd.lib') }
        "ltcg"  { if ($LtcgLib.ContainsKey($lib))  { return $LtcgLib[$lib] }  return $lib }
        default { return $lib }
    }
}

# A spac invocation to generate the SPA header, if the sample carries a
# .gameconfig, else $null. The generated header is named after the source's
# #include "...spa.h" (which need not match the gameconfig base, e.g.
# TitleStorageSample.gameconfig -> TitleStorage.spa.h). Returned as a target
# descriptor (the platform ignores PreBuildEvent, so a real Target is used).
function Get-SpaStep([string]$dir, [string]$blob) {
    # A .gameconfig and a .xlast are the same XML schema (XboxLiveSubmissionProject
    # -> GameConfigProject); spac.exe reads either. Some samples ship only the
    # .xlast (QNet/ArcadeSample/XStorage), so fall back to it when there is no
    # .gameconfig.
    $m = [regex]::Match($blob, '#include\s*"([^"]*\.spa\.h)"', 'IgnoreCase')
    # Only titles that actually #include a .spa.h need one. A .xlast may also be a
    # content/package project (ArcadeLicenseCheck) that spac cannot compile as a
    # game config -- don't emit a spurious step for it.
    if (-not $m.Success) { return $null }
    $wantBase = [IO.Path]::GetFileNameWithoutExtension([IO.Path]::GetFileName($m.Groups[1].Value.Replace("/", "\")))
    $gc = Get-ChildItem -LiteralPath $dir -Recurse -Filter *.gameconfig -File | Select-Object -First 1
    if (-not $gc) {
        # Prefer the .xlast whose base name matches the wanted .spa.h (the game
        # config), not an unrelated Package/Submission .xlast in the same tree.
        $xl = @(Get-ChildItem -LiteralPath $dir -Recurse -Filter *.xlast -File)
        if ($wantBase) { $gc = $xl | Where-Object { [IO.Path]::GetFileNameWithoutExtension($_.Name) -eq $wantBase } | Select-Object -First 1 }
        if (-not $gc) { $gc = $xl | Where-Object { $_.Name -notmatch 'Package|Submission' } | Select-Object -First 1 }
        if (-not $gc) { $gc = $xl | Select-Object -First 1 }
    }
    if (-not $gc) { return $null }
    $base = (Resolve-Path -LiteralPath $dir).Path
    $gcRel = $gc.FullName.Substring($base.Length).TrimStart([IO.Path]::DirectorySeparatorChar).Replace("/", "\")
    if ($m.Success) { $hdr = $m.Groups[1].Value.Replace("/", "\") }
    else { $hdr = [IO.Path]::GetFileNameWithoutExtension($gc.Name) + ".spa.h" }
    $spa = [IO.Path]::GetFileNameWithoutExtension($gc.Name) + ".spa"
    $cmd = ('"$(RxdkBinDir)\spac.exe" -nologo -forceoverwrite -h "$(ProjectDir){0}" -o "$(ProjectDir){1}" "$(ProjectDir){2}"' -f $hdr, $spa, $gcRel)
    return @{ Cmd = $cmd; In = $gcRel; Out = $hdr }
}

# psa/vsa steps for pixel/vertex shader ASSEMBLY (.psh/.vsh) a source #includes
# as "<base>.h" (defining g_<base>), when that header is not already present.
# Returns a list of step descriptors.
function Get-ShaderSteps([string]$dir, [string]$blob) {
    $steps = @()
    # (-Include is ignored with -LiteralPath, so filter on the extension here.)
    #   .psh -> psa (pixel shader asm), .vsh/.vsm -> vsa (vertex shader asm/microcode),
    #   .hlsl -> fxc (target vs_3_0/ps_3_0 chosen from the VS/PS name suffix, entry main).
    $exts = @(".psh", ".vsh", ".vsm", ".hlsl")
    $shaders = @(Get-ChildItem -LiteralPath $dir -Recurse -File | Where-Object { $exts -contains $_.Extension.ToLower() })
    # Every "X.h" the sample #includes (quoted, exact -- so "Foo.h" never matches a
    # runtime-compiled "Foo.hlsl"). A generated shader header is a build artifact
    # (gitignored), so we do NOT skip on its presence -- the step is incremental.
    $allIncludes = @([regex]::Matches($blob, '#include\s*"([A-Za-z0-9_]+\.h)"', 'IgnoreCase') |
        ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    # For the assembly default-var fallback: an included header that no shader
    # source is named after and that has no sibling .cpp -- i.e. a generated shader
    # header (e.g. MemExportShader.h). Not Test-Path, which a generated .h pollutes.
    $srcBases = @($shaders | ForEach-Object { [IO.Path]::GetFileNameWithoutExtension($_.Name) })
    $absentIncludes = @($allIncludes | Where-Object {
        ($srcBases -notcontains [IO.Path]::GetFileNameWithoutExtension($_)) -and
        (-not (Test-Path (Join-Path $dir ([IO.Path]::ChangeExtension($_, '.cpp'))))) })
    foreach ($sh in $shaders) {
        $nm = [IO.Path]::GetFileNameWithoutExtension($sh.Name)
        $base = (Resolve-Path -LiteralPath $dir).Path
        $rel = $sh.FullName.Substring($base.Length).TrimStart([IO.Path]::DirectorySeparatorChar).Replace("/", "\")
        # Header + variable name. Convention A (CubicBezierPatch): <base>.h is
        # #included and the sample uses g_<base> -> pass /Vng_<base>. Convention B
        # (MicrocodeMemExport): the header has an unrelated name and the sample
        # uses the assembler's DEFAULT variable (g_xvs_main/g_xps_main) -> map to
        # the single absent shader-header include and omit /Vn.
        # Match the EXACT include "<base>.h" (in $absentIncludes, parsed with the
        # closing quote) -- not a loose substring, or "Foo.h" would false-match a
        # runtime-compiled "Foo.hlsl" reference and emit a bogus fxc step.
        if ($allIncludes -contains "$nm.h") {
            $hdr = [IO.Path]::ChangeExtension($rel, ".h"); $vn = " `"/Vng_$nm`""
        } elseif ($sh.Extension.ToLower() -ne ".hlsl" -and $shaders.Count -eq 1 -and $absentIncludes.Count -eq 1) {
            # Default-variable convention only for ASSEMBLY shaders (a lone .vsm/.psh/
            # .vsh -> the one absent shader header). Never for .hlsl, which is often
            # compiled at runtime (mapping it to an unrelated missing .h is wrong).
            $hdr = (Split-Path $rel -Parent); if ($hdr) { $hdr = "$hdr\" }; $hdr = "$hdr$($absentIncludes[0])"; $vn = ""
        } else { continue }
        switch ($sh.Extension.ToLower()) {
            ".psh"  { $cmd = ('"$(RxdkBinDir)\psa.exe" /nologo "/Fh$(ProjectDir){0}"{1} "$(ProjectDir){2}"' -f $hdr, $vn, $rel) }
            ".hlsl" {
                $tgt = if ($nm -match 'PS$') { "ps_3_0" } else { "vs_3_0" }
                $cmd = ('"$(RxdkBinDir)\fxc.exe" /nologo "/T{0}" /Emain "/Fh$(ProjectDir){1}"{2} "$(ProjectDir){3}"' -f $tgt, $hdr, $vn, $rel)
            }
            default { $cmd = ('"$(RxdkBinDir)\vsa.exe" /nologo "/Fh$(ProjectDir){0}"{1} "$(ProjectDir){2}"' -f $hdr, $vn, $rel) }
        }
        $steps += @{ Cmd = $cmd; In = $rel; Out = $hdr }
    }
    return $steps
}

function New-Vcxproj($name, $guid, $confType, $cpps, $hdrs, $includeDirs, $catLibs, $projRefs, $preBuild) {
    $isApp = ($confType -eq "Application")
    $o = New-Object System.Collections.Generic.List[string]
    $o.Add('<?xml version="1.0" encoding="utf-8"?>')
    $o.Add('<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">')
    $o.Add('  <ItemGroup Label="ProjectConfigurations">')
    foreach ($c in $Configs) {
        $o.Add("    <ProjectConfiguration Include=`"$($c.Name)|$Platform`">")
        $o.Add("      <Configuration>$($c.Name)</Configuration>")
        $o.Add("      <Platform>$Platform</Platform>")
        $o.Add('    </ProjectConfiguration>')
    }
    $o.Add('  </ItemGroup>')
    $o.Add('  <PropertyGroup Label="Globals">')
    $o.Add("    <ProjectGuid>{$guid}</ProjectGuid>")
    $o.Add("    <RootNamespace>$(ConvertTo-Xml $name)</RootNamespace>")
    $o.Add('  </PropertyGroup>')
    $o.Add('  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />')
    # Per-config configuration properties (ConfigurationType, toolset, WPO).
    foreach ($c in $Configs) {
        $cond = "'`$(Configuration)|`$(Platform)'=='$($c.Name)|$Platform'"
        $o.Add("  <PropertyGroup Condition=`"$cond`" Label=`"Configuration`">")
        $o.Add("    <ConfigurationType>$confType</ConfigurationType>")
        $o.Add("    <PlatformToolset>$Toolset</PlatformToolset>")
        # The clang toolset builds stock XDK/ATG headers in always-modern mode.
        $o.Add('    <RxdkModernXdkHeaders>true</RxdkModernXdkHeaders>')
        # Link the ATG Common archive. A ProjectReference builds Common but the
        # clang link (ld.lld) does not auto-pull a referenced static lib, so name
        # its per-config output explicitly. Common is an on-demand archive, so a
        # sample links only the members it uses. (Not for Common itself.)
        if ($confType -eq 'Application') {
            $o.Add('    <RxdkModernLibs>$(MSBuildProjectDirectory)\..\..\Common\$(Configuration)\Common.lib</RxdkModernLibs>')
        }
        if ($c.Wpo) { $o.Add('    <WholeProgramOptimization>true</WholeProgramOptimization>') }
        $o.Add('  </PropertyGroup>')
    }
    $o.Add('  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />')
    foreach ($c in $Configs) {
        $cond = "'`$(Configuration)|`$(Platform)'=='$($c.Name)|$Platform'"
        $o.Add("  <ItemDefinitionGroup Condition=`"$cond`">")
        $o.Add('    <ClCompile>')
        $o.Add("      <PreprocessorDefinitions>$($c.Defs);%(PreprocessorDefinitions)</PreprocessorDefinitions>")
        $o.Add("      <Optimization>$($c.Opt)</Optimization>")
        $o.Add("      <RuntimeLibrary>$($c.Rt)</RuntimeLibrary>")
        if ($includeDirs) {
            $o.Add("      <AdditionalIncludeDirectories>$(ConvertTo-Xml $includeDirs);%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>")
        }
        $o.Add('    </ClCompile>')
        if ($isApp) {
            $deps = @($c.Base)
            foreach ($lib in $catLibs) { $deps += (ConvertTo-Variant $lib $c.CatVariant) }
            $o.Add('    <Link>')
            $o.Add("      <AdditionalDependencies>$(ConvertTo-Xml ($deps -join ';'));%(AdditionalDependencies)</AdditionalDependencies>")
            if ($c.Ignore.Count -gt 0) {
                $o.Add("      <IgnoreSpecificDefaultLibraries>$(ConvertTo-Xml ($c.Ignore -join ';'));%(IgnoreSpecificDefaultLibraries)</IgnoreSpecificDefaultLibraries>")
            }
            $o.Add('    </Link>')
        }
        $o.Add('  </ItemDefinitionGroup>')
    }
    if ($preBuild) {
        # The "Xbox 360" platform does not run PreBuildEvent, so wire each
        # generated-header step (spac for .spa.h, psa/vsa for shader .h) as a real
        # target before compilation. Inputs/Outputs make each incremental.
        $i = 0
        foreach ($step in @($preBuild)) {
            $i++
            $o.Add("  <Target Name=`"RxdkGen$i`" BeforeTargets=`"ClCompile`" Inputs=`"`$(ProjectDir)$(ConvertTo-Xml $step.In)`" Outputs=`"`$(ProjectDir)$(ConvertTo-Xml $step.Out)`">")
            $o.Add("    <Message Importance=`"high`" Text=`"RXDK-360: generating $(ConvertTo-Xml $step.Out)`" />")
            $o.Add("    <Exec Command=`"$(ConvertTo-Xml $step.Cmd)`" />")
            $o.Add('  </Target>')
        }
    }
    if ($cpps.Count -gt 0) {
        $o.Add('  <ItemGroup>')
        foreach ($x in $cpps) { $o.Add("    <ClCompile Include=`"$(ConvertTo-Xml $x)`" />") }
        $o.Add('  </ItemGroup>')
    }
    if ($hdrs.Count -gt 0) {
        $o.Add('  <ItemGroup>')
        foreach ($x in $hdrs) { $o.Add("    <ClInclude Include=`"$(ConvertTo-Xml $x)`" />") }
        $o.Add('  </ItemGroup>')
    }
    if ($projRefs) {
        $o.Add('  <ItemGroup>')
        foreach ($p in $projRefs) {
            $o.Add("    <ProjectReference Include=`"$(ConvertTo-Xml $p.Path)`">")
            $o.Add("      <Project>{$($p.Guid)}</Project>")
            $o.Add('    </ProjectReference>')
        }
        $o.Add('  </ItemGroup>')
    }
    $o.Add('  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />')
    $o.Add('</Project>')
    return ($o -join "`r`n") + "`r`n"
}

function New-Sln($name, $guid, $toCommon, $commonGuid) {
    $tg = "{$VcxprojType}"
    $o = New-Object System.Collections.Generic.List[string]
    $o.Add("")
    $o.Add("Microsoft Visual Studio Solution File, Format Version 12.00")
    $o.Add("# Visual Studio Version 17")
    $o.Add("Project(`"$tg`") = `"$name`", `"$name.vcxproj`", `"{$guid}`"")
    $o.Add("EndProject")
    $o.Add("Project(`"$tg`") = `"$CommonName`", `"$toCommon\$CommonName.vcxproj`", `"{$commonGuid}`"")
    $o.Add("EndProject")
    $o.Add("Global")
    $o.Add("`tGlobalSection(SolutionConfigurationPlatforms) = preSolution")
    foreach ($c in $Configs) { $o.Add("`t`t$($c.Name)|$Platform = $($c.Name)|$Platform") }
    $o.Add("`tEndGlobalSection")
    $o.Add("`tGlobalSection(ProjectConfigurationPlatforms) = postSolution")
    foreach ($g in @($guid, $commonGuid)) {
        foreach ($c in $Configs) {
            $o.Add("`t`t{$g}.$($c.Name)|$Platform.ActiveCfg = $($c.Name)|$Platform")
            $o.Add("`t`t{$g}.$($c.Name)|$Platform.Build.0 = $($c.Name)|$Platform")
        }
    }
    $o.Add("`tEndGlobalSection")
    $o.Add("`tGlobalSection(SolutionProperties) = preSolution")
    $o.Add("`t`tHideSolutionNode = FALSE")
    $o.Add("`tEndGlobalSection")
    $o.Add("EndGlobal")
    return ($o -join "`r`n") + "`r`n"
}

$script:Changed = New-Object System.Collections.Generic.List[string]
# Remove a stale generated file (a sample that is now skipped). In -Check mode a
# lingering file counts as out-of-date.
function Remove-Generated([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return }
    if ($Check) { $script:Changed.Add($path); return }
    Remove-Item -LiteralPath $path -Force
}
function Write-Generated([string]$path, [string]$text) {
    if ($Check) {
        $old = $null
        if (Test-Path -LiteralPath $path) { $old = [IO.File]::ReadAllText($path) }
        if ($old -ne $text) { $script:Changed.Add($path) }
        return
    }
    [IO.File]::WriteAllText($path, $text, $Utf8NoBom)
}

# ---- main ------------------------------------------------------------------

$rootFull = (Resolve-Path -LiteralPath $Root).Path
$commonDir = Join-Path $rootFull $CommonName
if (-not (Test-Path -LiteralPath $commonDir -PathType Container)) {
    Write-Error "no Common\ (ATG framework) under $rootFull"; exit 2
}
$commonGuid = Get-StableGuid $CommonName

# 1) Common ATG static library (all configs, no link set, no pre-build).
$cs = Get-Sources $commonDir
Write-Generated (Join-Path $commonDir "$CommonName.vcxproj") `
    (New-Vcxproj $CommonName $commonGuid "StaticLibrary" $cs.cpps $cs.hdrs '$(ProjectDir)' $null $null $null)

# 2) One project + solution per sample.
$made = 0
foreach ($area in (Get-ChildItem -LiteralPath $rootFull -Directory | Sort-Object Name)) {
    if ($IgnoreTop -contains $area.Name -or $area.Name.StartsWith(".")) { continue }
    foreach ($sample in (Get-ChildItem -LiteralPath $area.FullName -Directory | Sort-Object Name)) {
        $src = Get-Sources $sample.FullName
        if ($src.cpps.Count -eq 0) { continue }
        $blob = Read-Blob $sample.FullName ($src.cpps + $src.hdrs)
        if (Test-HostTool $blob) {              # PC-side tool, not an Xbox title
            Remove-Generated (Join-Path $sample.FullName ("$($sample.Name).vcxproj"))
            Remove-Generated (Join-Path $sample.FullName ("$($sample.Name).sln"))
            continue
        }
        $name = $sample.Name
        $guid = Get-StableGuid ($name + "|" + (Join-Path $area.Name $name))
        $toCommon = Get-RelPath $sample.FullName $commonDir
        $catLibs = Get-CategoryLibs $blob
        $pre = @()
        $spa = Get-SpaStep $sample.FullName $blob
        if ($spa) { $pre += $spa }
        $pre += Get-ShaderSteps $sample.FullName $blob
        $ref = @{ Path = "$toCommon\$CommonName.vcxproj"; Guid = $commonGuid }
        Write-Generated (Join-Path $sample.FullName "$name.vcxproj") `
            (New-Vcxproj $name $guid "Application" $src.cpps $src.hdrs $toCommon $catLibs @($ref) $pre)
        Write-Generated (Join-Path $sample.FullName "$name.sln") `
            (New-Sln $name $guid $toCommon $commonGuid)
        $made++
    }
}

if ($Check) {
    if ($script:Changed.Count -gt 0) {
        Write-Host ("out of date ({0}): run tools\Generate-Projects.ps1" -f $script:Changed.Count)
        $script:Changed | ForEach-Object { Write-Host ("  " + $_.Substring($rootFull.Length).TrimStart('\')) }
        exit 1
    }
    Write-Host "projects up to date"; exit 0
}
Write-Host ("generated {0} sample project(s) + Common library (6 configs)" -f $made)
