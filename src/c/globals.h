#pragma once

#include <pebble.h>

#define ANTIALIASING true

// --- Layout ---------------------------------------------------------------
// All on-screen geometry is derived at runtime from the display size so the
// watchface scales correctly across platforms (emery 200x228 rect, gabbro
// 260x260 round, and the classic 144x168 displays). The constants below are
// the proportions the original art was tuned for, expressed against the
// 144x168 baseline whose inscribed half-axis is 72px.
#define LAYOUT_BASE_RADIUS    72
#define LAYOUT_BASE_MARGIN     5
#define LAYOUT_BASE_THICKNESS  3
#define LAYOUT_BASE_HAND_SEC  65
#define LAYOUT_BASE_HAND_MIN  65
#define LAYOUT_BASE_HAND_HOUR 45
#define LAYOUT_BASE_TICK_HOUR 11
#define LAYOUT_BASE_TICK_MIN   6
#define LAYOUT_BASE_CENTER     4

// Above this inscribed radius the larger system fonts are used for the date.
#define LAYOUT_BIG_RADIUS_THRESHOLD 90

// Corner readout boxes. Width is a fraction of the display width. On a
// rectangular panel the boxes sit in the true corners (a small scaled gap from
// the edge). A round panel has no corners, so there the boxes are pulled inward
// by a fraction of the inscribed radius — far enough to clear the rim markers,
// not just enough to stay inside the visible circle.
#define LAYOUT_CORNER_W_NUM        46
#define LAYOUT_CORNER_W_DEN       144
#define LAYOUT_CORNER_H_BIG        22
#define LAYOUT_CORNER_H_SMALL      18
#define LAYOUT_CORNER_INSET_RECT    3
#define LAYOUT_CORNER_INSET_ROUND_NUM 42   // percent of the inscribed radius
#define LAYOUT_CORNER_INSET_ROUND_DEN 100

// Render the round dial (markers on a circle, not hugging the panel edges) on
// the physically round displays AND on emery (Pebble Time 2), so its face
// matches the Pebble Round 2 (gabbro) look.
#if defined(PBL_ROUND) || defined(PBL_PLATFORM_EMERY)
#define CRISP_ROUND_DIAL 1
#endif

#define ANIMATION_DELAY    300
#define ANIMATION_DURATION 1000
// Backstop that ends the launch sweep independently of the animation's stopped
// handler (which does not fire on every platform, e.g. the aplite emulator).
#define ANIMATION_SETTLE   250

#define PERSIST_DEFAULTS_SET 3489

// AppMessage buffers. The config payload is 13 keys (5 toggles + 8 colors),
// ~150 bytes; we never send anything back. Requesting fixed, sized buffers
// instead of app_message_*_size_maximum() keeps the app within aplite's small
// RAM budget (the maximum-size request hangs the app there).
#define APP_MESSAGE_INBOX_SIZE  256
#define APP_MESSAGE_OUTBOX_SIZE  64

// Persist keys for the boolean settings (also used as their array index).
#define PERSIST_KEY_DATE        0
#define PERSIST_KEY_DAY         1
#define PERSIST_KEY_BT          2
#define PERSIST_KEY_BATTERY     3
#define PERSIST_KEY_SECOND_HAND 4
#define NUM_SETTINGS            5

// Temperature unit for the weather corner, stored as its own int (not a bool).
// AUTO follows the watch's metric/imperial preference when the platform exposes
// it (PBL_HEALTH), falling back to Celsius.
#define PERSIST_KEY_TEMP_UNIT  6
#define TEMP_UNIT_AUTO         0
#define TEMP_UNIT_CELSIUS      1
#define TEMP_UNIT_FAHRENHEIT   2

// Persist keys for the colors, stored at PERSIST_KEY_COLOR_OFFSET + index.
#define PERSIST_KEY_COLOR_OFFSET          10
#define PERSIST_KEY_HOUR_MARKERS_COLOR     0
#define PERSIST_KEY_MINUTE_MARKERS_COLOR   1
#define PERSIST_KEY_CHARGING_MARKERS_COLOR 2
#define PERSIST_KEY_HANDS_COLOR            3
#define PERSIST_KEY_TIPS_COLOR             4
#define PERSIST_KEY_SECOND_HAND_COLOR      5
#define PERSIST_KEY_SECOND_TIP_COLOR       6
#define PERSIST_KEY_CALENDAR_DAY_COLOR     7
#define NUM_COLORS                         8

// --- Screen corners -------------------------------------------------------
// Each of the four corners can show one optional readout. The value stored per
// corner is one of these content kinds (sent by Clay as the select value).
#define CORNER_NONE        0
#define CORNER_BATTERY     1
#define CORNER_BLUETOOTH   2
#define CORNER_HEART_RATE  3
#define CORNER_STEPS       4
#define CORNER_WEATHER     5   // current temperature (relayed by the phone)
#define CORNER_PRECIP      6   // current-hour precipitation probability

// Corner positions, used both as the s_corners[] index and the persist slot
// (stored at PERSIST_KEY_CORNER_OFFSET + position).
#define PERSIST_KEY_CORNER_OFFSET 20
#define CORNER_POS_TL 0
#define CORNER_POS_TR 1
#define CORNER_POS_BL 2
#define CORNER_POS_BR 3
#define NUM_CORNERS   4

// Latest weather sample relayed by the phone (Tier 3 via PebbleKit JS). Cached
// so the weather/precip corners survive a relaunch until the next refresh. The
// weather corner shows today's forecast low/high; precip is the current hour.
#define PERSIST_KEY_WEATHER_TEMP_MIN 30   // degrees Celsius, rounded (today low)
#define PERSIST_KEY_WEATHER_PRECIP   31   // percent, 0..100
#define PERSIST_KEY_WEATHER_VALID    32   // bool: a sample has ever arrived
#define PERSIST_KEY_WEATHER_TEMP_MAX 33   // degrees Celsius, rounded (today high)

typedef struct {
  int days;
  int hours;
  int minutes;
  int seconds;
} Time;
