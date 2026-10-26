#ifndef DT_LSM6DS3TR_C_H
#define DT_LSM6DS3TR_C_H
#include <stdbool.h>
#include <stddef.h>
#include "dt_imu.h"

/* LSM6DS3TR-C only; a matching ID is necessary, not proof of the chip model. */
typedef struct {
 void *context;
 bool (*read)(void *, uint8_t reg, uint8_t *data, size_t size);
 bool (*write)(void *, uint8_t reg, uint8_t value);
 void (*sleep_ms)(void *, unsigned ms);
} dt_lsm6ds3tr_c_bus;
bool dt_lsm6ds3tr_c_init(const dt_lsm6ds3tr_c_bus *bus);
/* false never modifies sample; caller must not replay the last good sample. */
bool dt_lsm6ds3tr_c_read(const dt_lsm6ds3tr_c_bus *bus, uint32_t now, dt_imu_sample *sample);
#endif
