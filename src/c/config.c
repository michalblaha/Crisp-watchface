#include "config.h"

static bool s_arr[NUM_SETTINGS];
static uint32_t s_colors[NUM_COLORS];
static int s_corners[NUM_CORNERS];
static int s_temp_unit;
static int s_theme;
static int s_weather_temp_min;
static int s_weather_temp_max;
static int s_weather_precip;
static bool s_weather_valid;

void config_init() {
  // Set defaults on first launch (before any config arrives from the phone).
  if(!persist_exists(PERSIST_DEFAULTS_SET)) {
    persist_write_bool(PERSIST_DEFAULTS_SET, true);

    persist_write_bool(PERSIST_KEY_DATE, true);
    persist_write_bool(PERSIST_KEY_DAY, true);
    persist_write_bool(PERSIST_KEY_BT, true);
    persist_write_bool(PERSIST_KEY_BATTERY, true);
    // Second hand ships disabled; the wearer opts in from the config page.
    persist_write_bool(PERSIST_KEY_SECOND_HAND, false);
    // Show the heart(s) above the center while a heart-rate sample is in
    // progress; unobtrusive (only visible during measurement), so on by default.
    persist_write_bool(PERSIST_KEY_HR_INDICATOR, true);
    // Temperature unit follows the watch's measurement system by default.
    persist_write_int(PERSIST_KEY_TEMP_UNIT, TEMP_UNIT_AUTO);
    // Ship the dark theme; the accent defaults below are tuned for it.
    persist_write_int(PERSIST_KEY_THEME, THEME_DARK);

    persist_write_int(PERSIST_KEY_COLOR_OFFSET + PERSIST_KEY_HOUR_MARKERS_COLOR, 0xFFFFFF);
    persist_write_int(PERSIST_KEY_COLOR_OFFSET + PERSIST_KEY_MINUTE_MARKERS_COLOR, 0xAAAAAA);
    persist_write_int(PERSIST_KEY_COLOR_OFFSET + PERSIST_KEY_CHARGING_MARKERS_COLOR, 0x00FF00);
    persist_write_int(PERSIST_KEY_COLOR_OFFSET + PERSIST_KEY_HANDS_COLOR, 0xAAAAAA);
    persist_write_int(PERSIST_KEY_COLOR_OFFSET + PERSIST_KEY_TIPS_COLOR, 0xFFFFFF);
    persist_write_int(PERSIST_KEY_COLOR_OFFSET + PERSIST_KEY_SECOND_HAND_COLOR, 0xAA0000);
    persist_write_int(PERSIST_KEY_COLOR_OFFSET + PERSIST_KEY_SECOND_TIP_COLOR, 0xFFFF00);
    persist_write_int(PERSIST_KEY_COLOR_OFFSET + PERSIST_KEY_CALENDAR_DAY_COLOR, 0xFFFF00);

    // Default corner readouts: battery + heart rate on top, steps + weather
    // on the bottom (matches the layout preview shown on the config page).
    persist_write_int(PERSIST_KEY_CORNER_OFFSET + CORNER_POS_TL, CORNER_BATTERY);
    persist_write_int(PERSIST_KEY_CORNER_OFFSET + CORNER_POS_TR, CORNER_HEART_RATE);
    persist_write_int(PERSIST_KEY_CORNER_OFFSET + CORNER_POS_BL, CORNER_STEPS);
    persist_write_int(PERSIST_KEY_CORNER_OFFSET + CORNER_POS_BR, CORNER_WEATHER);
  }

  // Migration: the heart-rate indicator toggle was added after the first
  // release, so installs that already have the defaults flag set never got its
  // seed (an unset bool reads back false). Seed it once for them too, so it
  // ships on by default for upgraders as well as fresh installs.
  if(!persist_exists(PERSIST_KEY_HR_INDICATOR)) {
    persist_write_bool(PERSIST_KEY_HR_INDICATOR, true);
  }

  for(int i = 0; i < NUM_SETTINGS; i++) {
    s_arr[i] = persist_read_bool(i);
  }

  for(int j = 0; j < NUM_COLORS; j++) {
    s_colors[j] = persist_read_int(PERSIST_KEY_COLOR_OFFSET + j);
  }

  for(int k = 0; k < NUM_CORNERS; k++) {
    s_corners[k] = persist_read_int(PERSIST_KEY_CORNER_OFFSET + k);
  }

  s_temp_unit = persist_read_int(PERSIST_KEY_TEMP_UNIT);
  s_theme     = persist_read_int(PERSIST_KEY_THEME);

  s_weather_valid    = persist_read_bool(PERSIST_KEY_WEATHER_VALID);
  s_weather_temp_min = persist_read_int(PERSIST_KEY_WEATHER_TEMP_MIN);
  s_weather_temp_max = persist_read_int(PERSIST_KEY_WEATHER_TEMP_MAX);
  s_weather_precip   = persist_read_int(PERSIST_KEY_WEATHER_PRECIP);
}

bool config_get(int key) {
  return s_arr[key];
}

uint32_t config_get_color(int key) {
  return s_colors[key];
}

int config_get_corner(int corner_index) {
  return s_corners[corner_index];
}

int config_get_temp_unit(void) {
  return s_temp_unit;
}

int config_get_theme(void) {
  return s_theme;
}

bool config_get_weather_valid(void) {
  return s_weather_valid;
}

int config_get_weather_temp_min(void) {
  return s_weather_temp_min;
}

int config_get_weather_temp_max(void) {
  return s_weather_temp_max;
}

int config_get_weather_precip(void) {
  return s_weather_precip;
}
