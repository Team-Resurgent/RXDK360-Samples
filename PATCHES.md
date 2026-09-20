# Divergences from the stock XDK samples

These are the only edits to the stock Microsoft XDK sample sources, made so the
suite builds under the modern **clang/LLVM** toolchain (`PlatformToolset=clang`,
`RxdkModernXdkHeaders=true`). Each is guarded so a stock `cl.exe` build is
unaffected, and each notes *why* clang needs it. Everything else is stock.

Most clang/MSVC differences are handled at the **toolset** level (compat flags in
`Rxdk.Xbox360.Modern.Build`'s `XdkHeaderFlags()` — `-fms-extensions`,
`-fdelayed-template-parsing`, `-Wno-invalid-token-paste`, the `_WIN32`/`_XBOX`
gates, `_XM_NO_INTRINSICS_`, …), which touch **no source**. The list below is
only what genuinely cannot be fixed in the toolchain.

## Source edits

- **`Common/AtgUtil.cpp` — `SetThreadName` (MS SEH).**
  Guarded the `__try`/`__except` block with `#if defined(__clang__)`. clang does
  not implement Microsoft Structured Exception Handling on PowerPC (a hard
  compiler-capability gap, not a missing flag). The block is only the Visual
  Studio debugger thread-naming trick (`RaiseException(0x406D1388)`), which does
  nothing unless a VS debugger is attached, so under clang the function is a
  no-op. Standard C++ `try`/`catch`/`throw` are fully supported and unaffected.

- **`Common/AtgUtil.cpp` — `DebugSpewV` `const va_list`.**
  Dropped the `const` on the `va_list` parameter. Under the clang/PPC-ELF ABI
  `va_list` is an array type, so a top-level `const` on the parameter is
  non-portable (it can't bind to the CRT's `va_list` parameters). The `const`
  carried no meaning.

## Notes
- The scalar `xnamath` path (`_XM_NO_INTRINSICS_`) is a toolset flag, not a source
  edit: clang has AltiVec but not the MS `__vector4`/VMX128 intrinsic spellings,
  so vector math runs scalar (correct, lower performance). Not tracked here as a
  divergence because no source changed.
