# Changelog

All notable changes to Crisp are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/), and the project follows
[Semantic Versioning](https://semver.org/).

## [1.3.0] - 2026-06-09

Added a heart-rate indicator and made the dial adapt to Timeline Quick View.

### Added

- **Heart-rate indicator**: one heart above the center while the watch is
  sampling your heart rate, two while it samples intensively during a workout
  (run / open workout). Held briefly after the last sample, then cleared.
- **Heart rate indicator** toggle on the config page (on by default), seeded for
  existing installs too so upgraders get it without resetting their settings.

### Changed

- The dial now reacts to **Timeline Quick View**: when the peek slides up from
  the bottom, the whole face lifts and tightens into the visible area so it stays
  a complete circle and the bottom corner readouts stay on screen.

### Notes

- The heart-rate indicator needs a health-capable Pebble; it is absent on models
  without the sensor. "Intensive" follows the watch's current workout activity.
- Round displays do not show Timeline Quick View, so the lift only applies to
  rectangular models.

## [1.2.0] - 2026-06-06

Added a light theme and made the color setup friendlier.

### Added

- **Dark / Light theme** toggle on the config page. Light inverts the panel
  background, the date and corner text, the black-and-white marker/hand fallback
  and the structural fills (the spent battery segment and the hollow disconnect
  cap). Dark stays the default.
- **Per-theme accent palettes**: each theme remembers its own marker, hand,
  second-hand and day-of-month colors, seeded with defaults tuned for that
  background. Switching the theme loads that theme's palette, so customizing one
  never disturbs the other.
- **Reset colors** button that restores the active theme's default palette.
- The app version is shown under the Save button on the config page.

### Notes

- The theme switch and per-theme palette memory are handled on the phone (Clay);
  the watch always holds the active palette, so the face stays fully configured
  offline.

## [1.1.0] - 2026-06-06

Renamed to **Crisp** and turned the dial's empty space into live information.

### Added

- Four configurable screen corners. Each shows one of: nothing, battery level,
  Bluetooth status, heart rate, step count, today's low/high temperature, or the
  current-hour rain chance. Defaults: battery, heart rate, steps, weather.
- Weather and rain chance, relayed from the phone via PebbleKit JS using the
  free, key-less Open-Meteo service and the phone's location.
- Temperature unit setting: Automatic (follows the watch's metric/imperial
  preference), Celsius, or Fahrenheit. Conversion happens on the watch, so
  switching is instant and needs no re-fetch.
- Appstore assets: native-resolution screenshots per platform, a 720x320
  marketing banner, 80x80 and 144x144 icons, listing copy and release notes.

### Changed

- Renamed the watchface from "Thin Markers" to **Crisp** (display name, config
  page, internal `CRISP_ROUND_DIAL`).
- The second hand now ships **off** by default (was on) to save battery.
- Configuration is re-applied live on the running face (corners, tick rate,
  colors) instead of only at launch.

### Notes

- Heart rate and step count require a health-capable Pebble; on other models
  those corners show `--`.
- Weather and rain chance need the Pebble app connected and location access.

## [1.0.0] - 2026-05-31

Initial analog watchface ("Thin Markers"), the basis Crisp builds on.

### Added

- Analog dial that scales proportionally to every Pebble display, with thin edge
  markers and the current five-minute sector of minute ticks.
- Optional second hand and date block (weekday, day of month, month).
- Battery gauge drawn onto the hour markers, and a disconnection indicator that
  hollows the center cap when the phone is out of range.
- Full color customization of markers, hands, tips, second hand and the day of
  the month (color platforms).
- Built-in Clay configuration page; settings persist on the watch and a smooth
  sweep animation plays on launch.
- Builds for aplite, basalt, chalk, diorite, emery, flint and gabbro.
