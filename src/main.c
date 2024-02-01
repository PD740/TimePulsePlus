#include <pebble.h>
#include "main.h"  

#define QUIET_TIME_START 22
#define QUIET_TIME_END 9

static Window *window;
static TextLayer *textlayer;
time_t next;

int buzz_intensity, buzz_interval, buzz_start;

// schedules next wake up and does current buzz
static void schedule_and_buzz() {
  
  //canceling and resubscribing to wakeup
  vibes_cancel();
  wakeup_cancel_all();
  wakeup_service_subscribe(NULL);
    
  //getting current time
  if (persist_exists(KEY_NEXT_TIME)){ //if stored time exist - read it
     next = persist_read_int(KEY_NEXT_TIME);
  } else { // otherwise, for the first time - read system time
     next  = time(NULL);  
  }
  
//   //Debug
//   struct tm *t = localtime(&next);
//   char s_time[] = "88:44:67";
//   strftime(s_time, sizeof(s_time), "%H:%M:%S", t);
//   APP_LOG(APP_LOG_LEVEL_DEBUG, "Current time = %s", s_time);  
//   APP_LOG(APP_LOG_LEVEL_DEBUG, "Buzz Interval = %d, buzz_start = %d", buzz_interval, buzz_start);
  
  // if inital call is to start at specific hour time - calculate that time
  if (buzz_start != START_IMMEDIATLY) {
    
      int period;
      switch (buzz_start){
        case START_ON_15MIN: period = 15; break;
        case START_ON_HALFHOUR: period = 30; break;
        case START_ON_HOUR: period = 60; break;
        default: period = 60; break;
      }
    
      next = (next / 60) * 60; // rounding to a minute (removing seconds)
      next = next - (next % (period * 60)) + (period * 60); // calculating exact start timing    
    
      //and after that there will be regular wakeup call
      persist_write_int(KEY_BUZZ_START, START_IMMEDIATLY);
      buzz_start = START_IMMEDIATLY;
    
  } else { // otherwise scheduling next call according to inteval
      next = next + buzz_interval*60;  
  }
  
  //buzzing
 
// Check if quiet hours apply
static time_t t;
static struct tm now;
t = time(NULL);
  now = *(localtime(&t)); 
  // APP_LOG(APP_LOG_LEVEL_INFO, "t2");
  
    if (now.tm_hour >= QUIET_TIME_END && now.tm_hour <= QUIET_TIME_START) {
	   switch(buzz_intensity){
	    case BUZZ_SHORT:
	      vibes_short_pulse();
	      break;
	    case BUZZ_LONG:
	      vibes_short_pulse();
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


static void init() {
  
  // reading stored values
  buzz_intensity = persist_exists(KEY_BUZZ_INTENSITY)? persist_read_int(KEY_BUZZ_INTENSITY) : BUZZ_DOUBLE;
  buzz_interval = persist_exists(KEY_BUZZ_INTERVAL)? persist_read_int(KEY_BUZZ_INTERVAL) : 60;
  buzz_start = persist_exists(KEY_BUZZ_START)? persist_read_int(KEY_BUZZ_START) : START_ON_HOUR;
  // START_IMMEDIATLY
  // START_ON_15MIN
  // START_ON_HALFHOUR
  // START_ON_HOUR
  
  // if it is a wake up call - buzz and reschedule
  schedule_and_buzz();
   
}

static void deinit() {
  if (window) { // if UI existed - destroy it and schedule first buzz
    persist_delete(KEY_NEXT_TIME); // upon config close delete saved time so upon 1st start new system one can be read
    app_message_deregister_callbacks();
    text_layer_destroy(textlayer);
    window_destroy(window);
    if (buzz_intensity != BUZZ_DISABLED) {
      schedule_and_buzz();  
    }
  }
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
