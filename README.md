# RXDK-360 Samples

The Xbox 360 XDK sample suite, ported to build on **modern Visual Studio**
(VS2022 / VS "18") through the [RXDK-360](https://github.com/Team-Resurgent/RXDK360)
platform integration. Each sample carries a committed `.sln` + `.vcxproj` on the
`Xbox 360` MSBuild platform (toolset `2010-01`), with the full stock XDK
configuration set — **CodeAnalysis, Debug, Profile, Profile_FastCap, Release,
Release_LTCG** — so you can clone, open, and build without any project conversion.
The shared ATG framework lives once in [`Common/`](Common) as a static library
every sample references.

> These are the stock Microsoft XDK samples. You need the RXDK-360 SDK (the XDK
> compiler/headers/libs and the VS platform integration) installed to build them.

Everything here is PowerShell + MSBuild — **no other dependencies**. A freshly
cloned repo needs only Windows, and Visual Studio + RXDK-360 to build.

## Get the assets

The binary runtime assets (~800 MB of textures/scenes/audio/video) are committed
as split parts under [`assets/`](assets) to avoid Git LFS. After cloning, expand
them in place once:

```powershell
pwsh tools\Manage-Assets.ps1 unpack
```

(Windows PowerShell 5.1 also works: `powershell -File tools\Manage-Assets.ps1 unpack`.)

## Build

1. Install RXDK-360 (provides the `Xbox 360` VS platform + the XDK toolchain).
2. Open a sample's `.sln` in VS2022/VS18, or from a command line (any of the six
   configurations):

   ```
   msbuild Graphics\ArrayTexture\ArrayTexture.sln /p:Configuration=Release /p:Platform="Xbox 360"
   ```

   Build every sample with `tools\Build-All.ps1` (`-Configs "Debug,Release"` or
   `-Configs All`); it writes a pass/fail `build-report.txt`.

The build produces a `.xex` under the sample's `<Config>\` folder. (Building also
runs the XDK Deploy step, which reports `X1001 Could not connect` when no console
is set — the `.xex` is still produced; that error is deploy, not build.)

## Project files are generated

`tools\Generate-Projects.ps1` writes every `.vcxproj`/`.sln` and the `Common`
project from the source tree. Re-run it after adding or renaming sources:

```powershell
pwsh tools\Generate-Projects.ps1           # regenerate in place
pwsh tools\Generate-Projects.ps1 -Check    # CI: fail if committed projects are stale
```

Re-pack the assets after adding or changing any (`tools\Manage-Assets.ps1 pack`).

## Why these settings (hard-won)

The generator bakes in the exact settings that make the stock XDK/ATG code build
under the modern platform:

- **Platform is literally `Xbox 360`** — the installed MSBuild platform folder
  name. (Not `RXDK-360`; that name does not resolve.)
- **ANSI / MultiByte.** ATG calls `OutputDebugString("...")` etc.; forcing Unicode
  gives `const CHAR*` → `LPCWSTR` errors. The platform already defaults to
  MultiByte, so projects simply don't override `CharacterSet`.
- **`_XBOX` is defined explicitly.** The platform's `Title.props` defines it only
  for Application/DLL, so a `StaticLibrary` config (Common) misses it — and
  without `_XBOX`, ATG's `stdafx.h` skips `<xtl.h>` and `xnamath.h` fails to
  compile (`CONST`/`INT`/`HALF` undefined).
- **Explicit link set, per config.** The platform's `Core.props` lists the XDK
  title libraries but is a legacy property sheet that isn't imported, so each app
  names the libraries it links — with the right **per-config variant**: Debug/
  CodeAnalysis use the `d`-suffixed libs, Profile the instrumented `i` libs
  (`xapilibi`, `d3d9i`, …), Release_LTCG the `ltcg` libs. On top of the base title
  set, per-sample **category libraries** are detected from the source (e.g. `xhv2`
  for headset, `nuiapi`/`nuihandles`/`st` for NUI/Kinect, `xonline` for Live,
  `xuirun`/`xuirender` for XUI, `xinput2` for XInput2).
- **SPA pre-build.** Samples carrying a `.gameconfig` get a `spac.exe` pre-build
  step that generates their `<name>.spa.h` (the header they `#include`).

## Known non-building samples

A small minority still need more of the XDK **content pipeline** at build time:

- XUI samples that compile `.xui`/scene binaries offline (XuiTool).
- Any sample needing an offline-compiled shader or other generated resource.

## License

Tooling in this repo is GPL-3.0-or-later. The sample sources and assets are
Microsoft XDK content, redistributed here for use with RXDK-360.
