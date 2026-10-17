#include "dt_controller.h"
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <errno.h>
extern dt_controller dt_app_controller;
extern struct k_mutex dt_app_mutex;
static bool subscribed;
static void advertise_worker(struct k_work *work);
K_WORK_DEFINE(advertise_work,advertise_worker);
static const struct bt_uuid_128 service_uuid=BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x6d7c1000,0x9d40,0x4cb7,0xa381,0x5e908be0a001));
static const struct bt_uuid_128 tx_uuid=BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x6d7c1001,0x9d40,0x4cb7,0xa381,0x5e908be0a001));
static const struct bt_uuid_128 rx_uuid=BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x6d7c1002,0x9d40,0x4cb7,0xa381,0x5e908be0a001));
static void ccc_changed(const struct bt_gatt_attr *attr,uint16_t value){(void)attr;subscribed=value==BT_GATT_CCC_NOTIFY;}
static ssize_t command_write(struct bt_conn *connection,const struct bt_gatt_attr *attr,const void *buffer,uint16_t length,uint16_t offset,uint8_t flags);
BT_GATT_SERVICE_DEFINE(racket_service,
 BT_GATT_PRIMARY_SERVICE(&service_uuid),
 BT_GATT_CHARACTERISTIC(&tx_uuid.uuid,BT_GATT_CHRC_NOTIFY,BT_GATT_PERM_NONE,NULL,NULL,NULL),
 BT_GATT_CCC(ccc_changed,BT_GATT_PERM_READ_ENCRYPT|BT_GATT_PERM_WRITE_ENCRYPT),
 BT_GATT_CHARACTERISTIC(&rx_uuid.uuid,BT_GATT_CHRC_WRITE,BT_GATT_PERM_WRITE_ENCRYPT,NULL,command_write,NULL));
int dt_ble_notify(const dt_packet *packet){uint8_t bytes[20];if(!subscribed)return -ENOTCONN;if(dt_encode(packet,bytes,20)!=DT_OK)return -EINVAL;return bt_gatt_notify(NULL,&racket_service.attrs[2],bytes,20);}
static ssize_t command_write(struct bt_conn *connection,const struct bt_gatt_attr *attr,const void *buffer,uint16_t length,uint16_t offset,uint8_t flags){
 (void)connection;(void)attr;(void)flags;
 if(offset)return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
 dt_packet command,response;if(dt_decode(buffer,length,&command)!=DT_OK||command.type!=128)return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
 k_mutex_lock(&dt_app_mutex,K_FOREVER);bool reply=dt_controller_command(&dt_app_controller,&command,(uint32_t)k_uptime_get(),&response);k_mutex_unlock(&dt_app_mutex);
 if(reply)dt_ble_notify(&response);return length;
}
static void disconnected(struct bt_conn *connection,uint8_t reason){(void)connection;(void)reason;subscribed=false;k_mutex_lock(&dt_app_mutex,K_FOREVER);dt_app_controller.calibrated=false;dt_app_controller.calibrating=false;dt_app_controller.history_count=dt_app_controller.history_next=0;k_mutex_unlock(&dt_app_mutex);k_work_submit(&advertise_work);}
static void connected(struct bt_conn *connection,uint8_t error){if(!error)bt_conn_set_security(connection,BT_SECURITY_L2);}
BT_CONN_CB_DEFINE(racket_callbacks)={.connected=connected,.disconnected=disconnected};
static int advertise(void){static const struct bt_data data[]={BT_DATA_BYTES(BT_DATA_FLAGS,BT_LE_AD_GENERAL|BT_LE_AD_NO_BREDR),BT_DATA_BYTES(BT_DATA_UUID128_ALL,BT_UUID_128_ENCODE(0x6d7c1000,0x9d40,0x4cb7,0xa381,0x5e908be0a001))};return bt_le_adv_start(BT_LE_ADV_CONN_NAME,data,ARRAY_SIZE(data),NULL,0);}
static void advertise_worker(struct k_work *work){(void)work;advertise();}
int dt_ble_start(void){int error=bt_enable(NULL);return error?error:advertise();}
