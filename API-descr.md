# Pebble SDK — what an app can control on the watch

Overview of the watch capabilities a Pebble app can drive through the SDK,
grouped by what they depend on. Based on the **RePebble SDK 4.9.169** toolchain
used by this project (all watch-side C APIs live in a single `pebble.h`, with an
optional phone-side **PebbleKit JS** half).

A Pebble app has up to two halves:

- **Watch side** — C compiled to the watch (`src/c/`, `#include <pebble.h>`).
  Optionally a background **worker** (`pebble_worker.h`). Runs standalone.
- **Phone side** — **PebbleKit JS** (`src/pkjs/`), JavaScript that runs inside
  the Pebble mobile app on a paired phone. It is the app's only door to the
  phone and, through it, to the internet.

The two halves talk over Bluetooth with **AppMessage** (key/value dictionaries).
So the rule of thumb for the tiers below is simply: *how far does the data have
to travel?*

- **Tier 1** — stays on the watch.
- **Tier 2** — needs the paired phone (Bluetooth + the Pebble mobile app).
- **Tier 3** — needs the phone *and* a network/online service.

---

## 1. On the watch alone (no phone required)

Everything here works with the phone disconnected — the watch is a standalone
computer with a screen, buttons, sensors and storage.

### Display & UI
| Capability | SDK (C) |
|---|---|
| Windows, window stack, click/back handling | `Window`, `window_stack_*` |
| Text, images, menus, lists, action bar, status bar | `TextLayer`, `BitmapLayer`, `MenuLayer`, `ScrollLayer`, `ActionBarLayer`, `StatusBarLayer` |
| Custom drawing: lines, rects, circles, paths, bitmaps, text | `Layer` update procs, `graphics_*`, `GPath`, `GContext` |
| Fonts (system + bundled custom) | `fonts_get_system_font`, `fonts_load_custom_font` |
| Animations & transitions | `Animation`, `PropertyAnimation`, `animation_*` |
| Color / B&W handling per platform | `GColor`, `PBL_COLOR` / `PBL_BW`, `PBL_ROUND` |
| Timeline Quick View / obstruction layout | `unobstructed_area_service_subscribe` |
| Built-in number / text-pick windows | `NumberWindow`, `MenuLayer` |

### Input
| Capability | SDK (C) |
|---|---|
| Buttons: single / long / multi / repeating / raw click | `window_*_click_subscribe`, `ClickConfigProvider` |
| Touch (round/EM hardware with touch) | `touch_service_subscribe` |

### Feedback / actuators
| Capability | SDK (C) |
|---|---|
| Vibration: short / long / double / custom patterns | `vibes_short_pulse`, `vibes_long_pulse`, `vibes_enqueue_custom_pattern` |
| Backlight control | `light_enable`, `light_enable_interaction` |

### Time
| Capability | SDK (C) |
|---|---|
| Read current time, format dates | `time()`, `localtime`, `strftime`, `clock_to_timestamp` |
| Per-second / per-minute tick callbacks | `tick_timer_service_subscribe` |
| 12h/24h user preference | `clock_is_24h_style` |
| One-shot / app timers | `app_timer_register` |

### Motion & environment sensors (raw data lives on the watch)
| Capability | SDK (C) |
|---|---|
| Accelerometer: sampled data, shake/tap detection | `accel_data_service_subscribe`, `accel_tap_service_subscribe`, `accel_raw_data_service_subscribe` |
| Magnetometer / compass heading | `compass_service_subscribe` |

### Health & heart rate (collected on-watch by the firmware)
| Capability | SDK (C) |
|---|---|
| Steps, distance, calories, active time | `health_service_sum`, `health_service_aggregate_*` (`HealthMetricStepCount`, …) |
| Sleep totals | `HealthMetricSleepSeconds`, `HealthMetricSleepRestfulSeconds` |
| Heart rate (BPM) on HR-capable models | `HealthMetricHeartRateBPM`, `health_service_peek_current_value` |
| Activity / sleep change events | `health_service_events_subscribe` |

> Reading health data is on-watch. Long-term history and cross-device sync is a
> Tier-2/3 concern (Pebble Health app on the phone / cloud).

### Power
| Capability | SDK (C) |
|---|---|
| Battery %, charging, plugged-in | `battery_state_service_peek`, `battery_state_service_subscribe` |

### Storage, scheduling & background work
| Capability | SDK (C) |
|---|---|
| Persistent key/value storage (per app, on watch) | `persist_read_*`, `persist_write_*` |
| Launcher "last used / glance" slices | `app_glance_reload` |
| Wake the app at a future time (RTC alarm) | `wakeup_schedule`, `wakeup_service_subscribe` |
| Background worker process | `app_worker_*` (`pebble_worker.h`) |
| App focus (in/out of foreground) | `app_focus_service_subscribe` |

### Hardware accessories
| Capability | SDK (C) |
|---|---|
| Talk to a Smartstrap accessory on the band connector | `smartstrap_subscribe`, `smartstrap_attribute_*` |

---

## 2. Requires a connected phone (Bluetooth + Pebble mobile app)

These need the watch paired and connected to the phone running the Pebble app.
The watch C side uses **AppMessage**; the phone side is **PebbleKit JS**.

| Capability | Watch side (C) | Phone side (PebbleKit JS) |
|---|---|---|
| Send/receive key-value messages watch ↔ phone | `app_message_*`, `dict_*` | `Pebble.sendAppMessage`, `on('appmessage')` |
| Know whether the phone is reachable | `connection_service_subscribe` (`connection_service_peek_pebble_app_connection`) | — |
| App settings / configuration screen | receive config via `app_message` | `showConfiguration` + `Pebble.openURL` (Clay) |
| Buffered data export (synced when connected) | `data_logging_create`, `data_logging_log` | delivered to the phone app |
| Push a simple notification onto the watch | — | `Pebble.showSimpleNotificationOnPebble` |
| Identify the user / this watch | — | `Pebble.getAccountToken`, `Pebble.getWatchToken` |
| Tune Bluetooth latency vs. battery | `app_comm_set_sniff_interval` | — |

> This watchface (**Crisp**) lives mostly in Tier 1. It uses **Tier 2**
> for its **Clay** configuration page (settings arrive as one `AppMessage`,
> handled in `src/c/comm.c`) and **Tier 3** for its optional weather / rain-chance
> corners: `src/pkjs/index.js` geolocates the phone, queries the Open-Meteo web
> API, and forwards the result to the watch over AppMessage.

Phone-originated features the watch *displays* but a third-party app does not
fully own — incoming **phone notifications** and **now-playing / music
controls** — also belong here: they exist only while the phone is connected.

---

## 3. Requires internet / online services

The watch has no radio of its own to the internet — every online feature is
relayed by **PebbleKit JS on the phone**, which has normal web access, and then
forwarded to the watch over AppMessage.

| Capability | How |
|---|---|
| Any web/REST API (weather, transit, scores, your backend…) | `XMLHttpRequest` / `fetch` in PebbleKit JS, results sent via `Pebble.sendAppMessage` |
| Device location (phone GPS / network) | `navigator.geolocation` in PebbleKit JS |
| Timeline pins (push events to the user's timeline) | Pebble **timeline** web API; `Pebble.getTimelineToken`, `Pebble.timelineSubscribe` |
| Voice dictation / speech-to-text | `dictation_session_*` (C) — the audio is recognised by an online speech service **through the phone** |
| Remotely hosted configuration page | `Pebble.openURL(<https URL>)` instead of an offline Clay data-URL |
| Account-linked services | account/watch/timeline **tokens** identify the user to your server |
| Accurate time, app install/updates, app store | handled by the phone app + Pebble cloud |

> Voice dictation is the one feature that spans all three tiers: a **C API on
> the watch** captures intent, but it needs the **phone** to reach an **online**
> recognizer, so it only works fully online.

---

## Architecture cheat-sheet

```
            Tier 1                Tier 2                     Tier 3
   ┌───────────────────┐   ┌──────────────────┐   ┌────────────────────────┐
   │  Watch (C, pebble.h)│  │  Pebble mobile app │  │   Internet / cloud      │
   │  display, buttons,  │  │  (PebbleKit JS)    │  │   weather, your API,    │
   │  vibration, sensors,│◄─┤  AppMessage bridge ├─►│   timeline, speech,     │
   │  health, storage,   │  │  notifications,    │  │   geolocation source    │
   │  wakeup, smartstrap │  │  config, tokens    │  │                        │
   └───────────────────┘   └──────────────────┘   └────────────────────────┘
         standalone            +Bluetooth phone          +network
```

- Watch → phone is **always** AppMessage over Bluetooth (`app_message_*`).
- Phone → internet is **always** ordinary JS networking in PebbleKit JS.
- A feature's tier = the farthest hop its data must make.

*(An alternative watch-side language, **Rocky.js**, runs JavaScript directly on
the watch; the same three-tier model applies — `postMessage` replaces
AppMessage. This project is C, so the C APIs above are what apply here.)*
