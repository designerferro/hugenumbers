#include <pebble.h>
#include "numeral_assets.h"

#define SYSTEM_KEY 2
#define SCHEME_KEY 3
#define CUSTOM_UPPER_KEY 4
#define CUSTOM_LOWER_KEY 5
#define IDLE_BEHAVIOR_KEY 6
#define TRANSITION_FRAMES 8
#define CUSTOM_SCHEME 12
#define SCHEME_COUNT 13
#define DEFAULT_CUSTOM_UPPER 0x55AAFF
#define DEFAULT_CUSTOM_LOWER 0x000055
#define SHAKE_ACTIVE_MS 15000
#define IDLE_INVERTED_OUTLINE 0
#define IDLE_COLOR_OUTLINE 1
#define IDLE_FREEZE_NORMAL 2
#define IDLE_BEHAVIOR_COUNT 3

static Window *s_window;
static Layer *s_layer;
static AppTimer *s_timer;
#if !defined(_PBL_API_EXISTS_backlight_service_subscribe)
static AppTimer *s_shake_timer;
#endif
static uint8_t s_masks[4][MASK_CAPACITY];
static size_t s_lengths[4];
static GRect s_from[4], s_target[4], s_frames[4];
static int s_system, s_scheme, s_idle_behavior, s_second, s_minute = -1, s_step;
static uint32_t s_custom_upper, s_custom_lower;
static bool s_backlight_on = true;
static const uint32_t s_resources[] = {
  RESOURCE_ID_NUMERALS_WESTERN, RESOURCE_ID_NUMERALS_EASTERN,
  RESOURCE_ID_NUMERALS_DEVANAGARI
};

static uint32_t read_u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
         (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void load_mask(int slot, int digit, int state) {
  ResHandle handle = resource_get_handle(s_resources[s_system]);
  uint8_t offsets[8];
  s_lengths[slot] = 0;
  if (resource_load_byte_range(handle, (digit * NUMERAL_STATES + state) * 4,
                               offsets, sizeof(offsets)) != sizeof(offsets)) return;
  uint32_t start = read_u32(offsets), end = read_u32(offsets + 4);
  if (end <= start || end - start > MASK_CAPACITY) return;
  size_t length = end - start;
  if (resource_load_byte_range(handle, start, s_masks[slot], length) == length) {
    s_lengths[slot] = length;
  }
}

static void colors(GColor *upper, GColor *lower, GColor *ink_top, GColor *ink_bottom) {
#ifdef PBL_COLOR
  static const uint32_t palettes[][4] = {
    {0x55FFAA, 0xAA55FF, 0x000000, 0xFFFFFF},
    {0xFFFF55, 0x0000AA, 0x000000, 0xFFFFFF},
    {0xFFAA55, 0x550000, 0x000000, 0xFFFFFF},
    {0xFFFFFF, 0x000000, 0x000000, 0xFFFFFF},
    {0xAAFFFF, 0x0055AA, 0x000000, 0xFFFFFF},
    {0x55AAFF, 0x000055, 0x000000, 0xFFFFFF},
    {0x00AAFF, 0x0000AA, 0x000000, 0xFFFFFF},
    {0x00FFFF, 0x000055, 0x000000, 0xFFFFFF},
    {0xAAAAFF, 0x5500AA, 0x000000, 0xFFFFFF},
    {0xAAFFFF, 0x005555, 0x000000, 0xFFFFFF},
    {0x5555FF, 0x0000AA, 0x000000, 0xFFFFFF},
    {0x00AAAA, 0x000055, 0x000000, 0xFFFFFF}
  };
  uint32_t upper_hex = s_scheme == CUSTOM_SCHEME
      ? s_custom_upper : palettes[s_scheme][0];
  uint32_t lower_hex = s_scheme == CUSTOM_SCHEME
      ? s_custom_lower : palettes[s_scheme][1];
  *upper = GColorFromHEX(upper_hex);
  *lower = GColorFromHEX(lower_hex);
  // The animated face always uses solid white numerals across both fields.
  *ink_top = GColorWhite;
  *ink_bottom = GColorWhite;
#else
  *upper = GColorWhite; *lower = GColorBlack;
  *ink_top = GColorBlack; *ink_bottom = GColorWhite;
#endif
}

#ifdef PBL_COLOR
static GColor static_outline_color(GColor color) {
  // Pebble colors use two bits per RGB channel. Lift dark channels to the
  // upper half of the hardware palette so outlines keep their scheme hue but
  // remain readable against the static black background.
  uint8_t red = (color.argb >> 4) & 0x3;
  uint8_t green = (color.argb >> 2) & 0x3;
  uint8_t blue = color.argb & 0x3;
  if (red < 2) red = 2;
  if (green < 2) green = 2;
  if (blue < 2) blue = 2;
  return GColorFromRGB(red * 85, green * 85, blue * 85);
}
#endif

static void mask_rounded_corners(GContext *ctx, GRect bounds) {
#ifdef PBL_RECT
  // Mask only the pixels outside a 12 px rounded rectangle. Drawing this last
  // clips the color field and oversized numeral masks without a framebuffer.
  static const uint8_t insets[] = {9, 6, 5, 3, 3, 2, 1, 1, 0, 0, 0, 0};
  graphics_context_set_fill_color(ctx, GColorBlack);
  for (unsigned int row = 0; row < ARRAY_LENGTH(insets); ++row) {
    int inset = insets[row];
    if (!inset) continue;
    int top = bounds.origin.y + row;
    int bottom = bounds.origin.y + bounds.size.h - 1 - row;
    graphics_fill_rect(ctx, GRect(bounds.origin.x, top, inset, 1),
                       0, GCornerNone);
    graphics_fill_rect(ctx, GRect(bounds.origin.x + bounds.size.w - inset,
                                  top, inset, 1), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(bounds.origin.x, bottom, inset, 1),
                       0, GCornerNone);
    graphics_fill_rect(ctx, GRect(bounds.origin.x + bounds.size.w - inset,
                                  bottom, inset, 1), 0, GCornerNone);
  }
#else
  (void)ctx;
  (void)bounds;
#endif
}

static void paint_span(GContext *ctx, GRect rect, int boundary, GColor top, GColor bottom) {
  int end = rect.origin.y + rect.size.h;
  if (rect.origin.y < boundary) {
    int h = (end < boundary ? end : boundary) - rect.origin.y;
    graphics_context_set_fill_color(ctx, top);
    graphics_fill_rect(ctx, GRect(rect.origin.x, rect.origin.y, rect.size.w, h), 0, GCornerNone);
  }
  if (end > boundary) {
    int y = rect.origin.y > boundary ? rect.origin.y : boundary;
    graphics_context_set_fill_color(ctx, bottom);
    graphics_fill_rect(ctx, GRect(rect.origin.x, y, rect.size.w, end-y), 0, GCornerNone);
  }
}

static void draw_mask(GContext *ctx, int slot, int offset_x, int offset_y,
                      int boundary, GColor top, GColor bottom) {
  const uint8_t *data = s_masks[slot];
  size_t length = s_lengths[slot], pos = 2;
  if (length < 2 || !data[0] || !data[1]) return;
  GRect frame = s_frames[slot];
  frame.origin.x += offset_x;
  frame.origin.y += offset_y;
  int row = 0;
  // These are pre-rasterized 1-bit masks, compressed into repeated scanline runs.
  // Draw each run once, splitting only at the seconds boundary: no colour copies,
  // font processing, heap allocation, or temporary framebuffers during redraws.
  while (pos + 2 <= length && row < data[1]) {
    int repeat = data[pos++], count = data[pos++];
    if (!repeat || row + repeat > data[1] || pos + 2 * count > length) return;
    int y = frame.origin.y + row * frame.size.h / data[1];
    int end_y = frame.origin.y + (row + repeat) * frame.size.h / data[1];
    for (int i = 0; i < count; ++i) {
      int start = data[pos++], width = data[pos++];
      int x = frame.origin.x + start * frame.size.w / data[0];
      int end_x = frame.origin.x + (start + width) * frame.size.w / data[0];
      if (end_x > x && end_y > y) {
        paint_span(ctx, GRect(x, y, end_x-x, end_y-y), boundary, top, bottom);
      }
    }
    row += repeat;
  }
}

static void draw_outline_mask(GContext *ctx, int slot, int boundary,
                              GColor outline_top, GColor outline_bottom,
                              GColor center_top, GColor center_bottom) {
  static const int8_t offsets[][2] = {
    {-2, -2}, {0, -2}, {2, -2}, {-2, 0},
    {2, 0}, {-2, 2}, {0, 2}, {2, 2}
  };
  for (unsigned int i = 0; i < ARRAY_LENGTH(offsets); ++i) {
    draw_mask(ctx, slot, offsets[i][0], offsets[i][1], boundary,
              outline_top, outline_bottom);
  }
  draw_mask(ctx, slot, 0, 0, boundary, center_top, center_bottom);
}

static GColor contrast_color(GColor background) {
#ifdef PBL_COLOR
  uint8_t red = (background.argb >> 4) & 0x3;
  uint8_t green = (background.argb >> 2) & 0x3;
  uint8_t blue = background.argb & 0x3;
  return red * 299 + green * 587 + blue * 114 >= 1500
      ? GColorBlack : GColorWhite;
#else
  return background.argb == GColorWhite.argb ? GColorBlack : GColorWhite;
#endif
}

static void draw_face(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GColor upper, lower, top, bottom;
  colors(&upper, &lower, &top, &bottom);

  // The field stays at the last rendered second whenever animation is off.
  int boundary = bounds.size.h - bounds.size.h * s_second / 60;

  if (!s_backlight_on && s_idle_behavior == IDLE_INVERTED_OUTLINE) {
    // With the light off, invert the composition: use the darkest possible
    // background and retain only an outline in the scheme's lighter colors.
    // Offset copies expand each mask; repainting its center black hollows it.
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
    static const int8_t offsets[][2] = {
      {-2, -2}, {0, -2}, {2, -2}, {-2, 0},
      {2, 0}, {-2, 2}, {0, 2}, {2, 2}
    };
    for (int i = 0; i < 4; ++i) {
#ifdef PBL_COLOR
      GColor outline = static_outline_color(i < 2 ? upper : lower);
#else
      GColor outline = GColorWhite;
#endif
      for (unsigned int j = 0; j < ARRAY_LENGTH(offsets); ++j) {
        draw_mask(ctx, i, offsets[j][0], offsets[j][1], bounds.size.h,
                  outline, outline);
      }
      draw_mask(ctx, i, 0, 0, bounds.size.h, GColorBlack, GColorBlack);
    }
    mask_rounded_corners(ctx, bounds);
    return;
  }

  // fill_ratio = seconds / 60.0. Integer arithmetic gives zero fill at :00,
  // almost full at :59, and an exact reset when the next minute arrives.
  graphics_context_set_fill_color(ctx, upper);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_fill_color(ctx, lower);
  graphics_fill_rect(ctx, GRect(0, boundary, bounds.size.w, bounds.size.h-boundary), 0, GCornerNone);
  for (int i = 0; i < 4; ++i) {
    if (!s_backlight_on && s_idle_behavior == IDLE_COLOR_OUTLINE) {
      draw_outline_mask(ctx, i, boundary, contrast_color(upper),
                        contrast_color(lower), upper, lower);
      continue;
    }
#ifdef PBL_COLOR
    draw_mask(ctx, i, 0, 0, boundary, top, bottom);
#else
    draw_outline_mask(ctx, i, boundary, GColorBlack, GColorWhite,
                      upper, lower);
#endif
  }
  mask_rounded_corners(ctx, bounds);
}

static int interpolate(int a, int b) {
  // Smoothstep in fixed point for a brief, bounded 400 ms minute transition.
  int t = s_step * 256 / TRANSITION_FRAMES;
  int eased = t * t / 256 * (768 - 2*t) / 256;
  return a + (b-a) * eased / 256;
}

static void animate(void *context) {
  (void)context;
  s_timer = NULL;
  ++s_step;
  for (int i = 0; i < 4; ++i) {
    s_frames[i] = GRect(interpolate(s_from[i].origin.x, s_target[i].origin.x),
                       interpolate(s_from[i].origin.y, s_target[i].origin.y),
                       interpolate(s_from[i].size.w, s_target[i].size.w),
                       interpolate(s_from[i].size.h, s_target[i].size.h));
  }
  layer_mark_dirty(s_layer);
  if (s_step < TRANSITION_FRAMES) s_timer = app_timer_register(50, animate, NULL);
}

static void choose_layout(struct tm *now, bool transition) {
  int hour = now->tm_hour;
  if (!clock_is_24h_style()) { hour %= 12; if (!hour) hour = 12; }
  int digits[] = {hour/10, hour%10, now->tm_min/10, now->tm_min%10};
  GRect bounds = layer_get_bounds(s_layer);
  int mx = PBL_IF_ROUND_ELSE(bounds.size.w * 15 / 100, 2);
  int my = PBL_IF_ROUND_ELSE(bounds.size.h * 15 / 100, 2);
  int w = bounds.size.w - 2*mx, h = bounds.size.h - 2*my, gap = 3;
  // Coprime stepping traverses all 16 states; adjacent minutes change strongly.
  // Independent column splits keep HH over MM while stretching all four glyphs.
  int minute = now->tm_hour * 60 + now->tm_min;
  int state = (minute * 7) % NUMERAL_STATES;
  int left = (w-gap) * (22 + state * 56 / 15) / 100;
  int widths[] = {left, w-gap-left};
  for (int col = 0; col < 2; ++col) {
    int split = (h-gap) * (28 + ((minute * 5 + col * 7) % 16) * 44 / 15) / 100;
    int x = mx + (col ? left+gap : 0);
    s_target[col] = GRect(x, my, widths[col], split);
    s_target[col+2] = GRect(x, my+split+gap, widths[col], h-gap-split);
    for (int row = 0; row < 2; ++row) {
      int slot = row*2+col;
      // Select precomputed width masters by destination aspect ratio.
      int normalized = widths[col] * 80 / s_target[slot].size.h;
      int asset_state = (normalized-18) * 15 / 90;
      if (asset_state < 0) asset_state = 0;
      if (asset_state > 15) asset_state = 15;
      load_mask(slot, digits[slot], asset_state);
    }
  }
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
  for (int i = 0; i < 4; ++i) {
    s_from[i] = s_frames[i];
    if (!transition) s_frames[i] = s_target[i];
  }
  if (transition && s_backlight_on) {
    s_step = 0;
    s_timer = app_timer_register(50, animate, NULL);
  } else if (!s_backlight_on) {
    // Layout changes remain deterministic while unlit, but never animate.
    for (int i = 0; i < 4; ++i) s_frames[i] = s_target[i];
  }
}

static void update_time(struct tm *now) {
  // Preserve the last boundary while idle; minute ticks still update HH/MM.
  if (s_backlight_on) s_second = now->tm_sec > 59 ? 59 : now->tm_sec;
  int minute = now->tm_yday * 1440 + now->tm_hour * 60 + now->tm_min;
  if (minute != s_minute) {
    choose_layout(now, s_minute >= 0 && s_backlight_on);
    s_minute = minute;
  }
  layer_mark_dirty(s_layer);
}

static void tick_handler(struct tm *now, TimeUnits changed) {
  (void)changed;
  update_time(now);
}

#if defined(_PBL_API_EXISTS_backlight_service_subscribe)
static void backlight_handler(bool on) {
  if (s_backlight_on == on) return;
  s_backlight_on = on;

  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
  for (int i = 0; i < 4; ++i) s_frames[i] = s_target[i];

  // The outline is static between minute changes. Avoid waking every second
  // while the backlight is off, then restore the seconds field when it lights.
  tick_timer_service_unsubscribe();
  tick_timer_service_subscribe(on ? SECOND_UNIT : MINUTE_UNIT, tick_handler);
  time_t now = time(NULL);
  update_time(localtime(&now));
}
#endif

#if !defined(_PBL_API_EXISTS_backlight_service_subscribe)
static void shake_timeout(void *context) {
  (void)context;
  s_shake_timer = NULL;
  s_backlight_on = false;
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
  for (int i = 0; i < 4; ++i) s_frames[i] = s_target[i];
  tick_timer_service_unsubscribe();
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  time_t now = time(NULL);
  update_time(localtime(&now));
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  (void)axis;
  (void)direction;
  s_backlight_on = true;
  if (s_shake_timer) app_timer_cancel(s_shake_timer);
  s_shake_timer = app_timer_register(SHAKE_ACTIVE_MS, shake_timeout, NULL);
  tick_timer_service_unsubscribe();
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  time_t now = time(NULL);
  update_time(localtime(&now));
}
#endif

static bool parse_string_uint(const char *text, int base, uint32_t *value) {
  uint32_t result = 0;
  bool found = false;
  if (base == 16 && *text == '#') ++text;
  while (*text) {
    int digit;
    if (*text >= '0' && *text <= '9') digit = *text - '0';
    else if (base == 16 && *text >= 'a' && *text <= 'f') digit = *text - 'a' + 10;
    else if (base == 16 && *text >= 'A' && *text <= 'F') digit = *text - 'A' + 10;
    else return false;
    if (digit >= base || result > (UINT32_MAX - digit) / (uint32_t)base) return false;
    result = result * (uint32_t)base + (uint32_t)digit;
    found = true;
    ++text;
  }
  if (!found) return false;
  *value = result;
  return true;
}

static bool tuple_uint(Tuple *tuple, int string_base, uint32_t *value) {
  if (!tuple) return false;
  if (tuple->type == TUPLE_UINT || tuple->type == TUPLE_INT) {
    *value = tuple->value->uint32;
    return true;
  }
  // Clay select values and color pickers may arrive as C strings depending
  // on the companion-app/WebView version (for example "5" or "55AAFF").
  return tuple->type == TUPLE_CSTRING && tuple->length > 1 &&
         parse_string_uint(tuple->value->cstring, string_base, value);
}

static bool setting(DictionaryIterator *iterator, uint32_t key, int limit, int *value) {
  uint32_t parsed;
  if (!tuple_uint(dict_find(iterator, key), 10, &parsed) ||
      parsed >= (uint32_t)limit) return false;
  *value = (int)parsed;
  return true;
}

static bool color_setting(DictionaryIterator *iterator, uint32_t key,
                          uint32_t *value) {
  uint32_t parsed;
  if (!tuple_uint(dict_find(iterator, key), 16, &parsed) ||
      parsed > 0xFFFFFF) return false;
  *value = parsed;
  return true;
}

static void inbox_received(DictionaryIterator *iterator, void *context) {
  (void)context;
  if (setting(iterator, MESSAGE_KEY_NumeralSystem, 3, &s_system)) persist_write_int(SYSTEM_KEY, s_system);
  if (setting(iterator, MESSAGE_KEY_ColorScheme, SCHEME_COUNT, &s_scheme)) persist_write_int(SCHEME_KEY, s_scheme);
  if (color_setting(iterator, MESSAGE_KEY_CustomUpperColor, &s_custom_upper)) {
    persist_write_int(CUSTOM_UPPER_KEY, (int32_t)s_custom_upper);
  }
  if (color_setting(iterator, MESSAGE_KEY_CustomLowerColor, &s_custom_lower)) {
    persist_write_int(CUSTOM_LOWER_KEY, (int32_t)s_custom_lower);
  }
  if (setting(iterator, MESSAGE_KEY_IdleBehavior, IDLE_BEHAVIOR_COUNT,
              &s_idle_behavior)) {
    persist_write_int(IDLE_BEHAVIOR_KEY, s_idle_behavior);
  }
  if (s_layer) {
    time_t now = time(NULL);
    choose_layout(localtime(&now), false);
    layer_mark_dirty(s_layer);
  }
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_layer = layer_create(layer_get_bounds(root));
  if (!s_layer) return;
  layer_set_update_proc(s_layer, draw_face);
  layer_add_child(root, s_layer);
  time_t now = time(NULL);
  update_time(localtime(&now));
  tick_timer_service_subscribe(s_backlight_on ? SECOND_UNIT : MINUTE_UNIT,
                               tick_handler);
}

static void window_unload(Window *window) {
  (void)window;
  tick_timer_service_unsubscribe();
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
#if !defined(_PBL_API_EXISTS_backlight_service_subscribe)
  if (s_shake_timer) { app_timer_cancel(s_shake_timer); s_shake_timer = NULL; }
#endif
  if (s_layer) { layer_destroy(s_layer); s_layer = NULL; }
}

static void init(void) {
  s_system = persist_read_int(SYSTEM_KEY);
  s_scheme = persist_read_int(SCHEME_KEY);
  s_idle_behavior = persist_read_int(IDLE_BEHAVIOR_KEY);
  s_custom_upper = persist_exists(CUSTOM_UPPER_KEY)
      ? (uint32_t)persist_read_int(CUSTOM_UPPER_KEY) : DEFAULT_CUSTOM_UPPER;
  s_custom_lower = persist_exists(CUSTOM_LOWER_KEY)
      ? (uint32_t)persist_read_int(CUSTOM_LOWER_KEY) : DEFAULT_CUSTOM_LOWER;
  if (s_system < 0 || s_system > 2) s_system = 0;
  if (s_scheme < 0 || s_scheme >= SCHEME_COUNT) s_scheme = 0;
  if (s_idle_behavior < 0 || s_idle_behavior >= IDLE_BEHAVIOR_COUNT) {
    s_idle_behavior = IDLE_INVERTED_OUTLINE;
  }
#if defined(_PBL_API_EXISTS_light_is_on)
  s_backlight_on = light_is_on();
#else
  // These platforms expose no reliable backlight state. Start with the static
  // outline and show the animated face for 15 seconds after an accel tap.
  s_backlight_on = false;
#endif
  s_window = window_create();
  if (!s_window) return;
  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 64);
  window_set_window_handlers(s_window, (WindowHandlers){.load=window_load, .unload=window_unload});
  window_stack_push(s_window, false);
#if defined(_PBL_API_EXISTS_backlight_service_subscribe)
  backlight_service_subscribe(backlight_handler);
#else
  accel_tap_service_subscribe(tap_handler);
#endif
}

int main(void) {
  init();
  app_event_loop();
#if defined(_PBL_API_EXISTS_backlight_service_unsubscribe)
  backlight_service_unsubscribe();
#else
  accel_tap_service_unsubscribe();
#endif
  app_message_deregister_callbacks();
  if (s_window) window_destroy(s_window);
}
