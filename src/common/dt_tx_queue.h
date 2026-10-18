#ifndef DT_TX_QUEUE_H
#define DT_TX_QUEUE_H
#include "dt_protocol.h"
#include <stdbool.h>
#define DT_TX_CAPACITY 32
typedef struct { dt_packet packet; uint32_t queued_at, due_at, ticket; uint8_t remaining; } dt_tx_entry;
typedef struct {
 dt_tx_entry entries[DT_TX_CAPACITY]; unsigned head,count;
 uint32_t next_ticket,dropped,expired; uint16_t sequence; bool linked;
} dt_tx_queue;
void dt_tx_init(dt_tx_queue *queue);
void dt_tx_link(dt_tx_queue *queue,bool linked);
bool dt_tx_enqueue(dt_tx_queue *queue,const dt_packet *packet,uint32_t now);
bool dt_tx_next(dt_tx_queue *queue,uint32_t now,dt_packet *packet,uint32_t *ticket);
void dt_tx_complete(dt_tx_queue *queue,uint32_t ticket,uint32_t now,bool sent);
#endif
