<#
.SYNOPSIS
    Generate modern MSBuild projects (.vcxproj + .sln) for the Xbox 360 XDK samples.

.DESCRIPTION
    The stock XDK samples ship as source + media only; this writes, for each
    sample, a project on the installed "Xbox 360" VS platform (toolset 2010-01)
    so they open and build in VS2022 / VS "18" via the RXDK-360 integration. The
    ATG framework under Common\ is emitted once as a static library every sample
    references.

    Layout expected (and produced) at -Root:
        <Root>\Common\<AtgXxx.cpp...>      + Common.vcxproj
        <Root>\<Area>\<Sample>\<src...>    + <Sample>.vcxproj + <Sample>.sln

    Hard-won settings (see README "Why these settings"): the platform is literally
    named "Xbox 360"; ATG is ANSI so CharacterSet stays the platform default
    (MultiByte); StaticLibrary configs miss the platform's _XBOX define (which the
    ATG stdafx.h / xnamath.h require) so it is set explicitly; and the platform's
    default XDK link set is a property sheet that is not imported, so each app
    lists the libraries it needs (a base set + per-sample category libraries).

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

$Platform     = "Xbox 360"
$Config       = "Release"
$Toolset      = "2010-01"
$VcxprojType  = "8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942"
$CommonName   = "Common"
$CompileExt   = @(".cpp", ".cxx", ".cc", ".c")
$HeaderExt    = @(".h", ".hpp", ".inl")
$IgnoreTop    = @(".git", ".github", "tools", "Common", "assets")

# Base XDK "title" link set (Release libs) - what a plain graphics/audio/system
# sample needs. Category libraries are added on demand from LibTriggers.
$BaseLibs = @(
    "xapilib.lib", "xboxkrnl.lib", "d3d9.lib", "d3dx9.lib", "xgraphics.lib",
    "xnet.lib", "xaudio2.lib", "xact3.lib", "x3daudio.lib", "xmcore.lib",
    "xbdm.lib", "vcomp.lib"
)

# Extra libraries keyed by a signal in the sample's source (a dedicated XDK
# header, an ATG wrapper header, or an unmistakable API prefix). Import libs pull
# nothing when unreferenced, so over-inclusion is cheap; a missing *referenced*
# lib is a hard link error, so err toward adding.
$LibTriggers = @(
    @{ t = @("xhv2.h", "XHV2");                                          libs = @("xhv2.lib") },
    @{ t = @("nuiapi.h", "AtgNui", "NuiImageStream", "NuiSkeleton", "NuiInitialize", "NUI_"); libs = @("nuiapi.lib", "nuihandles.lib", "st.lib") },
    @{ t = @("nuispeech.h", "NuiSpeech");                                libs = @("nuispeech.lib") },
    @{ t = @("nuifitness", "NuiFitness");                                libs = @("nuifitnessapi.lib") },
    @{ t = @("xonline.h", "XOnline");                                    libs = @("xonline.lib") },
    @{ t = @("xparty.h", "XParty");                                      libs = @("xparty.lib") },
    @{ t = @("xavatar", "XAvatar");                                      libs = @("xavatar2.lib") },
    @{ t = @("xmic.h", "XMic");                                          libs = @("xmic.lib") },
    @{ t = @("xui.h", "xuiapp.h", "XuiInit", "XuiRender", "XuiElement", "XuiDrawText"); libs = @("xuirun.lib", "xuirender.lib") },
    @{ t = @("xuihtml.h", "XuiHtml");                                    libs = @("xuihtml.lib") },
    @{ t = @("xuivideo.h", "XuiVideo");                                  libs = @("xuivideo.lib") },
    @{ t = @("xav.h", "XAVCreate", "XAV_");                              libs = @("xav.lib") },
    @{ t = @("xime.h", "XimeXui", "XIME");                               libs = @("xime.lib") },
    @{ t = @("xhttp.h", "XHttp");                                        libs = @("xhttp.lib") },
    @{ t = @("xauth.h", "XAuth");                                        libs = @("xauth.lib") },
    @{ t = @("xmp.h", "XMPGet", "XMP_");                                 libs = @("xmp.lib") },
    @{ t = @("xffb.h", "XFFB");                                          libs = @("xffb.lib") },
    @{ t = @("xcam.h", "XCamera", "XCAMERA");                            libs = @("xcam.lib") },
    @{ t = @("xjson.h", "XJSON");                                        libs = @("xjson.lib") },
    @{ t = @("xinput2.h");                                               libs = @("xinput2.lib") },
    @{ t = @("xmedia2.h", "xmedia.h");                                   libs = @("xmedia2.lib") },
    @{ t = @("xwmadecode.h", "XWMA");                                    libs = @("xwmadecode.lib") },
    @{ t = @("dxerr9.h", "DXGetErrorString9");                          libs = @("dxerr9.lib") }
)

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

# Relative path FROM directory to file-or-dir, backslash-separated (5.1 has no
# [IO.Path]::GetRelativePath, so use Uri).
function Get-RelPath([string]$fromDir, [string]$to) {
    $f = (Resolve-Path -LiteralPath $fromDir).Path
    if (-not $f.EndsWith([IO.Path]::DirectorySeparatorChar)) { $f += [IO.Path]::DirectorySeparatorChar }
    $fromUri = New-Object System.Uri($f)
    $toUri = New-Object System.Uri((Resolve-Path -LiteralPath $to).Path)
    $rel = [System.Uri]::UnescapeDataString($fromUri.MakeRelativeUri($toUri).ToString())
    return $rel.Replace("/", "\")
}

function Get-Sources([string]$dir) {
    $cpps = New-Object System.Collections.Generic.List[string]
    $hdrs = New-Object System.Collections.Generic.List[string]
    $base = (Resolve-Path -LiteralPath $dir).Path
    foreach ($f in Get-ChildItem -LiteralPath $dir -Recurse -File) {
        $ext = $f.Extension.ToLowerInvariant()
        # MSBuild Include paths are always backslash-separated, whatever the OS.
        $rel = $f.FullName.Substring($base.Length).TrimStart([IO.Path]::DirectorySeparatorChar).Replace("/", "\")
        if ($CompileExt -contains $ext) { $cpps.Add($rel) }
        elseif ($HeaderExt -contains $ext) { $hdrs.Add($rel) }
    }
    $cmp = [System.StringComparer]::OrdinalIgnoreCase
    $ca = $cpps.ToArray(); [Array]::Sort($ca, $cmp)
    $ha = $hdrs.ToArray(); [Array]::Sort($ha, $cmp)
    return @{ cpps = $ca; hdrs = $ha }
}

function Get-DetectedLibs([string]$dir, [string[]]$cpps, [string[]]$hdrs) {
    $sb = New-Object System.Text.StringBuilder
    foreach ($r in ($cpps + $hdrs)) {
        try { [void]$sb.AppendLine([IO.File]::ReadAllText((Join-Path $dir $r))) } catch { }
    }
    $blob = $sb.ToString()
    $libs = New-Object System.Collections.Generic.List[string]
    $BaseLibs | ForEach-Object { $libs.Add($_) }
    foreach ($rule in $LibTriggers) {
        $hit = $false
        foreach ($tok in $rule.t) { if ($blob.Contains($tok)) { $hit = $true; break } }
        if ($hit) { foreach ($lib in $rule.libs) { if (-not $libs.Contains($lib)) { $libs.Add($lib) } } }
    }
    return $libs.ToArray()
}

function New-Vcxproj($name, $guid, $confType, $cpps, $hdrs, $includeDirs, $linkLibs, $projRefs) {
    $o = New-Object System.Collections.Generic.List[string]
    $o.Add('<?xml version="1.0" encoding="utf-8"?>')
    $o.Add('<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">')
    $o.Add('  <ItemGroup Label="ProjectConfigurations">')
    $o.Add("    <ProjectConfiguration Include=`"$Config|$Platform`">")
    $o.Add("      <Configuration>$Config</Configuration>")
    $o.Add("      <Platform>$Platform</Platform>")
    $o.Add('    </ProjectConfiguration>')
    $o.Add('  </ItemGroup>')
    $o.Add('  <PropertyGroup Label="Globals">')
    $o.Add("    <ProjectGuid>{$guid}</ProjectGuid>")
    $o.Add("    <RootNamespace>$(ConvertTo-Xml $name)</RootNamespace>")
    $o.Add('  </PropertyGroup>')
    $o.Add('  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />')
    $o.Add('  <PropertyGroup Label="Configuration">')
    $o.Add("    <ConfigurationType>$confType</ConfigurationType>")
    $o.Add("    <PlatformToolset>$Toolset</PlatformToolset>")
    $o.Add('  </PropertyGroup>')
    $o.Add('  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />')
    $o.Add('  <ItemDefinitionGroup>')
    $o.Add('    <ClCompile>')
    $o.Add('      <PreprocessorDefinitions>_XBOX;%(PreprocessorDefinitions)</PreprocessorDefinitions>')
    if ($includeDirs) {
        $o.Add("      <AdditionalIncludeDirectories>$(ConvertTo-Xml $includeDirs);%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>")
    }
    $o.Add('    </ClCompile>')
    if ($linkLibs) {
        $o.Add('    <Link>')
        $o.Add("      <AdditionalDependencies>$(ConvertTo-Xml ($linkLibs -join ';'));%(AdditionalDependencies)</AdditionalDependencies>")
        $o.Add('    </Link>')
    }
    $o.Add('  </ItemDefinitionGroup>')
    if ($cpps.Count -gt 0) {
        $o.Add('  <ItemGroup>')
        foreach ($c in $cpps) { $o.Add("    <ClCompile Include=`"$(ConvertTo-Xml $c)`" />") }
        $o.Add('  </ItemGroup>')
    }
    if ($hdrs.Count -gt 0) {
        $o.Add('  <ItemGroup>')
        foreach ($h in $hdrs) { $o.Add("    <ClInclude Include=`"$(ConvertTo-Xml $h)`" />") }
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
    $o.Add("`t`t$Config|$Platform = $Config|$Platform")
    $o.Add("`tEndGlobalSection")
    $o.Add("`tGlobalSection(ProjectConfigurationPlatforms) = postSolution")
    foreach ($g in @($guid, $commonGuid)) {
        $o.Add("`t`t{$g}.$Config|$Platform.ActiveCfg = $Config|$Platform")
        $o.Add("`t`t{$g}.$Config|$Platform.Build.0 = $Config|$Platform")
    }
    $o.Add("`tEndGlobalSection")
    $o.Add("`tGlobalSection(SolutionProperties) = preSolution")
    $o.Add("`t`tHideSolutionNode = FALSE")
    $o.Add("`tEndGlobalSection")
    $o.Add("EndGlobal")
    return ($o -join "`r`n") + "`r`n"
}

$script:Changed = New-Object System.Collections.Generic.List[string]
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

# 1) Common ATG static library.
$cs = Get-Sources $commonDir
Write-Generated (Join-Path $commonDir "$CommonName.vcxproj") `
    (New-Vcxproj $CommonName $commonGuid "StaticLibrary" $cs.cpps $cs.hdrs '$(ProjectDir)' $null $null)

# 2) One project + solution per sample (Root\<Area>\<Sample> with a source).
$made = 0
foreach ($area in (Get-ChildItem -LiteralPath $rootFull -Directory | Sort-Object Name)) {
    if ($IgnoreTop -contains $area.Name -or $area.Name.StartsWith(".")) { continue }
    foreach ($sample in (Get-ChildItem -LiteralPath $area.FullName -Directory | Sort-Object Name)) {
        $src = Get-Sources $sample.FullName
        if ($src.cpps.Count -eq 0) { continue }   # pure-C# tool or asset-only dir
        $name = $sample.Name
        $guid = Get-StableGuid ($name + "|" + (Join-Path $area.Name $name))
        $toCommon = Get-RelPath $sample.FullName $commonDir
        $libs = Get-DetectedLibs $sample.FullName $src.cpps $src.hdrs
        $ref = @{ Path = "$toCommon\$CommonName.vcxproj"; Guid = $commonGuid }
        Write-Generated (Join-Path $sample.FullName "$name.vcxproj") `
            (New-Vcxproj $name $guid "Application" $src.cpps $src.hdrs $toCommon $libs @($ref))
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
Write-Host ("generated {0} sample project(s) + Common library" -f $made)
