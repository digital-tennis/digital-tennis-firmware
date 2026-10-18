#include "dt_controller.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct{dt_imu_sample value;bool available;} fake;
static bool read_sample(void *context,dt_imu_sample *out){fake *f=context;*out=f->value;return f->available;}
static dt_packet command(uint8_t type,uint16_t id){dt_packet p={0};p.version=2;p.type=type;p.event_id=id;p.battery=255;return p;}
static bool step(dt_controller *c,fake *f,float y,float z,dt_packet *p){f->value.time_ms+=10;f->value.gyro_dps[1]=y;f->value.gyro_dps[2]=z;return dt_controller_tick(c,p);}
static void quiet(dt_controller *c,fake *f){dt_packet p;for(unsigned i=0;i<25;i++)step(c,f,0,0,&p);}
static bool stroke(dt_controller *c,fake *f,float y,float z,unsigned samples,dt_packet *p){
 bool emitted=false;dt_packet out;
 for(unsigned i=0;i<samples;i++){bool result=step(c,f,y,z,&out);assert(!(result&&out.type==1));}
 if(step(c,f,0,0,&out)&&out.type==1){*p=out;emitted=true;}
 return emitted;
}
int main(void){
 fake f={{{0,0,9.80665f},{0,0,0},10},false};dt_controller c;dt_packet p;
 dt_motion_config config=dt_motion_defaults();config.filter_alpha=1;
 assert(dt_controller_init(&c,(dt_imu_port){&f,read_sample},config));
 assert(!dt_controller_tick(&c,&p)&&!c.ready);
 dt_packet request=command(130,77);assert(dt_controller_command(&c,&request,10,&p)&&p.type==131&&p.event_id==77&&p.flags==0&&!c.calibrated&&!c.calibrating);
 assert(dt_controller_button(&c,3,10,&p)&&p.type==2&&p.flags==0&&dt_validate(&p)==DT_OK);
 request=command(128,7);assert(dt_controller_command(&c,&request,10,&p)&&p.kind==2&&!c.calibrated);
 f.available=true;step(&c,&f,0,0,&p);request=command(128,8);assert(!dt_controller_command(&c,&request,f.value.time_ms,&p));
 assert(dt_controller_command(&c,&request,f.value.time_ms,&p)&&p.kind==1);
 for(unsigned i=0;i<64;i++){bool result=step(&c,&f,0,0,&p);if(i==63)assert(result&&p.type==129&&p.kind==0&&p.event_id==8);}
 assert(c.calibrated&&c.ready);
 request=command(130,78);assert(dt_controller_command(&c,&request,f.value.time_ms,&p)&&p.type==131&&p.flags==3&&c.calibrated);
 request=command(128,8);assert(dt_controller_command(&c,&request,f.value.time_ms,&p)&&p.kind==0&&!c.calibrating);
 assert(stroke(&c,&f,0,400,3,&p)&&p.kind==1&&p.yaw_cdeg==1200&&p.pitch_cdeg==0&&p.power>0);
 uint8_t wire[20];assert(dt_encode(&p,wire,sizeof(wire))==DT_OK);uint16_t event=p.event_id;
 quiet(&c,&f);assert(stroke(&c,&f,0,-400,3,&p)&&p.kind==2&&p.yaw_cdeg==-1200&&p.event_id==event+1);
 quiet(&c,&f);assert(stroke(&c,&f,400,0,3,&p)&&p.kind==3&&p.pitch_cdeg==1200);
 // Explicit shot selection resolves motions whose semantics require match/user context.
 for(uint8_t kind=4;kind<=8;kind++){assert(dt_controller_set_mode(&c,kind));quiet(&c,&f);assert(stroke(&c,&f,0,600,3,&p)&&p.kind==kind&&dt_validate(&p)==DT_OK);}
 assert(!dt_controller_set_mode(&c,9));assert(dt_controller_set_mode(&c,0));
 // Synthetic replay profiles exercise the rule engine; they are not measured human gestures.
 dt_motion_rule rules[8];
 for(unsigned i=0;i<8;i++)rules[i]=(dt_motion_rule){(uint8_t)(i+1),11+6.0f*i,13+6.0f*i,-1,1,590,610,20,200};
 assert(dt_controller_set_rules(&c,rules,8));
 for(unsigned i=0;i<8;i++){quiet(&c,&f);assert(stroke(&c,&f,0,600,2+i,&p)&&p.kind==i+1);}
 quiet(&c,&f);assert(!stroke(&c,&f,0,-600,3,&p)); // unknown gesture rejected
 rules[1]=rules[0];rules[1].kind=2;assert(dt_controller_set_rules(&c,rules,2));quiet(&c,&f);assert(!stroke(&c,&f,0,600,2,&p)); // ambiguous rejected
 rules[0].min_yaw=NAN;assert(!dt_controller_set_rules(&c,rules,1)&&c.rule_count==2);
 assert(dt_controller_set_rules(&c,NULL,0));quiet(&c,&f);assert(!stroke(&c,&f,0,600,1,&p)); // short noise
 c.config.left_handed=true;quiet(&c,&f);assert(stroke(&c,&f,0,400,3,&p)&&p.kind==2&&p.yaw_cdeg==-1200&&(p.flags&4));c.config.left_handed=false;
 quiet(&c,&f);assert(!stroke(&c,&f,0,400,110,&p)&&!c.swinging); // sustained motion times out without firing
 quiet(&c,&f);step(&c,&f,0,400,&p);f.value.time_ms+=101;assert(!dt_controller_tick(&c,&p)||p.type!=1);assert(!c.calibrated&&!c.swinging); // gap aborts
 f.available=false;dt_controller_tick(&c,&p);assert(!dt_controller_intent(&c,8,f.value.time_ms,&p));
 request=command(130,90);assert(dt_controller_command(&c,&request,f.value.time_ms,&p)&&p.type==131&&p.flags==0); // probe survives sensor loss
 f.available=true;step(&c,&f,0,0,&p);request=command(128,9);assert(!dt_controller_command(&c,&request,f.value.time_ms,&p));
 f.value.time_ms+=5001;assert(dt_controller_tick(&c,&p)&&p.type==129&&p.kind==3&&!c.calibrated);
 // Button-only firmware announces its profile without inventing sensor flags.
 dt_controller buttons;assert(dt_controller_init(&buttons,(dt_imu_port){&f,read_sample},config));
 assert(!dt_controller_button(&buttons,DT_BUTTON_SERVE,0,&p));
 buttons.input_profile=DT_INPUT_BUTTONS;
 request=command(130,91);assert(dt_controller_command(&buttons,&request,10,&p)&&p.kind==DT_INPUT_BUTTONS&&p.flags==0);
 for(uint8_t kind=DT_BUTTON_SERVE;kind<=DT_BUTTON_SKILL;kind++){
  assert(dt_controller_button(&buttons,kind,10,&p)&&p.type==2&&p.kind==kind&&p.flags==0&&p.power==0&&dt_validate(&p)==DT_OK);
 }
 assert(!dt_controller_button(&buttons,8,10,&p));assert(!dt_controller_tick(&buttons,&p)&&!buttons.ready&&!buttons.calibrated);
 assert(!dt_controller_intent(&buttons,8,10,&p));
 request=command(128,92);assert(dt_controller_command(&buttons,&request,10,&p)&&p.kind==2&&!buttons.calibrated);
 config.axis[0]=2;assert(!dt_controller_init(&c,(dt_imu_port){&f,read_sample},config));
 puts("PASS: sensorless probe, independent buttons, calibration ACK/busy/idempotency, complete swings/directions, eight replay profiles, ambiguity/noise/gap/sensor loss and configuration checks");return 0;
}
