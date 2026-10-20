#include "dt_gesture.h"
#include <math.h>
#include <string.h>
#include "dt_gesture_rules.h"
static float norm(const float v[3]){return sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);}
static bool mapped_gyro(const dt_imu_sample *s,const float axis[9],const float bias[3],bool left,float out[3]){
 for(unsigned i=0;i<3;i++){
  out[i]=0;
  for(unsigned j=0;j<3;j++){
   if(!isfinite(s->gyro_dps[j])||fabsf(s->gyro_dps[j])>8000||!isfinite(s->acceleration[j])||fabsf(s->acceleration[j])>200||!isfinite(axis[i*3+j])||!isfinite(bias[j]))return false;
   out[i]+=axis[i*3+j]*(s->gyro_dps[j]-bias[j]);
  }
 }
 // A left-hand mirror is a reflection: angular velocity is an axial vector.
 if(left){out[1]=-out[1];out[2]=-out[2];}
 return true;
}
static const dt_imu_sample *at(const dt_imu_sample *s,unsigned count,unsigned head,unsigned capacity,unsigned index){return &s[(head+capacity-count+index)%capacity];}
dt_gesture_result dt_gesture_classify(const dt_imu_sample *s,unsigned count,const float axis[9],const float bias[3],bool left,float release,dt_gesture_context context){
 return dt_gesture_classify_ring(s,count,0,count,axis,bias,left,release,context);
}
dt_gesture_result dt_gesture_classify_ring(const dt_imu_sample *s,unsigned count,unsigned head,unsigned capacity,const float axis[9],const float bias[3],bool left,float release,dt_gesture_context context){
 dt_gesture_result result={.reason=DT_GESTURE_INVALID};
 if(!s||!axis||!bias||count<2||count>capacity||capacity>128||head>=capacity||!isfinite(release)||release<=0||(unsigned)context>DT_CONTEXT_RALLY)return result;
 unsigned end=count;float gyro[3];
 // Ignore the filtered detector's quiet tail, never look at future samples.
 while(end){if(!mapped_gyro(at(s,count,head,capacity,end-1),axis,bias,left,gyro))return result;if(norm(gyro)>=release)break;end--;}
 if(end<2){result.reason=DT_GESTURE_SHORT;return result;}
 unsigned begin=end-1;
 while(begin){if(!mapped_gyro(at(s,count,head,capacity,begin-1),axis,bias,left,gyro))return result;if(norm(gyro)<release)break;begin--;}
 if(end-begin<2){result.reason=DT_GESTURE_SHORT;return result;}
 float integral[3]={0},path=0,previous[3];
 if(!mapped_gyro(at(s,count,head,capacity,begin),axis,bias,left,previous))return result;
 for(unsigned i=begin;i<end;i++){
  const dt_imu_sample *sample=at(s,count,head,capacity,i);
  if(!mapped_gyro(sample,axis,bias,left,gyro))return result;
  float g=norm(sample->acceleration)/9.80665f,speed=norm(gyro);
  result.features.peak_g=fmaxf(result.features.peak_g,g);result.features.peak_dps=fmaxf(result.features.peak_dps,speed);
  if(i>begin){
   uint32_t elapsed=sample->time_ms-at(s,count,head,capacity,i-1)->time_ms;if(!elapsed||elapsed>100)return result;
   float dt=elapsed/1000.0f;path+=(norm(previous)+speed)*.5f*dt;
   for(unsigned j=0;j<3;j++)integral[j]+=(previous[j]+gyro[j])*.5f*dt;
  }
  memcpy(previous,gyro,sizeof(previous));
 }
 if(path<=0)return result;
 result.features.direction_x=integral[0]/path;result.features.direction_z=integral[2]/path;
 result.features.sweep_deg=path;result.features.active_ms=(float)(uint32_t)(at(s,count,head,capacity,end-1)->time_ms-at(s,count,head,capacity,begin)->time_ms);
 result.features.coherence=norm(integral)/path;
 const dt_gesture_features *f=&result.features;
 for(unsigned i=0;i<7;i++){
  const dt_gesture_rule *r=&gesture_rules[i];float az=fabsf(f->direction_z);
  if(f->direction_x>=r->min_x&&f->direction_x<=r->max_x&&f->direction_z>=r->min_z&&f->direction_z<=r->max_z&&az>=r->min_abs_z&&az<=r->max_abs_z&&f->sweep_deg>=r->min_sweep&&f->sweep_deg<=r->max_sweep&&f->active_ms>=r->min_ms&&f->active_ms<=r->max_ms&&f->coherence>=r->min_coherence&&f->peak_g>=r->min_g&&f->peak_g<=r->max_g)result.candidates|=(uint16_t)(1U<<r->kind);
 }
 uint16_t eligible=result.candidates;
 if(context==DT_CONTEXT_SERVE)eligible&=(uint16_t)~(1U<<4);
 if(context==DT_CONTEXT_RALLY)eligible&=(uint16_t)~(1U<<3);
 if(!eligible){result.reason=DT_GESTURE_UNMATCHED;return result;}
 if(eligible&(eligible-1)){result.reason=context==DT_CONTEXT_UNKNOWN&&(eligible&24)==24?DT_GESTURE_CONTEXT:DT_GESTURE_AMBIGUOUS;return result;}
 for(uint8_t i=1;i<=7;i++)if(eligible==(1U<<i))result.kind=i;
 result.reason=DT_GESTURE_OK;return result;
}
