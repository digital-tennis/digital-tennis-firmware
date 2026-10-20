#include "dt_ble.h"
#include "dt_sample_clock.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(digital_tennis,LOG_LEVEL_INF);
dt_controller dt_app_controller;
K_MUTEX_DEFINE(dt_app_mutex);
extern bool dt_external_imu_read(void *context,dt_imu_sample *sample);
int main(void){
 if(!dt_controller_init(&dt_app_controller,(dt_imu_port){NULL,dt_external_imu_read},dt_motion_peak_defaults()))return -1;
 #ifdef DT_BUTTON_PROFILE
 dt_app_controller.input_profile=DT_INPUT_BUTTONS;
 LOG_INF("Button profile: SW1 serve, SW2 forehand, SW3 backhand, SW4 skill; IMU unused.");
 #endif
 dt_buttons_init();int error=dt_ble_start();if(error){LOG_ERR("BLE initialization failed: %d",error);return error;}
 LOG_INF("BLE link probe works without a sensor; external IMU calibration remains required for swings.");
 dt_sample_clock sample_clock={0};
 for(;;){
  dt_packet input,output;uint32_t epoch;
  for(unsigned i=0;i<4&&dt_ble_receive(&input,&epoch);i++){
   k_mutex_lock(&dt_app_mutex,K_FOREVER);bool reply=dt_controller_command(&dt_app_controller,&input,(uint32_t)k_uptime_get(),&output);k_mutex_unlock(&dt_app_mutex);
   if(reply)dt_ble_send(&output,epoch);
  }
  bool emit=false;
  if(dt_sample_due(&sample_clock,(uint32_t)k_uptime_get())){
   epoch=dt_ble_epoch();k_mutex_lock(&dt_app_mutex,K_FOREVER);emit=dt_controller_tick(&dt_app_controller,&output);k_mutex_unlock(&dt_app_mutex);if(emit)dt_ble_send(&output,epoch);
  }
  epoch=dt_ble_epoch();k_mutex_lock(&dt_app_mutex,K_FOREVER);emit=dt_buttons_poll(&dt_app_controller,(uint32_t)k_uptime_get(),&output);k_mutex_unlock(&dt_app_mutex);if(emit)dt_ble_send(&output,epoch);
  k_sleep(K_MSEC(1));
 }
}
