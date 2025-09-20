#include <pebble.h>
#include "main.h"
#include <string.h>

static Window *window;
static TextLayer *textlayer;

GRect bounds;
time_t next;

int buzz_intensity, buzz_interval, buzz_start, quiet_time_start, quiet_time_end;
int menu_position = 0;

static int get_interval_minutes(int interval_setting)
{
  switch (interval_setting)
  {
  case 15:
    return 15;
  case 30:
    return 30;
  case 60:
    return 60;
  case BUZZ_INTERVAL_15_PLUS:
    return 15;
  default:
    return 60;
  }
}

static int compute_15_plus_buzz_count(time_t event_time)
{
  struct tm *event_tm = localtime(&event_time);
  if (!event_tm)
  {
    return 1;
  }

  int minute = event_tm->tm_min % 60;
  switch (minute)
  {
  case 0:
    return 4;
  case 15:
    return 1;
  case 30:
    return 2;
  case 45:
    return 3;
  default:
    return 1;
  }
}

static void trigger_single_buzz()
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
  default:
    break;
  }
}

static void perform_buzz_sequence(int count)
{
  if (buzz_intensity == BUZZ_DISABLED || count <= 0)
  {
    return;
  }

  for (int i = 0; i < count; i++)
  {
    trigger_single_buzz();
    if (i < count - 1)
    {
      psleep(1000);
    }
  }
}

static void saveConfig()
{
  persist_write_int(KEY_BUZZ_INTENSITY, buzz_intensity);
  persist_write_int(KEY_BUZZ_INTERVAL, buzz_interval);
  persist_write_int(KEY_QUIET_TIME_START, quiet_time_start);
  persist_write_int(KEY_QUIET_TIME_END, quiet_time_end);
  persist_write_int(KEY_BUZZ_START, COMPUTE_NEW_TIME);
  buzz_start = COMPUTE_NEW_TIME;
}

int isQuietTime(int startHour, int endHour)
{
  time_t currentTime;
  struct tm *localTime;

  // Obtenir l'heure actuelle
  currentTime = time(NULL);
  localTime = localtime(&currentTime);

  int currentHour = localTime->tm_hour;

  // Vérifier si la "quiet time" traverse minuit
  if (startHour <= endHour)
  {
    // Cas normal, pas de traversée de minuit
    if (currentHour >= startHour && currentHour < endHour)
    {
      return 1; // Dans la "quiet time"
    }
    else
    {
      return 0; // En dehors de la "quiet time"
    }
  }
  else
  {
    // Cas où la "quiet time" traverse minuit
    if (currentHour >= startHour || currentHour < endHour)
    {
      return 1; // Dans la "quiet time"
    }
    else
    {
      return 0; // En dehors de la "quiet time"
    }
  }
}

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

  time_t event_time = next;
  int interval_minutes = get_interval_minutes(buzz_interval);

  // Debug
  struct tm *t = localtime(&next);
  char s_time[] = "88:44:67";
  strftime(s_time, sizeof(s_time), "%H:%M:%S", t);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Current time = %s", s_time);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Buzz Interval = %d, buzz_start = %d", buzz_interval, buzz_start);

  // if inital call is to start at specific hour time - calculate that time
  if (buzz_start == COMPUTE_NEW_TIME)
  {
    next = (next / 60) * 60;                              // rounding to a minute (removing seconds)
    next = next - (next % (interval_minutes * 60)) + (interval_minutes * 60); // calculating exact start timing

    // and after that there will be regular wakeup call
    persist_write_int(KEY_BUZZ_START, START_IMMEDIATLY);
    buzz_start = START_IMMEDIATLY;
  }
  else
  { // otherwise scheduling next call according to inteval
    next = next + (interval_minutes * 60);
  }

  // Check if quiet hours apply
  if (!isQuietTime(quiet_time_start, quiet_time_end))
  {
    if (buzz_interval == BUZZ_INTERVAL_15_PLUS)
    {
      int buzz_count = compute_15_plus_buzz_count(event_time);
      perform_buzz_sequence(buzz_count);
    }
    else
    {
      perform_buzz_sequence(1);
    }
  }

  // debug
  t = localtime(&next);
  strftime(s_time, sizeof(s_time), "%H:%M:%S", t);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Next time = %s", s_time);
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
      buzz_start = COMPUTE_NEW_TIME;
      schedule_and_buzz();
    }
  }
}

static void drawOptions()
{

  // title
  const char *initialMessage = "TIME PULSE";
  char *finalMessage = (char *)malloc(500);
  strcpy(finalMessage, initialMessage);

  // Enabled
  strcat(finalMessage, "\n\n");
  if (menu_position == 0)
    strcat(finalMessage, "->");
  strcat(finalMessage, "Enabled : ");

  if (buzz_intensity == BUZZ_DISABLED)
    strcat(finalMessage, "No\n");
  else
    strcat(finalMessage, "Yes\n");

  // quiet start
  if (menu_position == 1)
    strcat(finalMessage, "-> ");
  strcat(finalMessage, "Quiet start : ");
  char maChaine[20];
  snprintf(maChaine, sizeof(maChaine), "%d", quiet_time_start);
  strcat(finalMessage, maChaine);
  strcat(finalMessage, "\n");

  // quiet end
  if (menu_position == 2)
    strcat(finalMessage, "-> ");
  maChaine[0] = '\0';
  strcat(finalMessage, "Quiet end : ");
  snprintf(maChaine, sizeof(maChaine), "%d", quiet_time_end);
  strcat(finalMessage, maChaine);
  strcat(finalMessage, "\n");

  // buzzer type
  if (menu_position == 3)
    strcat(finalMessage, "-> ");
  strcat(finalMessage, "Buzz : ");

  switch (buzz_intensity)
  {
  case BUZZ_DISABLED:
    strcat(finalMessage, "No");
    break;
  case BUZZ_SHORT:
    strcat(finalMessage, "Short");
    break;
  case BUZZ_LONG:
    strcat(finalMessage, "Long");
    break;
  case BUZZ_DOUBLE:
    strcat(finalMessage, "Double");
    break;
  }
  strcat(finalMessage, "\n");

  // Frequency
  if (menu_position == 4)
    strcat(finalMessage, "-> ");
  strcat(finalMessage, "Every : ");
  switch (buzz_interval)
  {
  case 15:
    strcat(finalMessage, "15mn");
    break;
  case 30:
    strcat(finalMessage, "30mn");
    break;
  case 60:
    strcat(finalMessage, "Hour");
    break;
  case BUZZ_INTERVAL_15_PLUS:
    strcat(finalMessage, "15mn+");
    break;
  default:
    buzz_interval = 60;
    strcat(finalMessage, "Hour");
    break;
  }
  strcat(finalMessage, "\n\n");

  if (menu_position == 5)
    strcat(finalMessage, "-> ");
  strcat(finalMessage, "SAVE & EXIT");

  text_layer_set_text(textlayer, finalMessage);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context)
{
  if (menu_position > 0)
    menu_position--;
  drawOptions();
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context)
{
  if (menu_position < 5)
    menu_position++;
  drawOptions();
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context)
{
  switch (menu_position)
  {
  case 0:
    if (buzz_intensity == BUZZ_DISABLED)
      buzz_intensity = BUZZ_DOUBLE;
    else
      buzz_intensity = BUZZ_DISABLED;
    break;
  case 1:
    if (quiet_time_start < 23)
      quiet_time_start++;
    else
      quiet_time_start = 0;
    break;
  case 2:
    if (quiet_time_end < 23)
      quiet_time_end++;
    else
      quiet_time_end = 0;
    break;
  case 3:
    if (buzz_intensity < 3)
      buzz_intensity++;
    else
      buzz_intensity = 0;
    break;
  case 4:

    if (buzz_interval == 15)
      buzz_interval = 30;
    else if (buzz_interval == 30)
      buzz_interval = 60;
    else if (buzz_interval == 60)
      buzz_interval = BUZZ_INTERVAL_15_PLUS;
    else if (buzz_interval == BUZZ_INTERVAL_15_PLUS)
      buzz_interval = 15;
    else
      buzz_interval = 60;

    break;
  case 5:
    saveConfig();
    window_stack_pop(false);
    break;
  }

  drawOptions();
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
  buzz_start = persist_exists(KEY_BUZZ_START) ? persist_read_int(KEY_BUZZ_START) : COMPUTE_NEW_TIME;
  quiet_time_start = persist_exists(KEY_QUIET_TIME_START) ? persist_read_int(KEY_QUIET_TIME_START) : 22;
  quiet_time_end = persist_exists(KEY_QUIET_TIME_END) ? persist_read_int(KEY_QUIET_TIME_END) : 9;

  // if it's not a wake up call, meaning we're launched from Config - display current config
  if (launch_reason() != APP_LAUNCH_WAKEUP)
  {

    window = window_create();
    window_set_background_color(window, GColorBlack);
    //       #ifdef PBL_PLATFORM_APLITE
    //         window_set_fullscreen(window, true);
    //       #endif

    bounds = layer_get_bounds(window_get_root_layer(window));
    window_set_click_config_provider(window, click_config_provider);

    textlayer = text_layer_create(GRect(bounds.origin.x + 14, bounds.origin.y, bounds.size.w, bounds.size.h));

    text_layer_set_font(textlayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_background_color(textlayer, GColorBlack);
    text_layer_set_text_color(textlayer, GColorWhite);
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(textlayer));

    window_stack_push(window, false);

    drawOptions();
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
