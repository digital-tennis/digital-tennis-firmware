#include "dt_controller.h"
#include "dt_button.h"
#include "dt_tx_queue.h"
#include <assert.h>
#include <stdio.h>
static bool absent(void *ctx,dt_imu_sample *s){(void)ctx;(void)s;return false;}
int main(void){
 dt_controller c;assert(dt_controller_init(&c,(dt_imu_port){NULL,absent},dt_motion_physical_defaults()));
 assert(c.input_profile==DT_INPUT_PHYSICAL_MOTION);dt_packet out,probe={.version=2,.type=130,.event_id=7,.battery=255};
 assert(dt_controller_command(&c,&probe,100,&out)&&out.version==3&&out.type==131&&out.kind==2&&out.event_id==7);
 assert(!dt_controller_set_mode(&c,3)&&!dt_controller_intent(&c,3,100,&out)&&!dt_controller_button(&c,4,100,&out));
 assert(dt_controller_button_edge(&c,4,true,100,&out)&&out.version==3&&out.type==5&&out.kind==4);
 assert(dt_validate(&out)==DT_OK);assert(dt_controller_button_edge(&c,4,false,140,&out)&&out.type==6);
 c.event_id=65535;assert(dt_controller_button_edge(&c,1,true,150,&out)&&out.event_id==1);
 c.input_profile=DT_INPUT_PHYSICAL_BUTTONS;assert(dt_controller_command(&c,&probe,160,&out)&&out.kind==3&&!c.ready&&!c.calibrated);
 assert(!dt_controller_tick(&c,&out));
 dt_button b={0};assert(dt_button_edge_poll(&b,false,0)==DT_EDGE_NONE);
 assert(dt_button_edge_poll(&b,true,10)==DT_EDGE_NONE);assert(dt_button_edge_poll(&b,true,39)==DT_EDGE_NONE);assert(dt_button_edge_poll(&b,true,40)==DT_EDGE_DOWN);
 assert(dt_button_edge_poll(&b,true,200)==DT_EDGE_NONE);assert(dt_button_edge_poll(&b,false,210)==DT_EDGE_NONE);assert(dt_button_edge_poll(&b,false,240)==DT_EDGE_UP);assert(dt_button_edge_poll(&b,false,300)==DT_EDGE_NONE);
 dt_button boot={0};assert(dt_button_edge_poll(&boot,true,0)==DT_EDGE_NONE);assert(dt_button_edge_poll(&boot,false,10)==DT_EDGE_NONE);assert(dt_button_edge_poll(&boot,false,40)==DT_EDGE_NONE);assert(dt_button_edge_poll(&boot,true,50)==DT_EDGE_NONE);assert(dt_button_edge_poll(&boot,true,80)==DT_EDGE_DOWN);
 for(uint8_t type=4;type<=6;type++){
  dt_tx_queue q;dt_tx_init(&q);dt_tx_link(&q,true);dt_packet p={.version=3,.type=type,.kind=1,.event_id=1,.battery=255};uint32_t ticket;
  assert(dt_tx_enqueue(&q,&p,0));assert(!dt_tx_next(&q,201,&out,&ticket)&&q.expired==1);
 }
 puts("PASS: v3 neutral profile/probe, legacy action rejection, physical Down/Up, boot/hold/debounce, wrapping IDs and input TTL");return 0;
}
