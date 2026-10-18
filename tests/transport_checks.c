#include "dt_tx_queue.h"
#include "dt_button.h"
#include <assert.h>
#include <stdio.h>
static dt_packet swing(uint16_t id){dt_packet p={0};p.version=2;p.type=1;p.flags=3;p.kind=1;p.event_id=id;p.power=50;p.battery=255;return p;}
int main(void){
 dt_tx_queue q;dt_tx_init(&q);dt_packet p=swing(7),out;uint32_t ticket;
 assert(!dt_tx_enqueue(&q,&p,0));dt_tx_link(&q,true);assert(dt_tx_enqueue(&q,&p,10));
 for(unsigned i=0;i<3;i++){assert(dt_tx_next(&q,10+20*i,&out,&ticket));assert(out.event_id==7&&out.sequence==i+1&&out.device_ms==p.device_ms);dt_tx_complete(&q,ticket,10+20*i,true);}
 assert(!q.count&&!dt_tx_next(&q,80,&out,&ticket));
 assert(dt_tx_enqueue(&q,&p,100));assert(dt_tx_next(&q,100,&out,&ticket));dt_tx_complete(&q,ticket,100,false);assert(q.count==1);assert(!dt_tx_next(&q,110,&out,&ticket));assert(dt_tx_next(&q,120,&out,&ticket));
 dt_tx_link(&q,false);dt_tx_complete(&q,ticket,120,true);assert(!q.count&&!dt_tx_enqueue(&q,&p,121));dt_tx_link(&q,true);assert(!dt_tx_next(&q,130,&out,&ticket));
 assert(dt_tx_enqueue(&q,&p,140));assert(!dt_tx_next(&q,341,&out,&ticket)&&q.expired==1);
 for(unsigned i=0;i<DT_TX_CAPACITY;i++){p.event_id=(uint16_t)i;assert(dt_tx_enqueue(&q,&p,400));}
 assert(dt_tx_next(&q,400,&out,&ticket));uint32_t leased=ticket;p.event_id=99;assert(dt_tx_enqueue(&q,&p,400)&&q.dropped==1);dt_tx_complete(&q,leased,400,true);assert(q.count==DT_TX_CAPACITY); // concurrent overflow cannot complete another entry
 dt_tx_link(&q,true);q.sequence=65535;p=swing(88);assert(dt_tx_enqueue(&q,&p,0xfffffff0));assert(dt_tx_next(&q,0xfffffff0,&out,&ticket)&&out.sequence==0);dt_tx_complete(&q,ticket,0xfffffff0,true);assert(dt_tx_next(&q,4,&out,&ticket)); // clocks and sequence wrap
 dt_button b={0};assert(!dt_button_poll(&b,true,0));assert(!dt_button_poll(&b,true,1000));assert(!dt_button_poll(&b,false,1010));assert(!dt_button_poll(&b,false,1040));
 assert(!dt_button_poll(&b,true,1050));assert(!dt_button_poll(&b,false,1055));assert(!dt_button_poll(&b,true,1060));assert(!dt_button_poll(&b,true,1089));assert(dt_button_poll(&b,true,1090));assert(!dt_button_poll(&b,true,2090));
 assert(!dt_button_poll(&b,false,2100));assert(!dt_button_poll(&b,false,2130));assert(!dt_button_poll(&b,true,2140));assert(dt_button_poll(&b,true,2170));
 dt_button wrapped={0};assert(!dt_button_poll(&wrapped,false,0xffffffc0));assert(!dt_button_poll(&wrapped,true,0xfffffff0));assert(dt_button_poll(&wrapped,true,14));
 puts("PASS: bounded TX queue, stable event retransmission, backpressure, expiry/overflow/in-flight race, connection/clock/sequence reset; button debounce/boot-held/hold/wrap");return 0;
}
