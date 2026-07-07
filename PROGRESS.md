# You Don't Know Jack — Development Progress

Toolchain: **ReXGlue SDK v0.8.0** (self-contained; `rexglue init` / `rexglue
codegen`, no XenonRecomp).

## Phase 1: Extraction & Triage (DONE)

- Source: a **retail Xbox 360 disc image** (NTSC-U), a 7.8 GB `.iso` in XGD /
  XDVDFS format.
- `extract_iso.py` → **2,702 files** to `extracted/`: `default.xex` (5,021,696 B),
  plus `talkshow/` (episode media, thousands of `.fsb` sound banks), `videos/`,
  `shaders/`, `textures/`, `flash/` (Scaleform UI), `jacktool/`, avatar packs.
  The disc is huge because the game is a quiz show — almost all of it is audio
  and FMV question content, not code.
- `xex_info.py` triage of `default.xex`:
  - Image base `0x82000000` (standard). Image size `0x650000` (**6.3 MB** code).
  - Imports: `xam.xex`, `xboxkrnl.exe` only. **No XNET/Live import wall.**
  - Title ID `0x54510869` (from the runtime's shader-storage init log) — publisher
    code `0x5451` = `"TQ"` = THQ.
  - Cute detail: the `ORIGINAL_BASE_ADDRESS` header field reads `0x4A61636B`,
    ASCII **"Jack"**.

## Phase 2: Scaffold & Codegen (DONE)

- `rexglue init --project-name ydkj --xex-path extracted/default.xex
  --game-root extracted --project-root project` → manifest, CMake, `src/main.cpp`,
  `src/ydkj_app.h`.
- `rexglue codegen`:
  - **First pass:** one `UnresolvedCall` — `b 0x82236F38 from 0x822387AC`, a
    tail-call branch target outside any discovered function.
  - Added it as an entry-only `[entrypoint.functions]` hint (discovery sizes it).
  - **Second pass: clean.** A few non-fatal "unresolved b target" Write-phase
    notes remain (intra-function boundary heuristics — harmless).
  - Output: **29 `.cpp` files, ~59 MB** in `project/generated/default/`
    (git-ignored), **14,552 functions** at this stage.

## Phase 3: Build (DONE — links with ZERO stubs)

- Configured with Clang 21 / Ninja / lld-link against the ReXGlue SDK v0.8.0
  install (`-DCMAKE_PREFIX_PATH=...`). All recomp TUs compiled first try.
- Checked the link gap **before** building by diffing the game's `__imp__`
  symbols against the SDK libs: the game references **209** kernel/XAM imports;
  the v0.8.0 `rexruntime.lib` defines all of them. **Zero missing** → no
  `stubs.cpp` needed (unusual and nice — most titles need at least the XUsbcam
  bundle; this one imports none of it).
- **Linked: `ydkj.exe`, ~21 MB.**

## Phase 4: Boot & bring-up (DONE — unregistered-function class cleared)

The build is minutes of work; the runtime is the craft. First boot got a long,
**healthy** way in before the expected wall:

- Clean runtime bring-up: **D3D12 device (NVIDIA RTX 5070)** → XMA Decoder +
  Audio Worker threads → mounted `extracted/` as the guest disk → GPU Commands +
  GPU VSync threads → `GPU system initialized (presentation=true)` →
  `Runtime initialized successfully`.
- **Loaded `default.xex`**, spun up Kernel Dispatch, `Initializing shader storage
  for title 54510869`, resolved XAM party/UI ordinals via generated thunks,
  registered the GPU interrupt callback (`SetInterruptCallback`).
- First crash: `[FATAL] Call to invalid or unregistered function at 0x82131FF0`
  — the house-standard bring-up class (a target discovery didn't place in a
  function; v0.8.0's discovery is stricter than v0.1.0's gap-fill).

Cleared the whole class in two moves:

1. **Batch vtable/thunk registration.** `extract_pe.py` decompressed the image
   cleanly (6.6 MB PE — no LZX-variant trouble). `find_missing_vtable_funcs.py`
   scanned it against the generated `ydkj_init.cpp` → **231 missing entries**
   (26 C++ adjustor thunks + 205 function entry points) reachable only through
   vtable / RTTI pointer tables in the data section. Registered all 231 as
   `[entrypoint.functions]` hints. That cleared `0x82131FF0` and its class.
2. **Runtime harvest for computed targets.** A few functions are reached by
   `lis/addi`-computed addresses that pointer scans can't see. Built with
   `-DYDKJ_HARVEST=ON` (a tolerant indirect dispatcher, `src/dispatch_tolerance.cpp`,
   that logs each unique unregistered target instead of fataling). One run
   surfaced exactly **2** (`0x822814A8`, `0x82404568`); registered them too.
   **Total hints: 234.**

Rebuilt with real dispatch (`YDKJ_HARVEST=OFF`): **boots crash-free** into the
render loop, **14,781 functions**.

## Phase 5: Front-end → title screen (REACHED — renders)

Captured directly from the running port's window (`PrintWindow`):

- **THQ publisher splash** renders (`images/thq_splash.png`) — the chrome THQ
  logo on black.
- **Lands on the "YOU DON'T KNOW JACK" title screen** (`images/title_screen.png`)
  — the logo, the smoky background, and **fully-rendered text** including the
  `PRESS ▶ START TO BEGIN` prompt. **Text renders correctly** — no memexport /
  glyph gap.

## What's NOT yet verified

- **Input into a question round.** The title screen is interactive
  (`PRESS START`) but gameplay past it hasn't been driven.
- **Audio output.** XMA is decoding (the audio threads run and consume banks);
  actual output not confirmed.

## Next up (TODO)

- [ ] Drive START → into an episode/round; confirm question rendering, buzz-in,
      and scoring. Text already renders, so this should be close.
- [ ] Verify audio output (the `.fsb`/XMA banks are being decoded).
- [ ] Input mapping (SDL keyboard/gamepad) for the four "player" buttons.
- [ ] Retire the bring-up scaffold (`src/dispatch_tolerance.cpp`) once gameplay
      is confirmed clean — kept for now, off by default.
