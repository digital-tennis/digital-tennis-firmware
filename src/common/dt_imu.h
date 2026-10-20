#ifndef DT_IMU_H
#define DT_IMU_H
#include <stdint.h>
typedef struct {float acceleration[3],gyro_dps[3];uint32_t time_ms;} dt_imu_sample;
#endif
