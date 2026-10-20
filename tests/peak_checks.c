#include "dt_controller.h"
#include "dt_sample_clock.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
typedef struct {dt_imu_sample sample;bool available;} sensor;
static bool read_sensor(void *context,dt_imu_sample *sample){sensor *s=context;*sample=s->sample;return s->available;}
static bool step(dt_controller *c,sensor *s,float g,float y,float z,dt_packet *p){
 s->sample.time_ms+=10;s->sample.acceleration[2]=g*9.80665f;s->sample.gyro_dps[1]=y;s->sample.gyro_dps[2]=z;
 return dt_controller_tick(c,p)&&p->type==1;
}
static void rest(dt_controller *c,sensor *s){dt_packet p;for(unsigned i=0;i<30;i++)assert(!step(c,s,1,0,0,&p));}
static void initialize(dt_controller *c,sensor *s){
 *s=(sensor){.sample={.acceleration={0,0,9.80665f},.time_ms=10},.available=true};
 dt_motion_config config=dt_motion_peak_defaults();config.filter_alpha=1;
 assert(dt_controller_init(c,(dt_imu_port){s,read_sensor},config));
 dt_packet p,cmd={.version=2,.type=128,.event_id=1,.battery=255};
 assert(!step(c,s,1,0,0,&p));assert(!dt_controller_command(c,&cmd,s->sample.time_ms,&p));
 for(unsigned i=0;i<64;i++)assert(!step(c,s,1,0,0,&p));
 assert(c->calibrated&&c->window_count==0);rest(c,s);
}
static dt_packet swing(dt_controller *c,sensor *s,float peak,float y,float z){
 dt_packet p;assert(!step(c,s,1,y,z,&p));assert(!step(c,s,2,y,z,&p));
 assert(!step(c,s,peak,y,z,&p));uint32_t peak_ms=s->sample.time_ms;
 assert(step(c,s,peak-0.25f,y,z,&p)); // no need to wait for gyro release
 assert(p.device_ms==peak_ms&&p.flags%4==3&&p.power>=1&&p.power<=100);
 uint8_t wire[20];dt_packet decoded;assert(dt_encode(&p,wire,20)==DT_OK&&dt_decode(wire,20,&decoded)==DT_OK);
 assert(decoded.kind==p.kind&&decoded.event_id==p.event_id);
 return p;
}
int main(void){
 dt_controller c;sensor s;dt_packet p;initialize(&c,&s);
 p=swing(&c,&s,4,0,400);assert(p.kind==1&&p.yaw_cdeg==1200);unsigned weak=p.power;
 for(unsigned i=0;i<40;i++)assert(!step(&c,&s,i%2?5:3,0,400,&p)); // rebound without rearm
 rest(&c,&s);p=swing(&c,&s,8,0,-400);assert(p.kind==2&&p.power>weak&&p.yaw_cdeg==-1200);
 rest(&c,&s);p=swing(&c,&s,14,400,0);assert(p.kind==3&&p.power==100&&p.pitch_cdeg==1200);
 rest(&c,&s);c.config.left_handed=true;p=swing(&c,&s,4,0,400);assert(p.kind==2&&(p.flags&4));c.config.left_handed=false;
 // Translation/shaking without racket rotation and pure rotation never fire.
 rest(&c,&s);for(unsigned i=0;i<30;i++)assert(!step(&c,&s,i%2?6:1,0,0,&p));
 rest(&c,&s);for(unsigned i=0;i<20;i++)assert(!step(&c,&s,1,0,400,&p));rest(&c,&s);
 assert(!step(&c,&s,1,0,400,&p));assert(!step(&c,&s,6,0,400,&p));assert(!step(&c,&s,1,0,0,&p)); // one-sample spike too short
 rest(&c,&s);
 // Plateau peak fires once; ambiguous profiles reject rather than guessing.
 assert(!step(&c,&s,1,0,400,&p));assert(!step(&c,&s,2,0,400,&p));assert(!step(&c,&s,4,0,400,&p));
 assert(step(&c,&s,4,0,400,&p));assert(!step(&c,&s,4,0,400,&p));rest(&c,&s);
 dt_motion_rule rules[2]={{1,-180,180,-90,90,0,2000,0,1000},{2,-180,180,-90,90,0,2000,0,1000}};
 assert(dt_controller_set_rules(&c,rules,2));rest(&c,&s);
 assert(!step(&c,&s,1,0,400,&p));assert(!step(&c,&s,2,0,400,&p));assert(!step(&c,&s,6,0,400,&p));assert(!step(&c,&s,5,0,400,&p));
 assert(dt_controller_set_rules(&c,NULL,0));rest(&c,&s);
 assert(dt_controller_set_mode(&c,8));rest(&c,&s);p=swing(&c,&s,4,0,400);assert(p.kind==8); // explicit skill mode, not learned recognition
 // Fixed-size chronological window and duplicate/old sample rejection.
 rest(&c,&s);dt_imu_sample rows[20];assert(dt_controller_copy_window(&c,rows,20)==20);
 for(unsigned i=1;i<20;i++)assert(rows[i].time_ms-rows[i-1].time_ms==10);
 unsigned head=c.window_head;assert(!dt_controller_tick(&c,&p)&&head==c.window_head);
 uint32_t latest=s.sample.time_ms;s.sample.time_ms-=10;assert(!dt_controller_tick(&c,&p)&&c.calibrated&&head==c.window_head);s.sample.time_ms=latest;
 s.sample.time_ms+=101;dt_controller_tick(&c,&p);assert(!c.calibrated&&c.window_count==1);
 s.available=false;dt_controller_tick(&c,&p);assert(!c.ready&&c.window_count==0);
 initialize(&c,&s);s.sample.acceleration[0]=NAN;dt_controller_tick(&c,&p);assert(!c.ready&&!c.calibrated);
 // Counter wrap remains monotonic; old timestamps do not masquerade as a gap.
 initialize(&c,&s);c.last_time=s.sample.time_ms=UINT32_MAX-5;
 assert(!step(&c,&s,1,0,0,&p)&&c.calibrated&&c.last_time==4);
 dt_motion_config bad=dt_motion_peak_defaults();bad.trigger_g=NAN;assert(!dt_controller_init(&c,(dt_imu_port){&s,read_sensor},bad));
 dt_sample_clock clock={0};assert(dt_sample_due(&clock,0));assert(!dt_sample_due(&clock,9));assert(dt_sample_due(&clock,11)&&clock.next_ms==20);
 assert(dt_sample_due(&clock,47)&&clock.next_ms==50);assert(!dt_sample_due(&clock,47));
 clock=(dt_sample_clock){.next_ms=UINT32_MAX-4,.started=true};assert(dt_sample_due(&clock,5)&&clock.next_ms==15);
 puts("PASS: acceleration peak confirmation, power, FH/BH/serve, rearm/rebound, noise, rules, explicit skill, window, loss/gap/wrap and sampling deadlines");return 0;
}
