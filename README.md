# You Don't Know Jack — native PC recompilation

**Jellyvision's smart-aleck quiz show, statically recompiled from Xbox 360
PowerPC to a native x86-64 executable.** No emulator, no interpreter, no JIT —
the original disc code is translated to C++ with the
[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) and linked against its
runtime. It builds clean, links, and **boots crash-free** into the game's own
startup on a real D3D12 device.

## Status: **boots** 🎩

Extraction → triage → codegen → build → boot, all in one sitting. The whole
5 MB game executable — **14,343 recompiled functions** — compiles, links into a
28 MB `ydkj.exe`, and runs: the ReXGlue runtime comes up on the GPU, loads the
recompiled XEX, and the guest code executes crash-free, registering its GPU
interrupt handler and streaming the game's shaders, Scaleform UI, and XMA audio.
No FATALs, no unregistered-function walls anywhere in the boot.

> **Honest scope:** on-screen rendering and gameplay were **not** verified in
> this pass — it was driven headlessly, so there's no captured frame yet. What's
> proven is that the recompiled code *runs*: runtime up, XEX loaded, guest
> execution streaming assets and audio without a single crash. Grabbing the
> first frame and driving input is the next step — see [PROGRESS.md](PROGRESS.md).

## How it got here

The whole pipeline — from a retail disc image to a running `.exe` — in one pass:

```
XGD disc image ──▶ XDVDFS extract ──▶ XEX triage ──▶ rexglue codegen
  (7.8 GB ISO)      (2,702 files)    (base 0x82000000)  (PowerPC → C++, 14,343 fns)
      │
      └──▶ clang/lld build ──▶ + 14 kernel stubs ──▶ ydkj.exe (28 MB)
           ──▶ boot ──▶ D3D12 up ──▶ 📼 XEX loaded ──▶ 🎩 guest running (shaders + UI + audio)
```

Codegen and build were minutes of work. The interesting parts:

- **One codegen hint.** The first analysis pass decoded **876,806 instructions**
  and discovered 14,551 functions, then surfaced a single `UnresolvedCall` —
  a tail-call to `0x82236F38` that discovery hadn't placed in any function.
  Registered it as one `[functions]` entry; the second pass came back clean:
  **14,343 functions recompiled** across 30 translation units.
- **The link gap, closed deterministically.** All 30 TUs compiled first try; the
  link failed on kernel/XAM imports the runtime doesn't export. Rather than
  play whack-a-mole with lld's 20-error cap, we diffed the game's 209 imported
  `__imp__` symbols against the 440 the SDK libs actually define — **14 missing**,
  all of them Xbox LIVE / voice / stats / marketplace / friends-UI, plus two
  `Ob`/`Ex` helpers. One `kernel_stubs.cpp` fills them (single-player never needs
  the online ones). **Linked.**
- **A clean boot.** First run: SDL input, XMA decoder + audio-worker threads, a
  **D3D12 device on the GPU**, GPU command + vsync threads, `Runtime initialized
  successfully`, XEX loaded, and then *the game's own code taking over* — setting
  its GPU interrupt callback and streaming `shaders/` and the Scaleform `flash/`
  UI while XMA decodes audio. It ran until we stopped it. **Zero** FATALs.

## Binary facts

| | |
|---|---|
| Title | You Don't Know Jack (2011) — Jellyvision, Xbox 360 retail disc (NTSC-U) |
| Format | XGD disc image (XDVDFS) — 7.8 GB ISO, 2,702 files extracted |
| Image base | `0x82000000` (standard) |
| Image size | `0x650000` (6.3 MB code image — the disc bulk is FMV + audio/question banks) |
| Imports | `xam.xex`, `xboxkrnl.exe` only — **no XNET/Live import wall** |
| Fun fact | the XEX's `ORIGINAL_BASE_ADDRESS` field reads `0x4A61636B` = ASCII **"Jack"** |
| Recompiled functions | **14,343** (from 876,806 decoded instructions) |
| Executable | `ydkj.exe`, 28 MB |
| Toolchain | ReXGlue **v0.1.0**, Clang 21, Ninja, lld-link |

## Build & run

You **bring your own** copy of the game — the disc image, extracted assets, and
recompiled C++ are all git-ignored. This repo tracks the project, not the game.
Prereqs: Clang 20+, CMake 3.25+, Ninja, and a built ReXGlue SDK, pointed at by
the `REXSDK` environment variable.

```bash
# 1. Extract your retail disc image with the 360tools kit
python /path/to/360tools/tools/extract_iso.py "You Dont Know Jack.iso" extracted/

# 2. Regenerate the recompiled C++ (git-ignored, ~59 MB)
export REXSDK=/path/to/rexglue-sdk/out/install/win-amd64
rexglue codegen project/ydkj_config.toml     # re-applies the one [functions] hint

# 3. Build
cd project && cmake --preset win-amd64-release
cmake --build out/build/win-amd64-release

# 4. Run  (positional arg = the extracted game directory, mounted as game:\)
./out/build/win-amd64-release/ydkj.exe ../../extracted
```

## Layout

```
project/
  ydkj_config.toml       # codegen config + the one function-entry hint
  CMakeLists.txt         # sources (incl. kernel_stubs.cpp) + SDK link
  CMakePresets.json      # clang/ninja presets
  src/main.cpp           # ReXGlue app entry (window, runtime, LaunchModule)
  src/kernel_stubs.cpp   # the 14 missing kernel/XAM imports — the link work
  generated/             # codegen output — git-ignored, regenerable
extracted/               # game data — bring your own (git-ignored)
```

## Credits

Built on the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) (recompiler +
runtime, D3D12/Vulkan backends derived from
[Xenia](https://github.com/xenia-project/xenia)) and the
[360tools](https://github.com/sp00nznet/360tools) toolkit. You Don't Know Jack
and all game assets are © Jellyvision / the respective rights holders — this
project contains **none** of them. Every game recompiled is a game preserved.

## License

This project's own code (the app entry, kernel stubs, config, and scripts) is
[MIT](LICENSE). It does **not** cover the game (bring your own) or the ReXGlue
SDK and its dependencies, which carry their own licenses.
