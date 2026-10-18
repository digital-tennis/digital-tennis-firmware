#include "dt_ble.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(digital_tennis,LOG_LEVEL_INF);
dt_controller dt_app_controller;
K_MUTEX_DEFINE(dt_app_mutex);
extern bool dt_external_imu_read(void *context,dt_imu_sample *sample);
int main(void){
 if(!dt_controller_init(&dt_app_controller,(dt_imu_port){NULL,dt_external_imu_read},dt_motion_defaults()))return -1;
 dt_buttons_init();int error=dt_ble_start();if(error){LOG_ERR("BLE initialization failed: %d",error);return error;}
 LOG_INF("BLE link probe works without a sensor; external IMU calibration remains required for swings.");
 for(;;){
  dt_packet input,output;uint32_t epoch;
  for(unsigned i=0;i<4&&dt_ble_receive(&input,&epoch);i++){
   k_mutex_lock(&dt_app_mutex,K_FOREVER);bool reply=dt_controller_command(&dt_app_controller,&input,(uint32_t)k_uptime_get(),&output);k_mutex_unlock(&dt_app_mutex);
   if(reply)dt_ble_send(&output,epoch);
  }
  epoch=dt_ble_epoch();k_mutex_lock(&dt_app_mutex,K_FOREVER);bool emit=dt_controller_tick(&dt_app_controller,&output);k_mutex_unlock(&dt_app_mutex);if(emit)dt_ble_send(&output,epoch);
  epoch=dt_ble_epoch();k_mutex_lock(&dt_app_mutex,K_FOREVER);emit=dt_buttons_poll(&dt_app_controller,(uint32_t)k_uptime_get(),&output);k_mutex_unlock(&dt_app_mutex);if(emit)dt_ble_send(&output,epoch);
  k_sleep(K_MSEC(10));
 }
}
