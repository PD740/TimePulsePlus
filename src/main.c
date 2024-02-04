#include <pebble.h>
#include "main.h"

#define QUIET_TIME_START 22
#define QUIET_TIME_END 9

static Window *window;
static TextLayer *textlayer;
time_t next;

int buzz_intensity, buzz_interval, buzz_start;

// schedules next wake up and does current buzz
static void schedule_and_buzz()
{

  // canceling and resubscribing to wakeup
  vibes_cancel();
  wakeup_cancel_all();
  wakeup_service_subscribe(NULL);

  // getting current time
  if (persist_exists(KEY_NEXT_TIME))
  { // if stored time exist - read it
    next = persist_read_int(KEY_NEXT_TIME);
  }
  else
  { // otherwise, for the first time - read system time
    next = time(NULL);
  }

  //   //Debug
  //   struct tm *t = localtime(&next);
  //   char s_time[] = "88:44:67";
  //   strftime(s_time, sizeof(s_time), "%H:%M:%S", t);
  //   APP_LOG(APP_LOG_LEVEL_DEBUG, "Current time = %s", s_time);
  //   APP_LOG(APP_LOG_LEVEL_DEBUG, "Buzz Interval = %d, buzz_start = %d", buzz_interval, buzz_start);

  // if inital call is to start at specific hour time - calculate that time
  if (buzz_start != START_IMMEDIATLY)
  {

    int period;
    switch (buzz_start)
    {
    case START_ON_15MIN:
      period = 15;
      break;
    case START_ON_HALFHOUR:
      period = 30;
      break;
    case START_ON_HOUR:
      period = 60;
      break;
    default:
      period = 60;
      break;
    }

    next = (next / 60) * 60;                              // rounding to a minute (removing seconds)
    next = next - (next % (period * 60)) + (period * 60); // calculating exact start timing

    // and after that there will be regular wakeup call
    persist_write_int(KEY_BUZZ_START, START_IMMEDIATLY);
    buzz_start = START_IMMEDIATLY;
  }
  else
  { // otherwise scheduling next call according to inteval
    next = next + buzz_interval * 60;
  }

  // Check if quiet hours apply
  time_t t;
  struct tm now;
  t = time(NULL);
  now = *(localtime(&t));

  if (now.tm_hour >= 9 && now.tm_hour <= 22)
  {
    switch (buzz_intensity)
    {
    case BUZZ_SHORT:
      vibes_short_pulse();
      break;
    case BUZZ_LONG:
      vibes_long_pulse();
      break;
    case BUZZ_DOUBLE:
      vibes_double_pulse();
      break;
    }
  }

  //   //debug
  //   t = localtime(&next);
  //   strftime(s_time, sizeof(s_time), "%H:%M:%S", t);
  //   strftime(s_time, sizeof(s_time), "%H:%M:%S", t);
  //   APP_LOG(APP_LOG_LEVEL_DEBUG, "Next time = %s", s_time);

  // scheduling next wakeup
  persist_write_int(KEY_NEXT_TIME, next);
  wakeup_schedule(next, 0, false);
}

static void deinit()
{
  if (window)
  {                                // if UI existed - destroy it and schedule first buzz
    persist_delete(KEY_NEXT_TIME); // upon config close delete saved time so upon 1st start new system one can be read
    app_message_deregister_callbacks();
    text_layer_destroy(textlayer);
    window_destroy(window);
    if (buzz_intensity != BUZZ_DISABLED)
    {
      schedule_and_buzz();
    }
  }
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context)
{
  schedule_and_buzz();
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context)
{
  schedule_and_buzz();
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context)
{
  schedule_and_buzz();
}

static void back_click_handler(ClickRecognizerRef recognizer, void *context)
{

  // close window
  window_stack_pop(false);
}

static void click_config_provider(void *context)
{
  // Register the ClickHandlers
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
}

static void init()
{

  // reading stored values
  buzz_intensity = persist_exists(KEY_BUZZ_INTENSITY) ? persist_read_int(KEY_BUZZ_INTENSITY) : BUZZ_DOUBLE;
  buzz_interval = persist_exists(KEY_BUZZ_INTERVAL) ? persist_read_int(KEY_BUZZ_INTERVAL) : 60;
  buzz_start = persist_exists(KEY_BUZZ_START) ? persist_read_int(KEY_BUZZ_START) : START_ON_HOUR;
  // START_IMMEDIATLY
  // START_ON_15MIN
  // START_ON_HALFHOUR
  // START_ON_HOUR

  // if it's not a wake up call, meaning we're launched from Config - display current config
  if (launch_reason() != APP_LAUNCH_WAKEUP)
  {

    window = window_create();
    window_set_background_color(window, GColorBlack);
    //       #ifdef PBL_PLATFORM_APLITE
    //         window_set_fullscreen(window, true);
    //       #endif

    GRect bounds = layer_get_bounds(window_get_root_layer(window));
    window_set_click_config_provider(window, click_config_provider);

#ifdef PBL_RECT
    textlayer = text_layer_create(GRect(bounds.origin.x + 10, bounds.origin.y + 20, bounds.size.w - 20, bounds.size.h - 20));
#else
    textlayer = text_layer_create(GRect(bounds.origin.x + 15, bounds.origin.y + 35, bounds.size.w - 30, bounds.size.h - 30));
#endif
    text_layer_set_font(textlayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_background_color(textlayer, GColorBlack);
    text_layer_set_text_color(textlayer, GColorWhite);

    text_layer_set_text(textlayer, "Please configure the Nag on your phone and tap 'Set' to start the app. This screen will automatically disppear.");
    text_layer_set_text_alignment(textlayer, GTextAlignmentLeft);
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(textlayer));

    window_stack_push(window, false);
  }
  else
  { // if it is a wake up call - buzz and reschedule
    schedule_and_buzz();
  }
}

int main(void)
{
  init();
  app_event_loop();
  deinit();
}
