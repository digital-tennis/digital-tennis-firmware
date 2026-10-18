#include "dt_tx_queue.h"
#include <string.h>
static void pop(dt_tx_queue *q){q->head=(q->head+1)%DT_TX_CAPACITY;q->count--;}
void dt_tx_init(dt_tx_queue *q){memset(q,0,sizeof(*q));}
void dt_tx_link(dt_tx_queue *q,bool linked){q->linked=linked;q->head=q->count=0;q->sequence=0;}
bool dt_tx_enqueue(dt_tx_queue *q,const dt_packet *p,uint32_t now){
 if(!q||!p||!q->linked||dt_validate(p)!=DT_OK)return false;
 if(q->count==DT_TX_CAPACITY){pop(q);q->dropped++;}
 dt_tx_entry *e=&q->entries[(q->head+q->count++)%DT_TX_CAPACITY];
 *e=(dt_tx_entry){.packet=*p,.queued_at=now,.due_at=now,.ticket=++q->next_ticket,.remaining=p->type==3?1:3};
 return true;
}
bool dt_tx_next(dt_tx_queue *q,uint32_t now,dt_packet *p,uint32_t *ticket){
 if(!q||!q->linked||!p||!ticket)return false;
 while(q->count){
  dt_tx_entry *e=&q->entries[q->head];uint32_t ttl=e->packet.type==1||e->packet.type==2?200:1000;
  if((uint32_t)(now-e->queued_at)>ttl){pop(q);q->expired++;continue;}
  if((int32_t)(now-e->due_at)<0)return false;
  *p=e->packet;p->sequence=++q->sequence;*ticket=e->ticket;e->due_at=now+20;return true;
 }
 return false;
}
void dt_tx_complete(dt_tx_queue *q,uint32_t ticket,uint32_t now,bool sent){
 if(!q||!q->linked||!q->count||q->entries[q->head].ticket!=ticket)return;
 dt_tx_entry *e=&q->entries[q->head];e->due_at=now+20;
 if(sent&&!--e->remaining)pop(q);
}
