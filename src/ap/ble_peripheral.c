#include "dt_ble.h"
#include "dt_tx_queue.h"
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <errno.h>
LOG_MODULE_REGISTER(racket_ble,LOG_LEVEL_INF);
extern dt_controller dt_app_controller;
extern struct k_mutex dt_app_mutex;
static struct bt_conn *active_connection;static struct k_spinlock connection_lock;
static atomic_t revision,subscribed;static uint32_t queue_revision;static dt_tx_queue queue;
K_MUTEX_DEFINE(queue_mutex);
typedef struct {dt_packet packet;uint32_t epoch;} pending_command;
K_MSGQ_DEFINE(commands,sizeof(pending_command),8,4);
static void advertise_worker(struct k_work *work);
K_WORK_DEFINE(advertise_work,advertise_worker);
static const struct bt_uuid_128 service_uuid=BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x6d7c1000,0x9d40,0x4cb7,0xa381,0x5e908be0a001));
static const struct bt_uuid_128 tx_uuid=BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x6d7c1001,0x9d40,0x4cb7,0xa381,0x5e908be0a001));
static const struct bt_uuid_128 rx_uuid=BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x6d7c1002,0x9d40,0x4cb7,0xa381,0x5e908be0a001));
static void ccc_changed(const struct bt_gatt_attr *attr,uint16_t value){(void)attr;atomic_set(&subscribed,value==BT_GATT_CCC_NOTIFY);atomic_inc(&revision);}
static ssize_t command_write(struct bt_conn *connection,const struct bt_gatt_attr *attr,const void *buffer,uint16_t length,uint16_t offset,uint8_t flags);
BT_GATT_SERVICE_DEFINE(racket_service,
 BT_GATT_PRIMARY_SERVICE(&service_uuid),
 BT_GATT_CHARACTERISTIC(&tx_uuid.uuid,BT_GATT_CHRC_NOTIFY,BT_GATT_PERM_NONE,NULL,NULL,NULL),
 BT_GATT_CCC(ccc_changed,BT_GATT_PERM_READ_ENCRYPT|BT_GATT_PERM_WRITE_ENCRYPT),
 BT_GATT_CHARACTERISTIC(&rx_uuid.uuid,BT_GATT_CHRC_WRITE,BT_GATT_PERM_WRITE_ENCRYPT,NULL,command_write,NULL));
uint32_t dt_ble_epoch(void){return (uint32_t)atomic_get(&revision);}
static void refresh_queue(void){uint32_t current=dt_ble_epoch();if(current!=queue_revision){dt_tx_link(&queue,atomic_get(&subscribed)!=0);queue_revision=current;}}
bool dt_ble_send(const dt_packet *p,uint32_t epoch){
 k_mutex_lock(&queue_mutex,K_FOREVER);refresh_queue();
 bool queued=epoch==queue_revision&&dt_tx_enqueue(&queue,p,(uint32_t)k_uptime_get());
 k_mutex_unlock(&queue_mutex);return queued;
}
bool dt_ble_receive(dt_packet *p,uint32_t *epoch){
 pending_command entry;
 while(k_msgq_get(&commands,&entry,K_NO_WAIT)==0)if(entry.epoch==dt_ble_epoch()&&atomic_get(&subscribed)){*p=entry.packet;*epoch=entry.epoch;return true;}
 return false;
}
static ssize_t command_write(struct bt_conn *connection,const struct bt_gatt_attr *attr,const void *buffer,uint16_t length,uint16_t offset,uint8_t flags){
 (void)attr;(void)flags;
 if(offset)return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
 if(!bt_gatt_is_subscribed(connection,&racket_service.attrs[2],BT_GATT_CCC_NOTIFY)||bt_conn_get_security(connection)<BT_SECURITY_L2)return BT_GATT_ERR(BT_ATT_ERR_AUTHORIZATION);
 pending_command entry={.epoch=dt_ble_epoch()};
 if(dt_decode(buffer,length,&entry.packet)!=DT_OK||(entry.packet.type!=128&&entry.packet.type!=130))return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
 if(k_msgq_put(&commands,&entry,K_NO_WAIT))return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
 return length;
}
static struct bt_conn *connection_ref(void){k_spinlock_key_t key=k_spin_lock(&connection_lock);struct bt_conn *c=active_connection?bt_conn_ref(active_connection):NULL;k_spin_unlock(&connection_lock,key);return c;}
static void disconnected(struct bt_conn *connection,uint8_t reason){
 (void)reason;k_spinlock_key_t key=k_spin_lock(&connection_lock);struct bt_conn *old=active_connection;if(old==connection)active_connection=NULL;else old=NULL;k_spin_unlock(&connection_lock,key);if(old)bt_conn_unref(old);
 atomic_clear(&subscribed);atomic_inc(&revision);k_msgq_purge(&commands);
 k_mutex_lock(&dt_app_mutex,K_FOREVER);dt_app_controller.calibrated=dt_app_controller.calibrating=dt_app_controller.swinging=false;dt_app_controller.history_count=dt_app_controller.history_next=0;k_mutex_unlock(&dt_app_mutex);k_work_submit(&advertise_work);
}
static void connected(struct bt_conn *connection,uint8_t error){
 if(error){k_work_submit(&advertise_work);return;}
 k_spinlock_key_t key=k_spin_lock(&connection_lock);active_connection=bt_conn_ref(connection);k_spin_unlock(&connection_lock,key);
 atomic_inc(&revision);bt_conn_set_security(connection,BT_SECURITY_L2);
}
BT_CONN_CB_DEFINE(racket_callbacks)={.connected=connected,.disconnected=disconnected};
static const struct bt_data advertisement[]={BT_DATA_BYTES(BT_DATA_FLAGS,BT_LE_AD_GENERAL|BT_LE_AD_NO_BREDR),BT_DATA_BYTES(BT_DATA_UUID128_ALL,BT_UUID_128_ENCODE(0x6d7c1000,0x9d40,0x4cb7,0xa381,0x5e908be0a001))};
static const struct bt_data scan_response[]={BT_DATA(BT_DATA_NAME_COMPLETE,CONFIG_BT_DEVICE_NAME,sizeof(CONFIG_BT_DEVICE_NAME)-1)};
static int advertise(void){return bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,advertisement,ARRAY_SIZE(advertisement),scan_response,ARRAY_SIZE(scan_response));}
static void advertise_worker(struct k_work *work){(void)work;int err=advertise();if(err&&err!=-EALREADY)LOG_WRN("Advertising failed: %d",err);}
static void transmit(void *a,void *b,void *c){
 (void)a;(void)b;(void)c;uint32_t last_dropped=0,last_expired=0;
 for(;;){
  dt_packet packet;uint32_t ticket=0,epoch;
  k_mutex_lock(&queue_mutex,K_FOREVER);refresh_queue();epoch=queue_revision;bool next=dt_tx_next(&queue,(uint32_t)k_uptime_get(),&packet,&ticket);uint32_t dropped=queue.dropped,expired=queue.expired;k_mutex_unlock(&queue_mutex);
  if(dropped!=last_dropped||expired!=last_expired){LOG_WRN("TX dropped=%u expired=%u",dropped,expired);last_dropped=dropped;last_expired=expired;}
  if(next){
   struct bt_conn *link=connection_ref();uint8_t wire[20];int result=-ENOTCONN;
   if(link&&epoch==dt_ble_epoch()&&bt_conn_get_security(link)>=BT_SECURITY_L2&&bt_gatt_is_subscribed(link,&racket_service.attrs[2],BT_GATT_CCC_NOTIFY)&&dt_encode(&packet,wire,sizeof(wire))==DT_OK)result=bt_gatt_notify(link,&racket_service.attrs[2],wire,sizeof(wire));
   if(link)bt_conn_unref(link);
   k_mutex_lock(&queue_mutex,K_FOREVER);refresh_queue();if(epoch==queue_revision)dt_tx_complete(&queue,ticket,(uint32_t)k_uptime_get(),result==0);k_mutex_unlock(&queue_mutex);
  }
  k_sleep(K_MSEC(10));
 }
}
K_THREAD_DEFINE(transmit_thread,2048,transmit,NULL,NULL,NULL,7,0,0);
int dt_ble_start(void){k_mutex_lock(&queue_mutex,K_FOREVER);dt_tx_init(&queue);queue_revision=dt_ble_epoch();k_mutex_unlock(&queue_mutex);int error=bt_enable(NULL);if(error)return error;if(IS_ENABLED(CONFIG_SETTINGS)){error=settings_load_subtree("bt");if(error)return error;}return advertise();}
