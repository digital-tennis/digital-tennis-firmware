#include "dt_controller.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static bool absent(void *c,dt_imu_sample *s){(void)c;(void)s;return false;}
int main(void){
 const float axis[9]={1,0,0,0,1,0,0,0,1},bias[3]={0};dt_imu_sample samples[40]={0};
 for(unsigned i=0;i<40;i++){samples[i].time_ms=i*10;samples[i].acceleration[2]=3*9.80665f;samples[i].gyro_dps[2]=i<30?400:0;}
 dt_gesture_result r=dt_gesture_classify(samples,40,axis,bias,false,80,DT_CONTEXT_UNKNOWN);assert(r.kind==1&&r.reason==DT_GESTURE_OK);
 dt_imu_sample ring[128]={0};unsigned head=7;
 for(unsigned i=0;i<40;i++)ring[(head+128-40+i)%128]=samples[i];
 dt_gesture_result wrapped=dt_gesture_classify_ring(ring,40,head,128,axis,bias,false,80,DT_CONTEXT_UNKNOWN);assert(wrapped.kind==r.kind&&fabsf(wrapped.features.sweep_deg-r.features.sweep_deg)<.001f);
 r=dt_gesture_classify(samples,40,axis,bias,true,80,DT_CONTEXT_UNKNOWN);assert(r.kind==2);
 for(unsigned i=0;i<40;i++){samples[i].gyro_dps[0]=-samples[i].gyro_dps[2];samples[i].gyro_dps[2]=0;}
 r=dt_gesture_classify(samples,40,axis,bias,false,80,DT_CONTEXT_UNKNOWN);assert(r.kind==0&&r.candidates==24&&r.reason==DT_GESTURE_CONTEXT);
 r=dt_gesture_classify(samples,40,axis,bias,false,80,DT_CONTEXT_SERVE);assert(r.kind==3&&r.candidates==24);
 r=dt_gesture_classify(samples,40,axis,bias,false,80,DT_CONTEXT_RALLY);assert(r.kind==4&&r.candidates==24);
 samples[12].gyro_dps[0]=NAN;r=dt_gesture_classify(samples,40,axis,bias,false,80,DT_CONTEXT_SERVE);assert(r.kind==0&&r.reason==DT_GESTURE_INVALID);
 assert(dt_gesture_classify(samples,40,axis,bias,false,80,(dt_gesture_context)-1).reason==DT_GESTURE_INVALID);
 assert(dt_gesture_classify_ring(samples,40,0,0,axis,bias,false,80,DT_CONTEXT_SERVE).reason==DT_GESTURE_INVALID);
 dt_controller c;dt_motion_config config=dt_motion_gesture_defaults();assert(dt_controller_init(&c,(dt_imu_port){NULL,absent},config));
 assert(!dt_controller_set_context(&c,(dt_gesture_context)-1));assert(dt_controller_set_context(&c,DT_CONTEXT_RALLY));
 config.axis[0]=-1;assert(!dt_controller_init(&c,(dt_imu_port){NULL,absent},config)); // mounting must be a proper rotation
 config=dt_motion_gesture_defaults();config.acceleration_peak=true;assert(!dt_controller_init(&c,(dt_imu_port){NULL,absent},config));
 puts("PASS: gesture features, ring wrap, handedness, identical overhead/context, invalid samples and proper mounting rotations");return 0;
}
