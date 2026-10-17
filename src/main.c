#include "dt_controller.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <errno.h>
LOG_MODULE_REGISTER(digital_tennis,LOG_LEVEL_INF);
dt_controller dt_app_controller;
K_MUTEX_DEFINE(dt_app_mutex);
extern bool dt_external_imu_read(void *context,dt_imu_sample *sample);
extern int dt_ble_start(void);
extern int dt_ble_notify(const dt_packet *packet);
int main(void){
 if(!dt_controller_init(&dt_app_controller,(dt_imu_port){NULL,dt_external_imu_read},dt_motion_defaults()))return -1;
 int error=dt_ble_start();if(error){LOG_ERR("BLE initialization failed: %d",error);return error;}
 LOG_INF("External IMU overlay required; sensor readiness and calibration are not assumed.");
 for(;;){dt_packet output;k_mutex_lock(&dt_app_mutex,K_FOREVER);bool emit=dt_controller_tick(&dt_app_controller,&output);k_mutex_unlock(&dt_app_mutex);if(emit){int result=dt_ble_notify(&output);if(result&&result!=-ENOTCONN)LOG_WRN("Notify rejected: %d",result);}k_sleep(K_MSEC(10));}
}
