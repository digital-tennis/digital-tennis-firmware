#ifndef DT_BLE_H
#define DT_BLE_H
#include "dt_controller.h"
int dt_ble_start(void);
uint32_t dt_ble_epoch(void);
bool dt_ble_receive(dt_packet *packet,uint32_t *epoch);
bool dt_ble_send(const dt_packet *packet,uint32_t epoch);
void dt_buttons_init(void);
bool dt_buttons_poll(dt_controller *controller,uint32_t now,dt_packet *packet);
#endif
