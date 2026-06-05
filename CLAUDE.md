# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A Pebble **watchface** named **Crisp** (`displayName` in `package.json`; the
project directory is `Crisp-watchface`, which is what the built `.pbw` is named
after). It is a customizable analog clock: thin edge markers around the dial,
the current 5-minute sector of minute ticks, an optional date block, an optional
battery gauge drawn onto the hour markers, an optional second hand, and four
optional **corner readouts**. It builds for every Pebble platform — aplite,
basalt, chalk, diorite, emery, flint, gabbro.

The whole dial is laid out **proportionally at runtime** from the display size
(see `Layout` in `src/c/main_window.c`), so it scales from the classic 144×168
screens up to emery (200×228) and the round gabbro (260×260). Emery and the
physically round displays render a *round* dial (`CRISP_ROUND_DIAL`); rectangular
displays let the markers hug the panel edges.

Because this is a watchface that can show a live second hand, it subscribes to
`SECOND_UNIT` only when the second hand is enabled and otherwise to
`MINUTE_UNIT` (`subscribe_ticks`). Do not "optimize" this into a fixed
`app_timer` loop — the tick service is the correct primitive here.

### SDK version (do not "fix" this)

Built with the **RePebble SDK 4.9.169** toolchain (Pebble Tool v5). Per the
current docs, `package.json` `pebble.sdkVersion` stays **`"3"`** — there is no
`"4"`; the toolchain rejects it. API availability follows from the installed
SDK, the build targets and the compile-time `PBL_*` macros, not from this field.
`enableMultiJS` is on (the phone side is split into `index.js` + `config.js`).
Leave `sdkVersion: "3"` as is; bumping it breaks the build.

## Build, run, debug

```bash
pebble build                                    # compiles all targetPlatforms -> build/Crisp-watchface.pbw
pebble clean                                    # wipe build/; REQUIRED after changing messageKeys in package.json
pebble install --emulator emery                 # boot QEMU + install (also: aplite, basalt, chalk, diorite, flint, gabbro)
pebble screenshot --no-open --emulator emery    # capture screenshot for visual verification
pebble logs --emulator emery                    # view APP_LOG / console.log output
pebble install --phone <ip>                     # install on a real watch via the Pebble app
```

A clean build emits no warnings from our code; the only remaining one is the
SDK-level `LOAD segment with RWX permissions` linker note, inherent to every
Pebble app. There is no unit-test harness in this project — verification is
visual, via the emulator and `pebble screenshot`. After any C change, build and
screenshot at least one rectangular (emery) and one round (chalk/gabbro) target,
since the layout math and corner insets differ between them.

## Architecture

Two-process design typical of Pebble apps, split into small single-responsibility
files under `src/c/` (compiled by `wscript`, which globs `src/c/**/*.c`):

- **`src/c/main.c`** — entry point. `comm_init()`, `config_init()`,
  `main_window_push()`, `app_event_loop()`.
- **`src/c/globals.h`** — all shared constants: `LAYOUT_*` proportions, animation
  timings, AppMessage buffer sizes, the `PERSIST_KEY_*` slots, the `CORNER_*`
  content enum, and the `Time` struct. **No magic numbers elsewhere** — add new
  constants here.
- **`src/c/config.{h,c}`** — the live settings store. `config_init()` seeds
  first-launch defaults (guarded by `PERSIST_DEFAULTS_SET`), then reads every
  persisted value into in-RAM arrays. Read it back through the typed getters:
  `config_get(bool key)`, `config_get_color(color index)`, `config_get_corner(pos)`,
  and `config_get_weather_{valid,temp,precip}()`.
- **`src/c/comm.{h,c}`** — AppMessage inbox handler. Routes each received
  `MESSAGE_KEY_*` to its persist slot, then calls `config_init()` +
  `main_window_refresh()`. It distinguishes a **config edit** (buzz once) from a
  **weather refresh** (stay silent), and tolerates Clay sending the corner
  selects as strings (`store_corner` parses with `atoi`).
- **`src/c/main_window.{h,c}`** — the watch face itself. Owns the window, the
  `Layout`, all drawing (`bg_update_proc` for markers/battery, `draw_proc` for
  hands + center cap), the launch sweep animation, the date `TextLayer`s, and the
  four corner `TextLayer`s. Exposes `main_window_push()` and
  `main_window_refresh()` (live re-apply of config without relaunch).
- **`src/pkjs/config.js`** — the **Clay** form definition (a `module.exports`
  array). Corner selects share one `CORNER_OPTIONS` list whose string values map
  to the `CORNER_*` enum on the watch.
- **`src/pkjs/index.js`** — phone-side glue. Initializes Clay (which auto-sends
  the config on save) and relays **weather**: geolocate → Open-Meteo → send
  `WEATHER_TEMP_MIN` / `WEATHER_TEMP_MAX` / `WEATHER_PRECIP` to the watch,
  refreshed on a timer.

### Settings model (persist + AppMessage)

Three kinds of state, all persisted on the watch so the face is fully configured
without the phone:

1. **Booleans** (`PERSIST_KEY_DATE` … `PERSIST_KEY_SECOND_HAND`, slots 0–4) —
   the Display toggles. Stored via `persist_write_bool`, read into `s_arr[]`.
2. **Colors** (`PERSIST_KEY_COLOR_OFFSET + index`, 8 of them) — `0xRRGGBB`
   integers, ready for `GColorFromHEX`. Read into `s_colors[]`. Only meaningful
   on `PBL_COLOR`; B&W platforms fall back to white markers/hands in the draw code.
3. **Corners** (`PERSIST_KEY_CORNER_OFFSET + position`, 4 of them) — one
   `CORNER_*` content kind per corner, read into `s_corners[]`.
4. **Temperature unit** (`PERSIST_KEY_TEMP_UNIT`, int) — `TEMP_UNIT_AUTO` /
   `CELSIUS` / `FAHRENHEIT`, read into `s_temp_unit`. AUTO follows the watch's
   metric/imperial preference via
   `health_service_get_measurement_system_for_display(HealthMetricWalkedDistanceMeters)`
   (Pebble has no dedicated temperature setting), falling back to Celsius when
   unknown or on non-`PBL_HEALTH` platforms.

Plus a cached **weather sample** (`PERSIST_KEY_WEATHER_TEMP_MIN/MAX/PRECIP/VALID`) that
the weather/precip corners read; it survives a relaunch until the next refresh.
Temperatures are always stored in Celsius and converted to Fahrenheit only at
display time (`weather_use_fahrenheit` / `celsius_to_fahrenheit` in
`main_window.c`), so flipping the unit updates instantly without a re-fetch.

**Defaults live in two places that must stay in sync:** `config_init()` in
`src/c/config.c` (the first-launch seed) and the `defaultValue`s in
`src/pkjs/config.js` (the Clay form). The shipped defaults: date + day on,
battery + disconnect indicator on, **second hand off**, temperature unit
Automatic, and corners = battery (TL) / heart rate (TR) / steps (BL) / weather (BR).

### Corner readouts

Each of the four corners (`CORNER_POS_TL/TR/BL/BR`) can show one of:
`CORNER_NONE`, `CORNER_BATTERY`, `CORNER_BLUETOOTH`, `CORNER_HEART_RATE`,
`CORNER_STEPS`, `CORNER_WEATHER` (today's low/high), `CORNER_PRECIP` (current-hour
rain chance). Each is a small right/left-aligned `TextLayer`. `corners_rebuild()`
attaches/detaches the layers per config; `corners_update()` re-formats the shown
values from live data and is called from the tick, battery, connection and
health handlers (and on every config/weather message via `main_window_refresh`).
Box width is a fraction of the screen; the inset is small on rectangular
displays but pushed inward on `PBL_ROUND` so the bezel does not clip the text.

### Conditional compilation

- `#ifdef PBL_COLOR` — colored markers/hands; B&W platforms force white.
- `#if defined(PBL_HEALTH)` — heart-rate / steps corners and the
  `health_service_events_subscribe` handler. Without it those corners show `--`.
- `#ifdef PBL_ROUND` — pushes the corner boxes inward (round bezel clipping).
- `#if defined(PBL_ROUND) || defined(PBL_PLATFORM_EMERY)` → `CRISP_ROUND_DIAL`,
  the round-dial marker layout (markers on a circle, not hugging the edges).

### Weather (Tier 3, phone-relayed)

The watch has no internet; `src/pkjs/index.js` does the work. On `ready` (and
every `WEATHER_REFRESH_MS`) it calls `navigator.geolocation.getCurrentPosition`,
queries the free, key-less **Open-Meteo** API
(`daily=temperature_2m_max,temperature_2m_min`,
`hourly=precipitation_probability`, `timezone=auto`),
rounds today's low/high, picks the precipitation probability for the current hour
(match `current.time`'s `…THH` prefix against `hourly.time`), and sends both
with `Pebble.sendAppMessage`. Geolocation requires the `location` capability in
`package.json`. Temperature is sent in °C. If no sample has arrived yet the
corners show `--°` / `--%`.

### Config message protocol (Clay + named keys)

Config travels as one AppMessage with **named** `messageKeys` declared in
`package.json`. Toggles arrive as 0/1, colors as `0xRRGGBB`, corner selects as
strings, weather as ints. **Changing the key list requires `pebble clean`** (the
`MESSAGE_KEY_*` macros are generated from `package.json`). Current keys: `DATE`,
`DAY`, `BT`, `BATTERY`, `SECOND_HAND`, the eight `*_COLOR` keys,
`CORNER_TL/TR/BL/BR`, `TEMP_UNIT`, and
`WEATHER_TEMP_MIN`/`WEATHER_TEMP_MAX`/`WEATHER_PRECIP`.

AppMessage buffers are fixed sizes (`APP_MESSAGE_INBOX_SIZE` /
`APP_MESSAGE_OUTBOX_SIZE` in `globals.h`), not
`app_message_*_size_maximum()` — the maximum-size request hangs the app on
aplite's small RAM budget.

## Conventions

- **No magic numbers**: C constants are `#define`/`static const` in `globals.h`;
  JS tunables are named constants at the top of their file.
- **No silent defaults / no mocks**: required config is seeded explicitly in
  `config_init()`; missing data shows `--`, it is never faked.
- The code logs liberally with `APP_LOG(APP_LOG_LEVEL_DEBUG, …)` (C) and
  `console.log` (JS) — keep this; the emulator `pebble logs` output is the main
  debugging affordance.
- All files are written in **English** (code, comments, identifiers).
- `resources/` bundles only `images/MenuImage_BW.png` (the launcher menu icon);
  there are no custom fonts — the date and corner text use system Gothic fonts.

## Reference

`API-descr.md` is a tiered cheat-sheet of what a Pebble app can drive (on-watch
vs. needs-phone vs. needs-internet) — useful when deciding where a new feature
has to live.
