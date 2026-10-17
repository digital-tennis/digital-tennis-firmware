#include "dt_controller.h"
#include <zephyr/drivers/sensor.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
// The alias must point to a separately attached sensor in a reviewed overlay.
// No onboard sensor, invented register map, address or pin fallback is used.
bool dt_external_imu_read(void *context,dt_imu_sample *sample){
 (void)context;
#if DT_NODE_HAS_STATUS(DT_ALIAS(external_imu), okay)
 const struct device *device=DEVICE_DT_GET(DT_ALIAS(external_imu));
 if(!device_is_ready(device)||sensor_sample_fetch(device)<0)return false;
 struct sensor_value acceleration[3],gyro[3];
 if(sensor_channel_get(device,SENSOR_CHAN_ACCEL_XYZ,acceleration)<0||sensor_channel_get(device,SENSOR_CHAN_GYRO_XYZ,gyro)<0)return false;
 for(int i=0;i<3;i++){sample->acceleration[i]=(float)sensor_value_to_double(&acceleration[i]);sample->gyro_dps[i]=(float)sensor_value_to_double(&gyro[i])*57.295779513f;}
 sample->time_ms=(uint32_t)k_uptime_get();return true;
#else
 (void)sample;return false;
#endif
}
