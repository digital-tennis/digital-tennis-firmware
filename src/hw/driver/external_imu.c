#include "dt_controller.h"
#include <zephyr/drivers/sensor.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#if DT_NODE_HAS_STATUS(DT_ALIAS(external_imu), okay)
#if DT_NODE_HAS_COMPAT(DT_ALIAS(external_imu), st_lsm6ds3tr_c)
#define DT_EXTERNAL_LSM6DS3TR_C 1
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include "dt_lsm6ds3tr_c.h"
LOG_MODULE_REGISTER(external_lsm6ds3tr_c,LOG_LEVEL_INF);
static const struct i2c_dt_spec lsm_bus = I2C_DT_SPEC_GET(DT_ALIAS(external_imu));
static bool lsm_io_error;
static bool lsm_read(void *context,uint8_t reg,uint8_t *data,size_t size){
 (void)context;bool ok=i2c_burst_read_dt(&lsm_bus,reg,data,size)==0;
 if(!ok)lsm_io_error=true;
 return ok;
}
static bool lsm_write(void *context,uint8_t reg,uint8_t value){
 (void)context;return i2c_reg_write_byte_dt(&lsm_bus,reg,value)==0;
}
static void lsm_sleep(void *context,unsigned ms){(void)context;k_sleep(K_MSEC(ms));}
static const dt_lsm6ds3tr_c_bus lsm_port={NULL,lsm_read,lsm_write,lsm_sleep};
#endif
#endif
#ifdef DT_IMU_TRACE
extern void dt_imu_trace_submit(const dt_imu_sample *sample);
#endif
// The alias must point to a separately attached sensor in a reviewed overlay.
// No onboard sensor, invented register map, address or pin fallback is used.
bool dt_external_imu_read(void *context,dt_imu_sample *sample){
 (void)context;
#if DT_NODE_HAS_STATUS(DT_ALIAS(external_imu), okay)
#ifdef DT_EXTERNAL_LSM6DS3TR_C
 static bool initialized,attempted;
 static uint32_t last_attempt,last_good;
 uint32_t now=(uint32_t)k_uptime_get();
 if(!initialized){
  if(attempted && (uint32_t)(now-last_attempt)<1000)return false;
  attempted=true;last_attempt=now;lsm_io_error=false;
  initialized=i2c_is_ready_dt(&lsm_bus) && dt_lsm6ds3tr_c_init(&lsm_port);
  if(initialized)LOG_INF("LSM6DS3TR-C configured: 416Hz, 16g, 2000dps; stationary calibration required.");
  else LOG_WRN("LSM6DS3TR-C initialization failed; check model, wiring, address and supply.");
  last_good=now;
  if(!initialized)return false;
 }
 lsm_io_error=false;
 if(!dt_lsm6ds3tr_c_read(&lsm_port,(uint32_t)k_uptime_get(),sample)){
  if(lsm_io_error || (uint32_t)(now-last_good)>100)initialized=false;
  return false;
 }
 last_good=now;
#else
 const struct device *device=DEVICE_DT_GET(DT_ALIAS(external_imu));
 if(!device_is_ready(device)||sensor_sample_fetch(device)<0)return false;
 struct sensor_value acceleration[3],gyro[3];
 if(sensor_channel_get(device,SENSOR_CHAN_ACCEL_XYZ,acceleration)<0||sensor_channel_get(device,SENSOR_CHAN_GYRO_XYZ,gyro)<0)return false;
 for(int i=0;i<3;i++){sample->acceleration[i]=(float)sensor_value_to_double(&acceleration[i]);sample->gyro_dps[i]=(float)sensor_value_to_double(&gyro[i])*57.295779513f;}
 sample->time_ms=(uint32_t)k_uptime_get();
#endif
#ifdef DT_IMU_TRACE
 dt_imu_trace_submit(sample);
#endif
 return true;
#else
 (void)sample;return false;
#endif
}
