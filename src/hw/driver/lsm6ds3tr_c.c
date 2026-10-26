#include "dt_lsm6ds3tr_c.h"

/* ST DocID030071 Rev 3: WHO_AM_I, CTRL1/2/3, STATUS and OUT registers.
 * Dedicated polling adapter, not the LSM6DSO/DSL driver or its BOOT sequence. */
bool dt_lsm6ds3tr_c_init(const dt_lsm6ds3tr_c_bus *bus) {
 uint8_t value;
 if (!bus || !bus->read || !bus->write || !bus->sleep_ms) return false;
 if (!bus->read(bus->context, 0x0f, &value, 1) || value != 0x6a) return false;
 if (!bus->write(bus->context, 0x12, 0x01)) return false; /* SW_RESET */
 bool reset = false;
 for (unsigned i = 0; i < 20; i++) {
  bus->sleep_ms(bus->context, 1);
  if (!bus->read(bus->context, 0x12, &value, 1)) return false;
  if (!(value & 1)) { reset = true; break; }
 }
 if (!reset) return false;
 /* BDU + address increment; 416 Hz, +/-16 g and +/-2000 deg/s.
  * The existing application consumes samples at 100 Hz, without FIFO replay. */
 const uint8_t registers[] = {0x12, 0x10, 0x11};
 const uint8_t values[] = {0x44, 0x64, 0x6c};
 for (unsigned i = 0; i < 3; i++) {
  if (!bus->write(bus->context, registers[i], values[i]) ||
      !bus->read(bus->context, registers[i], &value, 1) || value != values[i]) return false;
 }
 return true;
}

static int32_t signed_le16(const uint8_t *p) {
 uint32_t value = (uint32_t)p[0] | ((uint32_t)p[1] << 8);
 return value >= 32768 ? (int32_t)value - 65536 : (int32_t)value;
}

bool dt_lsm6ds3tr_c_read(const dt_lsm6ds3tr_c_bus *bus, uint32_t now, dt_imu_sample *sample) {
 uint8_t status, data[12];
 if (!bus || !bus->read || !sample) return false;
 if (!bus->read(bus->context, 0x1e, &status, 1) || (status & 3) != 3) return false;
 /* Six gyro bytes followed by six acceleration bytes, in one BDU-protected burst. */
 if (!bus->read(bus->context, 0x22, data, sizeof data)) return false;
 dt_imu_sample next = {0};
 for (unsigned i = 0; i < 3; i++) {
  next.gyro_dps[i] = (float)signed_le16(data + 2*i) * .070f;
  next.acceleration[i] = (float)signed_le16(data + 6 + 2*i) * (.000488f * 9.80665f);
 }
 next.time_ms = now;
 *sample = next;
 return true;
}
