#include "comm.h"
#include "main_window.h"
#include <stdlib.h>  // atoi

// Read the integer value out of a tuple regardless of the width PebbleKit/Clay
// chose to encode it with (toggles arrive as 1-byte 0/1, colors as a 0xRRGGBB
// int). Returning the value uniformly keeps the handler simple.
static int32_t tuple_get_int(const Tuple *t) {
  switch (t->length) {
    case 1: return (t->type == TUPLE_INT) ? t->value->int8  : t->value->uint8;
    case 2: return (t->type == TUPLE_INT) ? t->value->int16 : t->value->uint16;
    default: return (t->type == TUPLE_INT) ? t->value->int32 : (int32_t)t->value->uint32;
  }
}

// Map a received AppMessage boolean key onto its persist slot. Returns whether
// the key was present.
static bool store_bool(DictionaryIterator *iter, uint32_t message_key, int persist_key) {
  Tuple *t = dict_find(iter, message_key);
  if (!t) return false;
  persist_write_bool(persist_key, tuple_get_int(t) != 0);
  return true;
}

// Map a received AppMessage color key onto its persist slot (Clay sends colors
// as 0xRRGGBB integers, ready for GColorFromHEX on read).
static bool store_color(DictionaryIterator *iter, uint32_t message_key, int color_index) {
  Tuple *t = dict_find(iter, message_key);
  if (!t) return false;
  persist_write_int(PERSIST_KEY_COLOR_OFFSET + color_index, tuple_get_int(t));
  return true;
}

// Map a received Clay "select" onto a persist slot. Clay sends the chosen value
// as a string ("0".."6"), so parse it; tolerate a raw int too.
static bool store_select(DictionaryIterator *iter, uint32_t message_key, int persist_key) {
  Tuple *t = dict_find(iter, message_key);
  if (!t) return false;
  int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : tuple_get_int(t);
  persist_write_int(persist_key, val);
  return true;
}

static bool store_corner(DictionaryIterator *iter, uint32_t message_key, int corner_pos) {
  return store_select(iter, message_key, PERSIST_KEY_CORNER_OFFSET + corner_pos);
}

// Store one integer weather field. Returns whether it was present.
static bool store_int(DictionaryIterator *iter, uint32_t message_key, int persist_key) {
  Tuple *t = dict_find(iter, message_key);
  if (!t) return false;
  persist_write_int(persist_key, tuple_get_int(t));
  return true;
}

static void in_recv_handler(DictionaryIterator *iter, void *context) {
  bool config_changed = false;

  // Booleans. The named MESSAGE_KEY_* macros are generated from package.json
  // and get auto-assigned numeric ids, so we explicitly route each key to the
  // persist slot the rest of the app reads from.
  config_changed |= store_bool(iter, MESSAGE_KEY_DATE,        PERSIST_KEY_DATE);
  config_changed |= store_bool(iter, MESSAGE_KEY_DAY,         PERSIST_KEY_DAY);
  config_changed |= store_bool(iter, MESSAGE_KEY_BT,          PERSIST_KEY_BT);
  config_changed |= store_bool(iter, MESSAGE_KEY_BATTERY,     PERSIST_KEY_BATTERY);
  config_changed |= store_bool(iter, MESSAGE_KEY_SECOND_HAND, PERSIST_KEY_SECOND_HAND);

  // Colors.
  config_changed |= store_color(iter, MESSAGE_KEY_HOUR_MARKERS_COLOR,     PERSIST_KEY_HOUR_MARKERS_COLOR);
  config_changed |= store_color(iter, MESSAGE_KEY_MINUTE_MARKERS_COLOR,   PERSIST_KEY_MINUTE_MARKERS_COLOR);
  config_changed |= store_color(iter, MESSAGE_KEY_CHARGING_MARKERS_COLOR, PERSIST_KEY_CHARGING_MARKERS_COLOR);
  config_changed |= store_color(iter, MESSAGE_KEY_HANDS_COLOR,            PERSIST_KEY_HANDS_COLOR);
  config_changed |= store_color(iter, MESSAGE_KEY_TIPS_COLOR,             PERSIST_KEY_TIPS_COLOR);
  config_changed |= store_color(iter, MESSAGE_KEY_SECOND_HAND_COLOR,      PERSIST_KEY_SECOND_HAND_COLOR);
  config_changed |= store_color(iter, MESSAGE_KEY_SECOND_TIP_COLOR,       PERSIST_KEY_SECOND_TIP_COLOR);
  config_changed |= store_color(iter, MESSAGE_KEY_CALENDAR_DAY_COLOR,     PERSIST_KEY_CALENDAR_DAY_COLOR);

  // Corner selections.
  config_changed |= store_corner(iter, MESSAGE_KEY_CORNER_TL, CORNER_POS_TL);
  config_changed |= store_corner(iter, MESSAGE_KEY_CORNER_TR, CORNER_POS_TR);
  config_changed |= store_corner(iter, MESSAGE_KEY_CORNER_BL, CORNER_POS_BL);
  config_changed |= store_corner(iter, MESSAGE_KEY_CORNER_BR, CORNER_POS_BR);

  // Temperature unit selection (auto / Celsius / Fahrenheit).
  config_changed |= store_select(iter, MESSAGE_KEY_TEMP_UNIT, PERSIST_KEY_TEMP_UNIT);

  // Color theme (dark / light). The phone sends the accent palette that matches
  // the chosen theme in the same message, so the colors above already describe
  // this theme.
  config_changed |= store_select(iter, MESSAGE_KEY_THEME, PERSIST_KEY_THEME);

  // Weather sample relayed by the phone (separate from a config edit). When a
  // sample arrives, mark the cache valid so the corners stop showing "--".
  bool weather_changed = false;
  weather_changed |= store_int(iter, MESSAGE_KEY_WEATHER_TEMP_MIN, PERSIST_KEY_WEATHER_TEMP_MIN);
  weather_changed |= store_int(iter, MESSAGE_KEY_WEATHER_TEMP_MAX, PERSIST_KEY_WEATHER_TEMP_MAX);
  weather_changed |= store_int(iter, MESSAGE_KEY_WEATHER_PRECIP,   PERSIST_KEY_WEATHER_PRECIP);
  if (weather_changed) {
    persist_write_bool(PERSIST_KEY_WEATHER_VALID, true);
  }

  if (!config_changed && !weather_changed) return;

  // Refresh the live store and the face.
  config_init();
  main_window_refresh();

  // Confirm a user-initiated config edit; stay silent on the periodic weather
  // refresh so the watch does not buzz every half hour.
  if (config_changed) {
    vibes_short_pulse();
  }
}

void comm_init() {
  app_message_register_inbox_received(in_recv_handler);
  app_message_open(APP_MESSAGE_INBOX_SIZE, APP_MESSAGE_OUTBOX_SIZE);
}
