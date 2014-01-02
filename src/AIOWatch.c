#include <pebble.h>
#include <math.h>

#define KEY_LEVELMETER_OFFSET_X    11
#define KEY_LEVELMETER_OFFSET_Y    12

static Window *window;
static BitmapLayer *bitmap_layer_bluetooth;
static GBitmap *bm_bluetooth;
static GBitmap *bm_bluetooth_grey;

static Layer *layer_battery;
static TextLayer *text_layer_battery;
static int battery_level;
char battery_string[40];

static Layer *layer_levelmeter;
static int counter_levelmeter;
static TextLayer *text_layer_levelmeter;
static char accel_levelmeter_string[40];
static AccelData accel_data;

static int levelmeter_calibration_count;
static int levelmeter_cal_x[2];
static int levelmeter_cal_y[2];
static int levelmeter_offset_x = 0;
static int levelmeter_offset_y = 0;

static TextLayer *text_layer_date;
static char date_string[40];
static TextLayer *text_layer_time;
static char time_string[40];

static int calculate_workweek(struct tm *tick_time) {
  // calculate Jan 1 wday: Sun=0, Mon=1 ...
  int wday_jan_1 = (7 - tick_time->tm_yday % 7 + tick_time->tm_wday) % 7;
  int offset;
  if (wday_jan_1 <= 4) {
    offset = wday_jan_1 + 6;
  } else {
    offset = wday_jan_1 - 1;
  }
  int ww = (tick_time->tm_yday + offset) / 7;
  if (ww == 53) {
    int wday_jan_1_next_year = (tick_time->tm_wday + 31 - tick_time->tm_mday + 1) % 7;
    if (1 <= wday_jan_1_next_year && wday_jan_1_next_year <= 4)
      ww = 1;
  }
  return ww;
}

void battery_updateproc(struct Layer *layer, GContext *ctx) {
  //graphics_draw_rect(ctx, (GRect) { .origin = {1, 1}, .size = {38, 18} });
  
  const int LENGTH = 36;
  const int TIP_LENGTH = 4;
  const int TIP_HEIGHT = 4;
  const int SHOULDER = 4;

  GPathInfo pathinfo_outline = {
    .num_points = 4,
    .points = (GPoint []) { {0, 0}, {LENGTH, 0}, 
			    {LENGTH, 2*SHOULDER+TIP_HEIGHT}, {0, 2*SHOULDER+TIP_HEIGHT} }
  };
  GPathInfo pathinfo_tip = { 
    .num_points = 4,
    .points = (GPoint []) { {LENGTH, SHOULDER}, {LENGTH+TIP_LENGTH,SHOULDER},
			    {LENGTH+TIP_LENGTH, SHOULDER+TIP_HEIGHT}, {LENGTH, SHOULDER+TIP_HEIGHT} }
  };
  GPath *path = gpath_create(&pathinfo_outline);
  gpath_draw_outline(ctx, path);
  gpath_destroy(path);
  path = gpath_create(&pathinfo_tip);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);

  int length = LENGTH * battery_level / 100;
  GPathInfo pathinfo_fill = {
    .num_points = 4,
    .points = (GPoint []) { {0, 0}, {length, 0}, 
			    {length, 2*SHOULDER+TIP_HEIGHT}, {0, 2*SHOULDER+TIP_HEIGHT} }
  };
  path = gpath_create(&pathinfo_fill);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);

}

double levelmeter_map(double x) {
  if (x<-1)
    x = -1;
  if (x>1)
    x = 1;
  return sin(x * 3.1416 / 2);
}

// called every tick
// time 0: print "place watch on surface", accel_data_service_subscribe(0, NULL)
// time 10: take 10 accel samples and print "rotate watch 180deg"
// time 20: take 10 accel samples and calculate offset x y, accel_data_service_unsubscribe()
void levelmeter_calibration() {
  AccelData data;

  //app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "cal %d", levelmeter_calibration_count);

  if (levelmeter_calibration_count == 20) {
    accel_data_service_subscribe(0, NULL);
    text_layer_set_text(text_layer_levelmeter, "place watch");
  } else if (levelmeter_calibration_count == 10) {
    accel_service_peek(&data);
    levelmeter_cal_x[0] = data.x;
    levelmeter_cal_y[0] = data.y;
    text_layer_set_text(text_layer_levelmeter, "rotate 180deg");
  } else if (levelmeter_calibration_count == 1) {
    accel_service_peek(&data);
    accel_data_service_unsubscribe();
    levelmeter_cal_x[1] = data.x;
    levelmeter_cal_y[1] = data.y;
    levelmeter_offset_x = (levelmeter_cal_x[0] + levelmeter_cal_x[1]) / 2;
    levelmeter_offset_y = (levelmeter_cal_y[0] + levelmeter_cal_y[1]) / 2;
    persist_write_int(KEY_LEVELMETER_OFFSET_X, levelmeter_offset_x);
    persist_write_int(KEY_LEVELMETER_OFFSET_Y, levelmeter_offset_y);
    text_layer_set_text(text_layer_levelmeter, "done");  
    //app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "cal result %d %d", levelmeter_offset_x, levelmeter_offset_y);
  }
  if (levelmeter_calibration_count > 0)
    levelmeter_calibration_count--;
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  BatteryChargeState charge_state = battery_state_service_peek();
  battery_level = charge_state.charge_percent;
  layer_mark_dirty(layer_battery);

  bool bluetooth_connect = bluetooth_connection_service_peek();

  if (bluetooth_connect)
    bitmap_layer_set_bitmap(bitmap_layer_bluetooth, bm_bluetooth);
  else
    bitmap_layer_set_bitmap(bitmap_layer_bluetooth, bm_bluetooth_grey);

  snprintf(battery_string, sizeof(battery_string), "%d%%%s", 
	   charge_state.charge_percent, charge_state.is_charging? " charging": "");
  text_layer_set_text(text_layer_battery, battery_string);

  snprintf(date_string, sizeof(time_string), "%d-%02d-%02d ww%d.%d", 
	   tick_time->tm_year + 1900,	   
	   tick_time->tm_mon + 1,
	   tick_time->tm_mday, 
	   calculate_workweek(tick_time),
	   (tick_time->tm_wday == 0)? 7: tick_time->tm_wday);
  snprintf(time_string, sizeof(time_string), "%02d:%02d:%02d", 
	   tick_time->tm_hour,
	   tick_time->tm_min,
	   tick_time->tm_sec);
  text_layer_set_text(text_layer_date, date_string);
  text_layer_set_text(text_layer_time, time_string);
  
  accel_data.x = 10000;
  layer_mark_dirty(layer_levelmeter);
  levelmeter_calibration();
}

void levelmeter_updateproc2(struct Layer *layer, GContext *ctx) {
  AccelData data;
  data.x = accel_data.x - levelmeter_offset_x;
  data.y = accel_data.y - levelmeter_offset_y;
  data.z = accel_data.z;

  graphics_draw_circle(ctx, (GPoint) { .x = 20, .y = 20 }, 10);
  graphics_draw_circle(ctx, (GPoint) { .x = 20, .y = 20 }, 20);
  graphics_draw_line(ctx, (GPoint) { .x = 20, .y = 1 }, (GPoint) { .x = 20, .y = 39 });
  graphics_draw_line(ctx, (GPoint) { .x = 1, .y = 20 }, (GPoint) { .x = 39, .y = 20 });

  if (data.x < 8000) {
    graphics_fill_circle(ctx, (GPoint) { .x = 20 - 20 * levelmeter_map(data.x / 1000.0), 
	                                 .y = 20 + 20 * levelmeter_map(data.y / 1000.0) }, 3);
    snprintf(accel_levelmeter_string, sizeof(accel_levelmeter_string), "%d, %d, %d", data.x, data.y, data.z);
    text_layer_set_text(text_layer_levelmeter, accel_levelmeter_string);
    //app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, accel_levelmeter_string);
  }
}


static void accel_data_handler(AccelData *data, uint32_t num_samples) {
  counter_levelmeter--;
  if (counter_levelmeter == 0) {
    accel_data.x = 10000;
    accel_data_service_unsubscribe();
  } else {
    accel_data = *data;
  }
  layer_mark_dirty(layer_levelmeter);
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  AccelData data;
  /*
  if (ACCEL_AXIS_X == axis || ACCEL_AXIS_Y == axis) {
    accel_data_service_subscribe(0, NULL);
    accel_service_peek(&data);
    accel_data_service_unsubscribe();
  }
  if ((ACCEL_AXIS_X == axis || ACCEL_AXIS_Y == axis) && data.z > 0) {
    levelmeter_calibration_count = 20;
  } else */

  if (levelmeter_calibration_count != 0 || counter_levelmeter != 0)
    return;

  if (ACCEL_AXIS_Z == axis) {
    levelmeter_calibration_count = 20;
  } else {
    accel_data_service_subscribe(1, accel_data_handler);
    accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
    counter_levelmeter = 600;
  }
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // bluetooth bitmap
  bitmap_layer_bluetooth = bitmap_layer_create((GRect) { .origin = { 0, 20 }, .size = { 20, 20 } });
  bm_bluetooth = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH);
  bm_bluetooth_grey = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH_GREY);
  layer_add_child(window_layer, bitmap_layer_get_layer(bitmap_layer_bluetooth));

  // battery graph and text
  layer_battery = layer_create((GRect) { .origin = { 20, 22 }, .size = { 40, 16 } });
  text_layer_battery = text_layer_create((GRect) { .origin = { 65, 20 }, .size = { 80, 20 } });
  layer_set_update_proc(layer_battery, battery_updateproc);
  layer_add_child(window_layer, layer_battery);
  layer_add_child(window_layer, text_layer_get_layer(text_layer_battery));

  // accel graph and text
  layer_levelmeter = layer_create((GRect) { .origin = { bounds.size.w-41, 40 }, .size = { 41, 41 } });
  layer_add_child(window_layer, layer_levelmeter);
  text_layer_levelmeter = text_layer_create((GRect) { .origin = { 0, 40 }, .size = { bounds.size.w-41, 20 } });
  layer_add_child(window_layer, text_layer_get_layer(text_layer_levelmeter));
  layer_set_update_proc(layer_levelmeter, levelmeter_updateproc2);

  // date text
  text_layer_date = text_layer_create((GRect) { .origin = { 0, 90 }, .size = { bounds.size.w, 30 } });
  text_layer_set_font(text_layer_date, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(text_layer_date));

  // time text
  text_layer_time = text_layer_create((GRect) { .origin = { 0, 120 }, .size = { bounds.size.w, 40 } });
  text_layer_set_font(text_layer_time, fonts_get_system_font(FONT_KEY_DROID_SERIF_28_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(text_layer_time));
}

static void window_unload(Window *window) {
  bitmap_layer_destroy(bitmap_layer_bluetooth);
  gbitmap_destroy(bm_bluetooth);
  gbitmap_destroy(bm_bluetooth_grey);

  layer_destroy(layer_battery);
  text_layer_destroy(text_layer_battery);
  
  layer_destroy(layer_levelmeter);

  text_layer_destroy(text_layer_date);
  text_layer_destroy(text_layer_time);
}

static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
  accel_tap_service_subscribe(accel_tap_handler);

  const bool animated = true;
  window_stack_push(window, animated);

  if (persist_exists(KEY_LEVELMETER_OFFSET_X))
    levelmeter_offset_x = persist_read_int(KEY_LEVELMETER_OFFSET_X);
  if (persist_exists(KEY_LEVELMETER_OFFSET_Y))
    levelmeter_offset_y = persist_read_int(KEY_LEVELMETER_OFFSET_Y);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
