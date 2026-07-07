# You Don't Know Jack — Development Progress

Toolchain: **ReXGlue SDK v0.1.0** (the version installed on this machine).
Note: the current 360-recomp house style has moved to ReXGlue v0.8.0
(`rexglue init` / `rexglue codegen` with auto-hint resolution); v0.8.0 wasn't
checked out here, so this project uses the v0.1.0 config-driven flow. The repo
layout and hygiene follow the current house style either way.

## Phase 1: Extraction & Triage (DONE)

- Source: a **retail Xbox 360 disc image** (NTSC-U), a 7.8 GB `.iso` in XGD /
  XDVDFS format.
- `extract_iso.py` → **2,702 files** to `extracted/`: `default.xex` (5,021,696 B),
  plus `talkshow/` (episode media, thousands of `.fsb` sound banks), `videos/`,
  `shaders/`, `textures/`, `flash/` (Scaleform UI), `jacktool/`, avatar packs.
  The disc is huge because the game is a quiz show — almost all of it is audio
  and FMV question content, not code.
- `xex_info.py` triage of `default.xex`:
  - Image base `0x82000000` (standard — runtime handles this class cleanly).
  - Image size `0x650000` (**6.3 MB** of actual code/data).
  - Imports: `xam.xex`, `xboxkrnl.exe` only. **No XNET/Live import wall.**
  - Cute detail: the `ORIGINAL_BASE_ADDRESS` header field reads `0x4A61636B`,
    which is ASCII **"Jack"**.

## Phase 2: Scaffold & Codegen (DONE)

- `rexglue init --app_name ydkj --app_root .` → `CMakeLists.txt`,
  `CMakePresets.json`, `src/main.cpp`, `ydkj_config.toml`.
- Pointed `file_path` at `../extracted/default.xex` and ran `rexglue codegen`:
  - **First pass:** decoded **876,806 instructions** across the code regions;
    `10,289` functions from PDATA; iterative discovery + a 52-vtable / 43-function
    scan + gap-fill → 14,551 functions. Then **one** `UnresolvedCall`:
    `b 0x82236F38 from 0x822387AC — target not in any function` (a tail-call
    branch discovery hadn't sealed into a function).
  - Added a single `[functions]` hint: `0x82236F38 = { name = "ydkj_sub_82236F38" }`
    (entry-only; discovery sizes it).
  - **Second pass: clean** — "all calls resolve", 14,552 functions ready,
    **14,343 recompiled**.
  - Output: **30 `.cpp` files, ~59 MB** in `project/generated/` (git-ignored).
  - Imports: **209 kernel/XAM imports** resolved, 0 unresolved, 13 variables
    skipped.

## Phase 3: Build (DONE — links clean after 14 stubs)

- Configured with Clang 21 / Ninja / lld-link against the ReXGlue SDK v0.1.0
  install (via `REXSDK`). All **30 recomp TUs compiled first try.**
- The link failed on kernel/XAM imports the v0.1.0 runtime doesn't export.
  lld-link caps reported errors at 20, so instead of iterating we computed the
  **full** missing set deterministically: the game references 209 `__imp__`
  kernel symbols; the SDK libs define 440; the diff is exactly **14 missing**:

  | Group | Symbols |
  |-------|---------|
  | Xbox LIVE / net | `XNetLogonGetTitleID`, `NetDll_XNetGetConnectStatus`, `NetDll_XNetQosLookup`, `NetDll_XNetConnect`, `NetDll_WSAGetOverlappedResult` |
  | XAM user/stats/voice | `XamUserGetMembershipTierFromXUID`, `XamUserGetOnlineCountryFromXUID`, `XamUserCreateStatsEnumerator`, `XamVoiceSubmitPacket` |
  | XAM blade UI | `XamShowGamerCardUIForXUID`, `XamShowMarketplaceUI`, `XamShowFriendsUI` |
  | xboxkrnl helpers | `ObReferenceObject`, `ExAllocatePoolWithTag` |

- `src/kernel_stubs.cpp` defines all 14 with the standard `PPC_FUNC_IMPL`
  signature. Every LIVE/voice/stats/marketplace/friends symbol is off the
  single-player path, so they log-once and return 0. `ObReferenceObject` returns
  the object pointer it was handed (so callers don't null-fault);
  `ExAllocatePoolWithTag` returns 0 for now (promote to a real guest-heap alloc
  if a boot trace ever shows it on a hot path).
- **Linked: `ydkj.exe`, 28,230,144 bytes (28 MB).**

## Phase 4: Boot (DONE — crash-free into asset streaming)

First run (`ydkj.exe <extracted>`), logged to a file sink:

- Clean runtime bring-up: SDL input → **XMA Decoder + Audio Worker** threads →
  mounted `extracted/` as the guest disk → **D3D12 GPU backend, NVIDIA RTX 5070**
  adapter, D3D12 device created → GPU Commands + GPU VSync threads →
  `GPU system initialized (presentation=true)` → `Runtime initialized successfully`.
- **Loaded `default.xex`** (`game:\default.xex`), spun up the Kernel Dispatch
  thread, and handed control to guest code.
- Guest execution proceeded and stayed healthy: registered a GPU interrupt
  callback (`SetInterruptCallback`), then streamed `GAME:\shaders` (many reads —
  the game loading its shader set) and `GAME:\flash` (Scaleform UI), with the XMA
  decoder actively consuming audio the whole time.
- Across the entire run: **0 FATALs, 0 unregistered-function crashes.** The only
  warnings were benign — six missing `xam` ordinals (the online functions we
  stubbed), a `default.xexp` patch-file probe (there is none), and a stream of
  `XMA: Write to unknown register (0601)` (audio hardware register the runtime's
  XMA model doesn't model — harmless).
- The run was terminated manually after ~15 s; it was still executing and
  decoding audio at that point, not crashing.

## What's NOT yet verified

- **On-screen rendering.** This was a headless session — no captured frame, and
  a targeted `PrintWindow` grab of the game window returned no handle (the window
  may not present in a non-interactive session). We know the GPU device is up and
  the game is loading shaders/UI, but we have **not** confirmed pixels on screen.
- **Gameplay / input.** Not driven.

## Next up (TODO)

- [ ] Capture the first rendered frame (run in an interactive desktop session;
      confirm the presenter shows the intro / attract screen). This is the single
      highest-value next step — it upgrades the status from "boots" to "renders".
- [ ] Drive input (SDL keyboard/gamepad) through the intro into a round.
- [ ] Audio verification — the `.fsb`/XMA banks are being decoded; confirm output.
- [ ] Watch `ExAllocatePoolWithTag`: if a trace shows real allocations, wire it to
      the guest heap instead of returning 0.
- [ ] Re-run on ReXGlue v0.8.0 when available and compare (the house-standard
      toolchain; may change function counts and the stub set).
