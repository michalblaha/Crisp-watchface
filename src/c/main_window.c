#include "main_window.h"

static Window *s_main_window;
static TextLayer *s_weekday_layer, *s_day_in_month_layer, *s_month_layer;
static Layer *s_canvas_layer, *s_bg_layer;

// One small readout per screen corner (CORNER_POS_*), shown only when its
// configured content kind is not CORNER_NONE.
static TextLayer *s_corner_layer[NUM_CORNERS];
static char s_corner_buffer[NUM_CORNERS][12];

static Time s_last_time, s_anim_time;
static char s_weekday_buffer[8], s_month_buffer[8], s_day_in_month_buffer[3];
static bool s_animating, s_connected;
// Whether the big font set is in use. Captured once from the full-screen radius
// at window_load so a Timeline Quick View peek (which shrinks the working area)
// never flips the date/corner fonts mid-animation.
static bool s_big;
static AppTimer *s_anim_timer;

#if defined(PBL_HEALTH)
// Heart-rate indicator state. s_hr_active is held true for HR_ACTIVE_WINDOW_MS
// after each HeartRateUpdate (the sensor does not signal when it stops), and
// s_hr_intensive tracks whether that measurement is happening inside a workout.
static bool s_hr_active, s_hr_intensive;
static AppTimer *s_hr_off_timer;
// A single heart silhouette as a filled GPath, sized once at window_load and
// re-positioned with gpath_move_to for each heart drawn.
static GPath *s_heart_path;
static GPoint s_heart_points[14];
static int s_heart_halfw, s_heart_gap;
#endif

static void corners_update(void);
static void corners_rebuild(void);
static void relayout(GRect visible);

// Theme-driven structural colors. The eight user accent colors are unaffected;
// these only cover what used to be hardcoded black/white: the panel background,
// the date/corner text, the B&W marker/hand fallback, and the fills that must
// read as "the background" (empty battery segment, hollow disconnect cap).
static GColor theme_bg(void) {
  return (config_get_theme() == THEME_LIGHT) ? GColorWhite : GColorBlack;
}

static GColor theme_fg(void) {
  return (config_get_theme() == THEME_LIGHT) ? GColorBlack : GColorWhite;
}

#ifdef PBL_COLOR
// Muted "off" color for the spent part of the battery gauge on color displays:
// dark gray on a black panel, light gray on a white one. (On B&W the empty
// segment blends into the background instead, so this is unused there.)
static GColor theme_battery_empty(void) {
  return (config_get_theme() == THEME_LIGHT) ? GColorLightGray : GColorDarkGray;
}
#endif

// Runtime layout, derived once from the display size so everything scales
// across platforms (emery 200x228 rect, gabbro 260x260 round, classic 144x168).
typedef struct {
  GPoint center;
  int radius;     // inscribed radius = min(half width, half height)
  int halfw, halfh;
  int margin;
  int thickness;
  int hand_sec, hand_min, hand_hour;
  int tick_hour, tick_min;
  int center_dot;
} Layout;
static Layout s_layout;

static int scaled(int base) {
  return s_layout.radius * base / LAYOUT_BASE_RADIUS;
}

static void layout_init(GRect bounds) {
  s_layout.center = grect_center_point(&bounds);
  s_layout.halfw = bounds.size.w / 2;
  s_layout.halfh = bounds.size.h / 2;
  s_layout.radius = (s_layout.halfw < s_layout.halfh) ? s_layout.halfw : s_layout.halfh;

  s_layout.margin    = scaled(LAYOUT_BASE_MARGIN);
  s_layout.thickness = scaled(LAYOUT_BASE_THICKNESS);
  if (s_layout.thickness < 2) s_layout.thickness = 2;
  s_layout.hand_sec  = scaled(LAYOUT_BASE_HAND_SEC);
  s_layout.hand_min  = scaled(LAYOUT_BASE_HAND_MIN);
  s_layout.hand_hour = scaled(LAYOUT_BASE_HAND_HOUR);
  s_layout.tick_hour = scaled(LAYOUT_BASE_TICK_HOUR);
  s_layout.tick_min  = scaled(LAYOUT_BASE_TICK_MIN);
  s_layout.center_dot = scaled(LAYOUT_BASE_CENTER);
  if (s_layout.center_dot < 3) s_layout.center_dot = 3;
}

static GPoint point_at(int32_t angle, int len) {
  return (GPoint) {
    .x = (int16_t)(sin_lookup(angle) * (int32_t)len / TRIG_MAX_RATIO) + s_layout.center.x,
    .y = (int16_t)(-cos_lookup(angle) * (int32_t)len / TRIG_MAX_RATIO) + s_layout.center.y,
  };
}

// Distance from the center to the display boundary (inset by the margin) along
// the given angle. On round displays that is a constant radius; on rectangular
// ones it follows the rectangle edge so the markers hug the border.
static int boundary_radius(int32_t angle) {
#ifdef CRISP_ROUND_DIAL
  return s_layout.radius - s_layout.margin;
#else
  int32_t s = sin_lookup(angle);
  int32_t c = cos_lookup(angle);
  int hw = s_layout.halfw - s_layout.margin;
  int hh = s_layout.halfh - s_layout.margin;
  int rx = (s == 0) ? (1 << 20) : (int)((int64_t)hw * TRIG_MAX_RATIO / (s > 0 ? s : -s));
  int ry = (c == 0) ? (1 << 20) : (int)((int64_t)hh * TRIG_MAX_RATIO / (c > 0 ? c : -c));
  return (rx < ry) ? rx : ry;
#endif
}

static void update_time(struct tm *now) {
  s_last_time.days = now->tm_mday;
  s_last_time.hours = now->tm_hour;
  s_last_time.minutes = now->tm_min;
  s_last_time.seconds = now->tm_sec;

  s_last_time.hours -= (s_last_time.hours > 12) ? 12 : 0;

  snprintf(s_day_in_month_buffer, sizeof(s_day_in_month_buffer), "%d", s_last_time.days);
  strftime(s_weekday_buffer, sizeof(s_weekday_buffer), "%a", now);
  strftime(s_month_buffer, sizeof(s_month_buffer), "%b", now);

  text_layer_set_text(s_weekday_layer, s_weekday_buffer);
  text_layer_set_text(s_day_in_month_layer, s_day_in_month_buffer);
  text_layer_set_text(s_month_layer, s_month_buffer);

  layer_mark_dirty(s_canvas_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  update_time(tick_time);
  corners_update();
}

static void subscribe_ticks(void) {
  // Drive the clock directly, independent of the launch animation, so the face
  // always advances even on platforms where the animation stopped handler does
  // not fire.
  if(config_get(PERSIST_KEY_SECOND_HAND)) {
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  } else {
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  }
}

/****************************** Corner readouts *******************************/

// Frames for the four corner readouts, derived from the display size in
// window_load so they scale across platforms.
static GRect s_corner_rect[NUM_CORNERS];

// Resolve whether to display temperature in Fahrenheit. AUTO follows the
// watch's metric/imperial preference where the platform exposes it, otherwise
// falls back to Celsius.
static bool weather_use_fahrenheit(void) {
  int unit = config_get_temp_unit();
  if(unit == TEMP_UNIT_FAHRENHEIT) return true;
  if(unit == TEMP_UNIT_CELSIUS) return false;
#if defined(PBL_HEALTH)
  MeasurementSystem ms =
    health_service_get_measurement_system_for_display(HealthMetricWalkedDistanceMeters);
  return ms == MeasurementSystemImperial;
#else
  return false;
#endif
}

static int celsius_to_fahrenheit(int c) {
  float f = c * 9.0f / 5.0f + 32.0f;
  return (int)(f >= 0 ? f + 0.5f : f - 0.5f);
}

// Render the current value of one corner content kind into buf.
static void format_corner(int kind, char *buf, size_t n) {
  switch(kind) {
    case CORNER_BATTERY: {
      BatteryChargeState st = battery_state_service_peek();
      snprintf(buf, n, "%d%%", st.charge_percent);
      break;
    }
    case CORNER_BLUETOOTH:
      snprintf(buf, n, "%s", s_connected ? "BT" : "no BT");
      break;
    case CORNER_HEART_RATE: {
#if defined(PBL_HEALTH)
      HealthValue bpm = health_service_peek_current_value(HealthMetricHeartRateBPM);
      if(bpm > 0) {
        snprintf(buf, n, "%d", (int)bpm);
      } else {
        snprintf(buf, n, "--");
      }
#else
      snprintf(buf, n, "--");
#endif
      break;
    }
    case CORNER_STEPS: {
#if defined(PBL_HEALTH)
      int steps = (int)health_service_sum_today(HealthMetricStepCount);
      if(steps >= 1000) {
        snprintf(buf, n, "%d.%dk", steps / 1000, (steps % 1000) / 100);
      } else {
        snprintf(buf, n, "%d", steps);
      }
#else
      snprintf(buf, n, "--");
#endif
      break;
    }
    case CORNER_WEATHER:
      if(config_get_weather_valid()) {
        // Today's low/high, e.g. "8/15°" (UTF-8 degree sign). Stored in Celsius;
        // convert for display when Fahrenheit is selected.
        int lo = config_get_weather_temp_min();
        int hi = config_get_weather_temp_max();
        if(weather_use_fahrenheit()) {
          lo = celsius_to_fahrenheit(lo);
          hi = celsius_to_fahrenheit(hi);
        }
        snprintf(buf, n, "%d/%d\xC2\xB0", lo, hi);
      } else {
        snprintf(buf, n, "--/--\xC2\xB0");
      }
      break;
    case CORNER_PRECIP:
      if(config_get_weather_valid()) {
        snprintf(buf, n, "%d%%", config_get_weather_precip());
      } else {
        snprintf(buf, n, "--%%");
      }
      break;
    default:
      buf[0] = '\0';
      break;
  }
}

// Refresh the text of every shown corner from the latest live data.
static void corners_update(void) {
  for(int i = 0; i < NUM_CORNERS; i++) {
    int kind = config_get_corner(i);
    if(kind == CORNER_NONE) continue;
    format_corner(kind, s_corner_buffer[i], sizeof(s_corner_buffer[i]));
    text_layer_set_text(s_corner_layer[i], s_corner_buffer[i]);
  }
}

// Attach/detach the corner layers from the window to match the current config.
static void corners_rebuild(void) {
  if(!s_main_window) return;
  Layer *root = window_get_root_layer(s_main_window);
  for(int i = 0; i < NUM_CORNERS; i++) {
    Layer *l = text_layer_get_layer(s_corner_layer[i]);
    if(config_get_corner(i) == CORNER_NONE) {
      layer_remove_from_parent(l);
    } else {
      layer_add_child(root, l);
    }
  }
}

#if defined(PBL_HEALTH)
// No event fires when the sensor stops sampling, so the indicator is held on a
// timeout: this clears it once HR_ACTIVE_WINDOW_MS passes with no new sample.
static void hr_off_timeout(void *data) {
  s_hr_off_timer = NULL;
  if(s_hr_active || s_hr_intensive) {
    s_hr_active = false;
    s_hr_intensive = false;
    layer_mark_dirty(s_canvas_layer);
  }
}

// A fresh heart-rate sample arrived: light the indicator and (re)arm the
// timeout that turns it off when samples stop. Two hearts while in a workout
// (the system samples the heart rate intensively during run / open workout).
static void hr_on_sample(void) {
  if(!config_get(PERSIST_KEY_HR_INDICATOR)) return;

  time_t now = time(NULL);
  bool available =
    (health_service_metric_accessible(HealthMetricHeartRateBPM, now, now)
       & HealthServiceAccessibilityMaskAvailable) != 0;
  HealthValue bpm = health_service_peek_current_value(HealthMetricHeartRateBPM);
  if(!available || bpm <= 0) return;

  HealthActivityMask act = health_service_peek_current_activities();
  s_hr_active = true;
  s_hr_intensive = (act & (HealthActivityRun | HealthActivityOpenWorkout)) != 0;

  if(s_hr_off_timer) {
    app_timer_reschedule(s_hr_off_timer, HR_ACTIVE_WINDOW_MS);
  } else {
    s_hr_off_timer = app_timer_register(HR_ACTIVE_WINDOW_MS, hr_off_timeout, NULL);
  }
  layer_mark_dirty(s_canvas_layer);
}

static void health_handler(HealthEventType event, void *context) {
  if(event == HealthEventHeartRateUpdate) {
    hr_on_sample();
  }
  if(event == HealthEventHeartRateUpdate ||
     event == HealthEventMovementUpdate ||
     event == HealthEventSignificantUpdate) {
    corners_update();
  }
}
#endif

/****************************** AnimationImplementation ***********************/

static void end_sweep(void) {
  s_animating = false;
  layer_mark_dirty(s_canvas_layer);
}

static void animation_started(Animation *anim, void *context) {
  s_animating = true;
}

static void animation_stopped(Animation *anim, bool stopped, void *context) {
  end_sweep();
}

// Backstop in case the animation's stopped handler never fires.
static void anim_timeout(void *data) {
  s_anim_timer = NULL;
  end_sweep();
}

static void animate(int duration, int delay, AnimationImplementation *implementation, bool handlers) {
  Animation *anim = animation_create();
  if(anim) {
    animation_set_duration(anim, duration);
    animation_set_delay(anim, delay);
    animation_set_curve(anim, AnimationCurveEaseInOut);
    animation_set_implementation(anim, implementation);
    if(handlers) {
      animation_set_handlers(anim, (AnimationHandlers) {
        .started = animation_started,
        .stopped = animation_stopped
      }, NULL);
    }
    animation_schedule(anim);
  }
}

/****************************** Drawing Functions *****************************/

static void bg_update_proc(Layer *layer, GContext *ctx) {
  // Antialiasing is meaningful only on the color (non 1-bit) displays; enabling
  // it on B&W aplite hits a slow path and is pointless there.
  graphics_context_set_antialiased(ctx, PBL_IF_COLOR_ELSE(ANTIALIASING, false));

  BatteryChargeState state = battery_state_service_peek();
  int perc = state.charge_percent;
  int batt_hours = (int)(12.0F * ((float)perc / 100.0F)) + 1;

  int mmin = ( s_last_time.minutes / 5 ) * 5;
  int mmax = ( mmin + 5 );

  for(int m = 0; m < 60; m++) {
    int h = m / 5;
    bool isHourMarker = ( m % 5 ) == 0;
    int thickness = isHourMarker ? s_layout.thickness : (s_layout.thickness > 2 ? 2 : 1);
    int tick_len = isHourMarker ? s_layout.tick_hour : s_layout.tick_min;

    if (!isHourMarker && ( m < mmin || m > mmax )) continue;

    int32_t angle = TRIG_MAX_ANGLE * m / 60;
    int r_out = boundary_radius(angle);
    GPoint p_out = point_at(angle, r_out);
    GPoint p_in = point_at(angle, r_out - tick_len);

#ifdef PBL_COLOR
    GColor marker_color = isHourMarker
      ? GColorFromHEX(config_get_color(PERSIST_KEY_HOUR_MARKERS_COLOR))
      : GColorFromHEX(config_get_color(PERSIST_KEY_MINUTE_MARKERS_COLOR));
#else
    GColor marker_color = theme_fg();
#endif

    GColor draw_color = marker_color;
    if (config_get(PERSIST_KEY_BATTERY) && isHourMarker) {
      if (h < batt_hours) {
#ifdef PBL_COLOR
        draw_color = state.is_plugged
          ? GColorFromHEX(config_get_color(PERSIST_KEY_CHARGING_MARKERS_COLOR))
          : marker_color;
#else
        draw_color = theme_fg();
#endif
      } else {
        // Empty battery segment: muted gray on color, blended into the panel
        // background (invisible) on B&W.
        draw_color = PBL_IF_COLOR_ELSE(theme_battery_empty(), theme_bg());
      }
    }

    graphics_context_set_stroke_color(ctx, draw_color);
    graphics_context_set_stroke_width(ctx, thickness);
    graphics_draw_line(ctx, p_in, p_out);
  }
}

static void draw_hand(GContext *ctx, int32_t angle, int length, int tip_length,
                      GColor body_color, GColor tip_color, int thickness) {
  if (thickness < 1) thickness = 1;
  int body_len = length - tip_length;
  if (body_len < 0) body_len = 0;
  GPoint tip = point_at(angle, length);
  GPoint joint = point_at(angle, body_len);

  graphics_context_set_stroke_width(ctx, thickness);
  graphics_context_set_stroke_color(ctx, body_color);
  graphics_draw_line(ctx, s_layout.center, joint);

  graphics_context_set_stroke_color(ctx, tip_color);
  graphics_draw_line(ctx, joint, tip);
}

#if defined(PBL_HEALTH)
// Build the heart silhouette as a filled polygon anchored at its bottom tip
// (0,0) with the lobes above (negative y); height ~1.9*W. W is the half-width,
// scaled from the dial once at window_load. gpath_create keeps a reference to
// s_heart_points, so that array must stay alive (it is static).
static void heart_path_init(int W) {
  if(W < 3) W = 3;
  s_heart_halfw = W;
  s_heart_gap = scaled(LAYOUT_BASE_HEART_GAP);
  if(s_heart_gap < 1) s_heart_gap = 1;

  const GPoint pts[14] = {
    { 0,             0 },
    { (6 * W) / 10,  (-7 * W) / 10 },
    { W,             (-11 * W) / 10 },
    { W,             (-15 * W) / 10 },
    { (85 * W) / 100,(-18 * W) / 10 },
    { (5 * W) / 10,  (-19 * W) / 10 },
    { (2 * W) / 10,  (-175 * W) / 100 },
    { 0,             (-15 * W) / 10 },
    { (-2 * W) / 10, (-175 * W) / 100 },
    { (-5 * W) / 10, (-19 * W) / 10 },
    { (-85 * W) / 100,(-18 * W) / 10 },
    { -W,            (-15 * W) / 10 },
    { -W,            (-11 * W) / 10 },
    { (-6 * W) / 10, (-7 * W) / 10 },
  };
  for(int i = 0; i < 14; i++) s_heart_points[i] = pts[i];

  static GPathInfo info;
  info.num_points = 14;
  info.points = s_heart_points;
  if(s_heart_path) gpath_destroy(s_heart_path);
  s_heart_path = gpath_create(&info);
}

// Draw one (measuring) or two (intensive) hearts just above the center cap,
// centered horizontally on the dial. Red on color displays, theme foreground
// on B&W.
static void draw_hr_hearts(GContext *ctx, int count) {
  if(!s_heart_path) return;
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorFromHEX(HR_HEART_COLOR_HEX), theme_fg()));

  int tip_y = s_layout.center.y - s_layout.center_dot - s_heart_gap;
  if(count <= 1) {
    gpath_move_to(s_heart_path, GPoint(s_layout.center.x, tip_y));
    gpath_draw_filled(ctx, s_heart_path);
  } else {
    int off = s_heart_halfw + s_heart_gap / 2;
    gpath_move_to(s_heart_path, GPoint(s_layout.center.x - off, tip_y));
    gpath_draw_filled(ctx, s_heart_path);
    gpath_move_to(s_heart_path, GPoint(s_layout.center.x + off, tip_y));
    gpath_draw_filled(ctx, s_heart_path);
  }
}
#endif

static void draw_proc(Layer *layer, GContext *ctx) {
  // Antialiasing only matters on the color displays (see bg_update_proc).
  graphics_context_set_antialiased(ctx, PBL_IF_COLOR_ELSE(ANTIALIASING, false));

  Time mode_time = (s_animating) ? s_anim_time : s_last_time;

  int32_t second_angle = TRIG_MAX_ANGLE * mode_time.seconds / 60;
  int32_t minute_angle = TRIG_MAX_ANGLE * mode_time.minutes / 60;

  // Hour hand advances smoothly with the minutes (kept in float for accuracy).
  float minute_frac = (float)minute_angle / (float)TRIG_MAX_ANGLE;
  float hour_angle_f;
  if(s_animating) {
    // Hours out of 60 for the launch sweep.
    hour_angle_f = (float)TRIG_MAX_ANGLE * mode_time.hours / 60.0f;
  } else {
    hour_angle_f = (float)TRIG_MAX_ANGLE * mode_time.hours / 12.0f;
  }
  hour_angle_f += minute_frac * ((float)TRIG_MAX_ANGLE / 12.0f);
  int32_t hour_angle = (int32_t)hour_angle_f;

#ifdef PBL_COLOR
  GColor hands_color = GColorFromHEX(config_get_color(PERSIST_KEY_HANDS_COLOR));
  GColor tips_color = GColorFromHEX(config_get_color(PERSIST_KEY_TIPS_COLOR));
  GColor second_color = GColorFromHEX(config_get_color(PERSIST_KEY_SECOND_HAND_COLOR));
  GColor second_tip_color = GColorFromHEX(config_get_color(PERSIST_KEY_SECOND_TIP_COLOR));
#else
  GColor hands_color = theme_fg();
  GColor tips_color = theme_fg();
  GColor second_color = theme_fg();
  GColor second_tip_color = theme_fg();
#endif

  int tip_len = s_layout.margin;

  // Hour and minute hands (body + colored tip).
  draw_hand(ctx, minute_angle, s_layout.hand_min, tip_len, hands_color, tips_color, s_layout.thickness);
  draw_hand(ctx, hour_angle, s_layout.hand_hour, tip_len, hands_color, tips_color, s_layout.thickness);

  // Second hand.
  if(config_get(PERSIST_KEY_SECOND_HAND)) {
    draw_hand(ctx, second_angle, s_layout.hand_sec, tip_len, second_color, second_tip_color,
              s_layout.thickness - 1);
  }

  // Center cap.
  graphics_context_set_fill_color(ctx, tips_color);
  graphics_fill_circle(ctx, s_layout.center, s_layout.center_dot);

  // Hollow the cap to signal a dropped Bluetooth connection (punch the panel
  // background through the cap).
  if(config_get(PERSIST_KEY_BT) && !s_connected) {
    graphics_context_set_fill_color(ctx, theme_bg());
    graphics_fill_circle(ctx, s_layout.center, s_layout.center_dot - 1);
  }

#if defined(PBL_HEALTH)
  // Heart-rate indicator above the center: one heart while measuring, two while
  // measuring intensively (workout). Hidden during the launch sweep.
  if(config_get(PERSIST_KEY_HR_INDICATOR) && s_hr_active && !s_animating) {
    draw_hr_hearts(ctx, s_hr_intensive ? 2 : 1);
  }
#endif
}

static void bt_handler(bool connected) {
  // Buzz on a dropped link, but only when the disconnection indicator is on.
  if(!connected && s_connected && config_get(PERSIST_KEY_BT)) {
    vibes_long_pulse();
  }

  s_connected = connected;
  corners_update();
  layer_mark_dirty(s_canvas_layer);
}

static void batt_handler(BatteryChargeState state) {
  corners_update();
  layer_mark_dirty(s_canvas_layer);
}

// Recompute everything whose position depends on the working area and apply it
// to the live layers: the dial Layout (center, radius, hand/tick lengths), the
// date block frames and the four corner-readout frames. `visible` is the area
// not covered by a system overlay (Timeline Quick View) — at rest it equals the
// full window, and during a peek it is the window minus the bottom overlay, so
// feeding it here lifts the dial up and tightens it to stay a complete circle.
// Fonts and the big/small height switch stay fixed (driven by s_big, captured
// from the full-screen radius) so a peek never reflows the text.
static void relayout(GRect visible) {
  layout_init(visible);

  int day_h = s_big ? LAYOUT_DATE_DAY_H_BIG : LAYOUT_DATE_DAY_H_SMALL;
  int label_h = s_big ? LAYOUT_DATE_LABEL_H_BIG : LAYOUT_DATE_LABEL_H_SMALL;
  int block_w = visible.size.w * LAYOUT_DATE_BLOCK_W_NUM / LAYOUT_DATE_BLOCK_W_DEN;
  int block_x = s_layout.center.x + visible.size.w * LAYOUT_DATE_BLOCK_X_NUM / LAYOUT_DATE_BLOCK_X_DEN;
  if (block_x + block_w > visible.size.w) {
    block_x = visible.size.w - block_w;
  }
  int day_y = s_layout.center.y - day_h / 2;
  layer_set_frame(text_layer_get_layer(s_weekday_layer),
                  GRect(block_x, day_y - label_h, block_w, label_h));
  layer_set_frame(text_layer_get_layer(s_day_in_month_layer),
                  GRect(block_x, day_y, block_w, day_h));
  layer_set_frame(text_layer_get_layer(s_month_layer),
                  GRect(block_x, day_y + day_h, block_w, label_h));

  // Corner readouts sit in the true panel corners on rectangular displays
  // (emery / Time 2 included). A physically round display has no corners, so
  // there the boxes are pulled in just enough to clear the bezel — the literal
  // corner pixels are outside the visible circle.
  int corner_w = visible.size.w * LAYOUT_CORNER_W_NUM / LAYOUT_CORNER_W_DEN;
  int corner_h = s_big ? LAYOUT_CORNER_H_BIG : LAYOUT_CORNER_H_SMALL;
#ifdef PBL_ROUND
  int inset = s_layout.radius * LAYOUT_CORNER_INSET_ROUND_NUM / LAYOUT_CORNER_INSET_ROUND_DEN;
#else
  int inset = scaled(LAYOUT_CORNER_INSET_RECT);
  if(inset < 2) inset = 2;
#endif
  int right_x = visible.size.w - inset - corner_w;
  int bottom_y = visible.size.h - inset - corner_h;
  s_corner_rect[CORNER_POS_TL] = GRect(inset,   inset,    corner_w, corner_h);
  s_corner_rect[CORNER_POS_TR] = GRect(right_x, inset,    corner_w, corner_h);
  s_corner_rect[CORNER_POS_BL] = GRect(inset,   bottom_y, corner_w, corner_h);
  s_corner_rect[CORNER_POS_BR] = GRect(right_x, bottom_y, corner_w, corner_h);
  for(int i = 0; i < NUM_CORNERS; i++) {
    layer_set_frame(text_layer_get_layer(s_corner_layer[i]), s_corner_rect[i]);
  }
}

// A Timeline Quick View peek changed the unobstructed area. During the slide the
// service animates layer_get_unobstructed_bounds(), so simply re-laying out from
// it on every callback rides that animation for free. The dial layers cover the
// whole window (not just the visible band) per the SDK rule, so only the drawn
// geometry — not the layer frames — shrinks behind the overlay.
static void apply_unobstructed(void) {
  if(!s_main_window) return;
  relayout(layer_get_unobstructed_bounds(window_get_root_layer(s_main_window)));
  layer_mark_dirty(s_bg_layer);
  layer_mark_dirty(s_canvas_layer);
}

static void unobstructed_change(AnimationProgress progress, void *context) {
  apply_unobstructed();
}

static void unobstructed_did_change(void *context) {
  apply_unobstructed();
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Capture the big/small font choice from the FULL-screen radius so a later
  // peek (which shrinks the dial) cannot flip the fonts mid-animation.
  layout_init(bounds);
  s_big = s_layout.radius >= LAYOUT_BIG_RADIUS_THRESHOLD;

#if defined(PBL_HEALTH)
  // Size the heart from the full-screen dial (kept constant across a peek).
  heart_path_init(scaled(LAYOUT_BASE_HEART_HALFW));
#endif

  s_bg_layer = layer_create(bounds);
  layer_set_update_proc(s_bg_layer, bg_update_proc);
  layer_add_child(window_layer, s_bg_layer);

  const char *label_font = s_big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD;
  const char *day_font = s_big ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD;

  // Date block (frames are set by relayout below).
  s_weekday_layer = text_layer_create(bounds);
  text_layer_set_text_alignment(s_weekday_layer, GTextAlignmentCenter);
  text_layer_set_font(s_weekday_layer, fonts_get_system_font(label_font));
  text_layer_set_text_color(s_weekday_layer, theme_fg());
  text_layer_set_background_color(s_weekday_layer, GColorClear);

  s_day_in_month_layer = text_layer_create(bounds);
  text_layer_set_text_alignment(s_day_in_month_layer, GTextAlignmentCenter);
  text_layer_set_font(s_day_in_month_layer, fonts_get_system_font(day_font));
#ifdef PBL_COLOR
  text_layer_set_text_color(s_day_in_month_layer, GColorFromHEX(config_get_color(PERSIST_KEY_CALENDAR_DAY_COLOR)));
#else
  text_layer_set_text_color(s_day_in_month_layer, theme_fg());
#endif
  text_layer_set_background_color(s_day_in_month_layer, GColorClear);

  s_month_layer = text_layer_create(bounds);
  text_layer_set_text_alignment(s_month_layer, GTextAlignmentCenter);
  text_layer_set_font(s_month_layer, fonts_get_system_font(label_font));
  text_layer_set_text_color(s_month_layer, theme_fg());
  text_layer_set_background_color(s_month_layer, GColorClear);

  if(config_get(PERSIST_KEY_DAY)) {
    layer_add_child(window_layer, text_layer_get_layer(s_day_in_month_layer));
  }
  if(config_get(PERSIST_KEY_DATE)) {
    layer_add_child(window_layer, text_layer_get_layer(s_weekday_layer));
    layer_add_child(window_layer, text_layer_get_layer(s_month_layer));
  }

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, draw_proc);
  layer_add_child(window_layer, s_canvas_layer);

  const char *corner_font = s_big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD;
  for(int i = 0; i < NUM_CORNERS; i++) {
    s_corner_layer[i] = text_layer_create(bounds);
    GTextAlignment align = (i == CORNER_POS_TR || i == CORNER_POS_BR)
      ? GTextAlignmentRight : GTextAlignmentLeft;
    text_layer_set_text_alignment(s_corner_layer[i], align);
    text_layer_set_font(s_corner_layer[i], fonts_get_system_font(corner_font));
    text_layer_set_text_color(s_corner_layer[i], theme_fg());
    text_layer_set_background_color(s_corner_layer[i], GColorClear);
  }

  // Lay everything out against the unobstructed area so the face is correct even
  // if it launches while a Timeline Quick View peek is already up.
  relayout(layer_get_unobstructed_bounds(window_layer));

  corners_rebuild();
  corners_update();
}

static void window_unload(Window *window) {
  if(s_anim_timer) {
    app_timer_cancel(s_anim_timer);
    s_anim_timer = NULL;
  }

  unobstructed_area_service_unsubscribe();

#if defined(PBL_HEALTH)
  if(s_hr_off_timer) {
    app_timer_cancel(s_hr_off_timer);
    s_hr_off_timer = NULL;
  }
  if(s_heart_path) {
    gpath_destroy(s_heart_path);
    s_heart_path = NULL;
  }
#endif

  layer_destroy(s_canvas_layer);
  layer_destroy(s_bg_layer);

  text_layer_destroy(s_weekday_layer);
  text_layer_destroy(s_day_in_month_layer);
  text_layer_destroy(s_month_layer);

  for(int i = 0; i < NUM_CORNERS; i++) {
    text_layer_destroy(s_corner_layer[i]);
  }

  // Self destroying
  window_destroy(s_main_window);
}

static int anim_percentage(AnimationProgress dist_normalized, int max) {
  return (int)(float)(((float)dist_normalized / (float)ANIMATION_NORMALIZED_MAX) * (float)max);
}

static int hours_to_minutes(int hours_out_of_12) {
  return (int)(float)(((float)hours_out_of_12 / 12.0F) * 60.0F);
}

static void hands_update(Animation *anim, AnimationProgress dist_normalized) {
  s_last_time.hours -= (s_last_time.hours > 12) ? 12 : 0;

  s_anim_time.hours = anim_percentage(dist_normalized, hours_to_minutes(s_last_time.hours));
  s_anim_time.minutes = anim_percentage(dist_normalized, s_last_time.minutes);
  s_anim_time.seconds = anim_percentage(dist_normalized, s_last_time.seconds);

  layer_mark_dirty(s_canvas_layer);
}

void main_window_push() {
  s_main_window = window_create();
  window_set_background_color(s_main_window, theme_bg());
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_main_window, true);

  battery_state_service_subscribe(batt_handler);

#if defined(PBL_HEALTH)
  // Drive the heart-rate / steps corners as fresh samples arrive.
  health_service_events_subscribe(health_handler, NULL);
#endif

  // Seed the current time and date so the face is correct from the first frame,
  // and start ticking immediately rather than waiting for the launch sweep.
  time_t t = time(NULL);
  update_time(localtime(&t));
  subscribe_ticks();

  // Always track the connection: it feeds both the dropped-link indicator and
  // the optional Bluetooth corner. The handler buzzes only when the indicator
  // is enabled.
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bt_handler
  });
  bt_handler(connection_service_peek_pebble_app_connection());

  // React to a Timeline Quick View peek: re-lay out from the (animated)
  // unobstructed area so the dial lifts and tightens while the overlay is up.
  unobstructed_area_service_subscribe((UnobstructedAreaHandlers) {
    .change = unobstructed_change,
    .did_change = unobstructed_did_change,
  }, NULL);

  // Begin smooth launch sweep. A timer guarantees the sweep ends even where the
  // animation stopped handler is unreliable.
  static AnimationImplementation hands_impl = {
    .update = hands_update
  };
  animate(ANIMATION_DURATION, ANIMATION_DELAY, &hands_impl, true);
  s_anim_timer = app_timer_register(ANIMATION_DELAY + ANIMATION_DURATION + ANIMATION_SETTLE,
                                    anim_timeout, NULL);
}

void main_window_refresh(void) {
  if(!s_main_window) return;

  // A fresh config may have toggled the second hand (changes the tick rate) or
  // reassigned the corners; re-apply both and repaint with any new colors.
  subscribe_ticks();
  corners_rebuild();
  corners_update();

  // A theme switch flips the panel background and the text colors, which were
  // set once at window_load; re-apply them so the change shows without a
  // relaunch. (The accent colors are read live in the draw procs below.)
  window_set_background_color(s_main_window, theme_bg());
  text_layer_set_text_color(s_weekday_layer, theme_fg());
  text_layer_set_text_color(s_month_layer, theme_fg());
#ifdef PBL_COLOR
  text_layer_set_text_color(s_day_in_month_layer,
                            GColorFromHEX(config_get_color(PERSIST_KEY_CALENDAR_DAY_COLOR)));
#else
  text_layer_set_text_color(s_day_in_month_layer, theme_fg());
#endif
  for(int i = 0; i < NUM_CORNERS; i++) {
    text_layer_set_text_color(s_corner_layer[i], theme_fg());
  }

  layer_mark_dirty(s_bg_layer);
  layer_mark_dirty(s_canvas_layer);
}
