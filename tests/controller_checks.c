#include "dt_controller.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
typedef struct{dt_imu_sample value;bool available;} fake;
static bool read_sample(void *context,dt_imu_sample *out){fake *f=context;*out=f->value;return f->available;}
static dt_packet command(uint16_t id){dt_packet p={0};p.version=2;p.type=128;p.event_id=id;p.battery=255;return p;}
int main(void){
 fake f={{{0,0,9.80665f},{1,2,3},10},false};dt_controller c;dt_packet p;
 dt_motion_config config=dt_motion_defaults();config.filter_alpha=1;
 assert(dt_controller_init(&c,(dt_imu_port){&f,read_sample},config));
 assert(!dt_controller_tick(&c,&p)&&!c.ready);dt_packet request=command(7);
 assert(dt_controller_command(&c,&request,10,&p)&&p.kind==2&&!c.calibrated);
 f.available=true;f.value.time_ms+=10;dt_controller_tick(&c,&p);request=command(8);
 assert(!dt_controller_command(&c,&request,f.value.time_ms,&p));
 assert(dt_controller_command(&c,&request,f.value.time_ms,&p)&&p.kind==1);
 for(unsigned i=0;i<64;i++){f.value.time_ms+=10;bool emitted=dt_controller_tick(&c,&p);if(i==63)assert(emitted&&p.type==129&&p.kind==0&&p.event_id==8);}
 assert(c.calibrated&&c.ready&&fabsf(c.bias[2]-3)<.001f);
 assert(dt_controller_command(&c,&request,f.value.time_ms,&p)&&p.kind==0&&!c.calibrating);
 f.value.gyro_dps[2]=403;f.value.time_ms+=250;assert(dt_controller_tick(&c,&p)&&p.type==1&&p.kind==1&&p.flags==3);
 uint8_t bytes[20];assert(dt_encode(&p,bytes,20)==DT_OK);
 f.value.time_ms+=10;assert(!dt_controller_tick(&c,&p));
 f.value.gyro_dps[2]=3;f.value.time_ms+=10;dt_controller_tick(&c,&p);
 f.value.gyro_dps[2]=-397;f.value.time_ms+=250;assert(dt_controller_tick(&c,&p)&&p.kind==2);
 for(uint8_t kind=1;kind<=8;kind++){assert(dt_controller_intent(&c,kind,f.value.time_ms,&p));assert(dt_validate(&p)==DT_OK&&p.kind==kind);}
 f.value.gyro_dps[0]=NAN;assert(dt_controller_tick(&c,&p)&&p.type==3&&!c.ready&&!c.calibrated);assert(!dt_controller_intent(&c,8,2000,&p));
 f.value.gyro_dps[0]=1;f.value.gyro_dps[2]=3;f.value.time_ms+=10;dt_controller_tick(&c,&p);request=command(9);
 assert(!dt_controller_command(&c,&request,f.value.time_ms,&p));
 f.value.time_ms+=5001;assert(dt_controller_tick(&c,&p)&&p.type==129&&p.kind==3&&!c.calibrated);
 request=command(10);assert(!dt_controller_command(&c,&request,f.value.time_ms,&p));
 f.available=false;assert(dt_controller_tick(&c,&p)&&p.type==129&&p.kind==3&&!c.calibrating);
 config.axis[0]=2;assert(!dt_controller_init(&c,(dt_imu_port){&f,read_sample},config));
 puts("PASS: unavailable sensor, calibration ACK/busy/idempotency, bias, FH/BH, latched swing, eight intents, invalid samples and mounting matrix");return 0;
}
