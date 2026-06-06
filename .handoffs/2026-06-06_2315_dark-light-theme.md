# Session Handoff: Dark/Light theme with per-theme palettes (Crisp 1.2.0)
**Date**: 2026-06-06 23:15
**Branch**: main
**Working Directory**: /Users/michalblaha/Documents/Dev/Dev Projects/pebble/Crisp-watchface

## What Was Done
- Renamed the appstore "Built for" line from platform codenames to product
  names (Pebble Classic/Steel/Time/Time Steel/Time Round/2/2 Duo/Time 2/Round 2).
  Confirmed against the official RePebble hardware page.
- Implemented a **Dark/Light theme** for the dial (Crisp 1.2.0):
  - New `THEME` setting (`PERSIST_KEY_THEME` slot 7, `THEME_DARK`/`THEME_LIGHT`).
  - Structural colors now go through `theme_bg()` / `theme_fg()` /
    `theme_battery_empty()` in `main_window.c` (was hardcoded black/white): panel
    background, date + corner text, B&W marker/hand fallback, spent battery
    segment, hollow disconnect cap. Live re-apply in `main_window_refresh()`.
  - **Per-theme accent palettes** remembered on the phone (Clay custom function
    `src/pkjs/custom-clay.js`): switching the Theme select stashes the old
    theme's pickers and loads the new theme's saved palette; submit hands the
    watch the matching palette. Watch only ever holds the active palette.
  - **Reset colors** button (Clay `button` id `resetColors`) -> active theme's
    default palette.
  - App version shown under Save on the config page (`Crisp v1.2.0`).
  - Diagnostic log in `index.js` on `webviewclosed` (prints THEME + a few colors
    to `pebble logs`).
- Bumped version 1.1.0 -> 1.2.0; wrote CHANGELOG, appstore RELEASE_NOTES, added
  theme to DESCRIPTION; updated CLAUDE.md docs.
- Committed everything as `6b832d7`.

## Current State
- Clean tree, all work committed on `main` (`6b832d7`).
- `pebble build` is clean across all 7 platforms (no warnings from our code,
  only the SDK RWX linker note). JS bundles config.js + custom-clay.js + index.js.
- **Verified working on a real watch** (installed via `pebble install --phone
  192.168.1.126`): user confirmed theme switch and Reset now change the colors.
- Emulator (QEMU) was unstable this session (ConnectionResetError / hangs), so
  no emulator screenshots were taken; user supplied real-device screenshots
  (`screenshots/emery-black.jpeg`, `emery-light.jpeg`).

### Git State
- **Modified**: none (clean)
- **Untracked**: none
- **Recent commits**:
  - 6b832d7 Crisp 1.2.0: dark/light theme with per-theme palettes
  - 5337466 Crisp watchface: configurable corners, weather, units, second hand toggle

## What Remains
- [ ] Decide whether to keep or remove the diagnostic `webviewclosed` log in
      `src/pkjs/index.js` (user was asked; left in for now).
- [ ] Optional: emulator screenshots of dark/light on emery + chalk/gabbro once
      QEMU is healthy (CLAUDE.md asks for rect + round visual verification).
- [ ] Optional: publish 1.2.0 to the appstore using `appstore/RELEASE_NOTES.md`.

## Key Decisions Made
- **Per-theme palette memory lives on the phone (Clay localStorage), not the
  watch.** Rationale: a theme can only be changed from the config page anyway, so
  the watch only needs the active palette; keeps C minimal.
- **Light accent defaults** (`000000`/`555555`/`00aa00`/`555555`/`000000`/
  `ff0000`/`ff5500`/`0055aa`) live only in `custom-clay.js`; dark defaults mirror
  `config.c` + `config.js`. Yellow/bright-green wash out on white, hence sturdier
  hues.
- Committed directly to `main` (solo repo, linear history, user asked to commit).

## Gotchas / Notes
- **Clay custom function is injected via `.toString()`** -> it must be fully
  self-contained. All constants/helpers live INSIDE the exported function in
  `custom-clay.js`. (First bug: constants at module scope were `undefined` in the
  webview and killed `AFTER_BUILD`.)
- **localStorage in the config webview can throw** -> `custom-clay.js` wraps it
  in `readStore`/`writeStore` (try/catch + in-memory fallback) and registers all
  handlers BEFORE the initial palette apply, so nothing can abort the swap/reset.
  This was the actual fix that made it work on-device.
- Clay color manipulator: `get()` returns an **integer**, `set()` accepts hex
  string or int; `custom-clay.js` normalizes to hex strings (`toHex`).
- Clay event is `AFTER_BUILD` (not AFTER_RENDER); select emits `change`; button
  emits `click`; look items up with `getItemByMessageKey` / `getItemById`.
- Adding/removing `messageKeys` in `package.json` requires `pebble clean`
  (already done when `THEME` was added).
- Defaults must stay in sync across `config.c` (first-launch seed) and
  `config.js` (Clay defaults); the light palette additionally in `custom-clay.js`.
- QEMU emulator was flaky this session; if it hangs, `pkill -9 -f qemu-pebble`
  and retry. `timeout` is unavailable on macOS (use `gtimeout` or background+Monitor).

## Key Files
- `src/c/main_window.c` - `theme_bg()/theme_fg()/theme_battery_empty()`, the
  draw procs, and the live re-apply in `main_window_refresh()`.
- `src/c/globals.h` - `PERSIST_KEY_THEME`, `THEME_DARK`/`THEME_LIGHT`.
- `src/c/config.c` / `config.h` - theme seed + `config_get_theme()`.
- `src/c/comm.c` - routes `MESSAGE_KEY_THEME`.
- `src/pkjs/custom-clay.js` - per-theme palette memory, swap, reset (the tricky
  part; self-contained, defensive localStorage).
- `src/pkjs/config.js` - `THEME` select, Reset button, version text.
- `src/pkjs/index.js` - `new Clay(clayConfig, customClay)` + diagnostic log.
- `CHANGELOG.md` / `appstore/RELEASE_NOTES.md` / `appstore/DESCRIPTION.md` - 1.2.0 copy.
- `CLAUDE.md` - architecture/settings docs (Theme section).
