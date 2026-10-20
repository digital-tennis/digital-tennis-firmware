#include "dt_controller.h"
#include <math.h>
#include <string.h>
#include "dt_gesture_rules.h"
static float magnitude(const float *v){return sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);}
static float clamp(float x,float low,float high){return fminf(high,fmaxf(low,x));}
static bool finite_sample(const dt_imu_sample *s){for(int i=0;i<3;i++){if(!isfinite(s->acceleration[i])||!isfinite(s->gyro_dps[i])||fabsf(s->acceleration[i])>200||fabsf(s->gyro_dps[i])>8000)return false;}return true;}
static void packet(dt_controller *c,dt_packet *p,uint8_t type,uint8_t kind,uint32_t now){memset(p,0,sizeof(*p));p->version=c->input_profile>=DT_INPUT_PHYSICAL_MOTION?3:2;p->type=type;p->flags=(c->ready?2:0)|(c->calibrated?1:0)|(c->config.left_handed?4:0);p->sequence=++c->sequence;p->device_ms=now;p->battery=255;p->kind=kind;}
static uint16_t next_event(dt_controller *c){
 if(++c->event_id==0)++c->event_id;
 return c->event_id;
}
static void remember(dt_controller *c,uint8_t result){unsigned i=c->history_next;c->command_history[i]=c->command_id;c->command_results[i]=result;c->history_next=(i+1)%64;if(c->history_count<64)c->history_count++;}
static void ack(dt_controller *c,dt_packet *p,uint8_t result,uint32_t now){packet(c,p,129,result,now);p->event_id=c->command_id;}
dt_motion_config dt_motion_defaults(void){dt_motion_config c={.axis={1,0,0,0,1,0,0,0,1},.filter_alpha=.25f,.trigger_dps=220,.release_dps=80,.serve_dps=180,.cooldown_ms=200,.calibration_samples=64,.max_gap_ms=100,.min_swing_ms=20,.max_swing_ms=1000,.max_power_dps=2000,.trigger_g=2.5f,.release_g=1.5f,.max_power_g=12};return c;}
dt_motion_config dt_motion_peak_defaults(void){dt_motion_config c=dt_motion_defaults();c.acceleration_peak=true;return c;}
dt_motion_config dt_motion_physical_defaults(void){
 dt_motion_config c=dt_motion_defaults();c.recognize_gestures=true;
 c.filter_alpha=DT_GESTURE_FILTER_ALPHA;c.trigger_dps=DT_GESTURE_TRIGGER_DPS;c.release_dps=DT_GESTURE_RELEASE_DPS;
 c.cooldown_ms=DT_GESTURE_COOLDOWN_MS;c.min_swing_ms=DT_GESTURE_MIN_SWING_MS;c.max_swing_ms=DT_GESTURE_MAX_SWING_MS;
 return c;
}
dt_motion_config dt_motion_gesture_defaults(void){return dt_motion_physical_defaults();}
static void clear_window(dt_controller *c){c->window_head=c->window_count=c->acceleration_count=0;c->acceleration_before=c->acceleration_previous=0;}
unsigned dt_controller_copy_window(const dt_controller *c,dt_imu_sample *samples,unsigned capacity){
 if(!c||!samples)return 0;
 unsigned count=c->window_count<capacity?c->window_count:capacity;
 for(unsigned i=0;i<count;i++)samples[i]=c->window[(c->window_head+DT_IMU_WINDOW_CAPACITY-count+i)%DT_IMU_WINDOW_CAPACITY];
 return count;
}
static void retain(dt_controller *c,const dt_imu_sample *s){
 c->window[c->window_head]=*s;c->window_head=(c->window_head+1)%DT_IMU_WINDOW_CAPACITY;
 unsigned limit=c->config.recognize_gestures?DT_IMU_WINDOW_CAPACITY:20;
 if(c->window_count<limit)c->window_count++;
}
bool dt_controller_init(dt_controller *c,dt_imu_port port,dt_motion_config config){
 if(!c||!port.read||!isfinite(config.filter_alpha)||config.filter_alpha<=0||config.filter_alpha>1||!isfinite(config.trigger_dps)||config.trigger_dps<=0||!isfinite(config.release_dps)||config.release_dps<0||config.release_dps>=config.trigger_dps||!isfinite(config.serve_dps)||config.serve_dps<=0||config.calibration_samples<8||config.calibration_samples>4096||config.cooldown_ms<20||config.cooldown_ms>5000||config.max_gap_ms<10||config.max_gap_ms>1000||config.min_swing_ms<10||config.max_swing_ms<=config.min_swing_ms||config.max_swing_ms>5000||!isfinite(config.max_power_dps)||config.max_power_dps<=config.trigger_dps)return false;
 if(!isfinite(config.trigger_g)||!isfinite(config.release_g)||!isfinite(config.max_power_g)||config.release_g<=1||config.trigger_g<=config.release_g||config.max_power_g<=config.trigger_g||config.max_power_g>20)return false;
 if(config.recognize_gestures&&(config.acceleration_peak||config.release_dps<=0))return false;
 for(int row=0;row<3;row++){for(int col=0;col<3;col++){float dot=0;for(int k=0;k<3;k++){float v=config.axis[row*3+k];if(!isfinite(v))return false;dot+=v*config.axis[col*3+k];}if(fabsf(dot-(row==col?1:0))>.001f)return false;}}
 const float *a=config.axis;float determinant=a[0]*(a[4]*a[8]-a[5]*a[7])-a[1]*(a[3]*a[8]-a[5]*a[6])+a[2]*(a[3]*a[7]-a[4]*a[6]);
 if(fabsf(determinant-1)>.001f)return false;
 memset(c,0,sizeof(*c));c->imu=port;c->config=config;c->input_profile=config.recognize_gestures?DT_INPUT_PHYSICAL_MOTION:DT_INPUT_MOTION;return true;
}
bool dt_controller_command(dt_controller *c,const dt_packet *command,uint32_t now,dt_packet *response){
 if(!c||!command||!response||dt_validate(command)!=DT_OK)return false;
 if(command->type==130){packet(c,response,131,(uint8_t)c->input_profile,now);response->event_id=command->event_id;return true;}
 if(command->type!=128)return false;
 if(c->calibrating){packet(c,response,129,1,now);response->event_id=command->event_id;return true;}
 for(unsigned i=0;i<c->history_count;i++){if(c->command_history[i]==command->event_id){packet(c,response,129,c->command_results[i],now);response->event_id=command->event_id;return true;}}
 c->command_id=command->event_id;
 if(!c->ready){ack(c,response,2,now);remember(c,2);return true;}
 c->calibrating=true;c->calibrated=false;c->latched=c->swinging=false;c->samples=0;c->calibration_started=now;clear_window(c);memset(c->sum,0,sizeof(c->sum));return false;
}
bool dt_controller_button(dt_controller *c,uint8_t kind,uint32_t now,dt_packet *out){
 if(!c||c->input_profile>=DT_INPUT_PHYSICAL_MOTION||!out||kind<1||kind>7||(kind>3&&c->input_profile!=DT_INPUT_BUTTONS))return false;
 packet(c,out,2,kind,now);out->event_id=next_event(c);return true;
}
bool dt_controller_button_edge(dt_controller *c,uint8_t number,bool down,uint32_t now,dt_packet *out){
 if(!c||!out||c->input_profile<DT_INPUT_PHYSICAL_MOTION||number<1||number>5)return false;
 packet(c,out,down?DT_PACKET_BUTTON_DOWN:DT_PACKET_BUTTON_UP,number,now);out->event_id=next_event(c);
 return true;
}
bool dt_controller_set_mode(dt_controller *c,uint8_t kind){
 if(!c||c->input_profile>=DT_INPUT_PHYSICAL_MOTION||kind>8)return false;
 c->shot_mode=kind;c->swinging=false;c->latched=true;return true;
}
bool dt_controller_set_rules(dt_controller *c,const dt_motion_rule *rules,unsigned count){
 if(!c||c->input_profile>=DT_INPUT_PHYSICAL_MOTION||count>8||(count&&!rules))return false;
 for(unsigned i=0;i<count;i++){
  const dt_motion_rule *r=&rules[i];
  if(r->kind<1||r->kind>8||!isfinite(r->min_yaw)||!isfinite(r->max_yaw)||!isfinite(r->min_pitch)||!isfinite(r->max_pitch)||!isfinite(r->min_peak)||!isfinite(r->max_peak)||r->min_yaw < -180||r->max_yaw>180||r->min_yaw>r->max_yaw||r->min_pitch < -90||r->max_pitch>90||r->min_pitch>r->max_pitch||r->min_peak<0||r->min_peak>r->max_peak||r->min_ms>r->max_ms||r->max_ms>5000)return false;
 }
 if(count){memcpy(c->rules,rules,count*sizeof(*rules));}c->rule_count=count;c->swinging=false;c->latched=true;return true;
}
static uint8_t classify(dt_controller *c,uint32_t duration){
 if(c->shot_mode)return c->shot_mode;
 if(c->config.recognize_gestures){
  c->gesture_result=dt_gesture_classify_ring(c->window,c->window_count,c->window_head,DT_IMU_WINDOW_CAPACITY,c->config.axis,c->bias,c->config.left_handed,c->config.release_dps);
  c->gesture_attempts++;return c->gesture_result.kind;
 }
 uint8_t found=0;
 for(unsigned i=0;i<c->rule_count;i++){
  const dt_motion_rule *r=&c->rules[i];
  if(c->yaw>=r->min_yaw&&c->yaw<=r->max_yaw&&c->pitch>=r->min_pitch&&c->pitch<=r->max_pitch&&c->peak>=r->min_peak&&c->peak<=r->max_peak&&duration>=r->min_ms&&duration<=r->max_ms){if(found&&found!=r->kind)return 0;found=r->kind;}
 }
 if(c->rule_count)return found; // A configured profile rejects unknown/ambiguous gestures.
 return c->peak_y>=c->config.serve_dps?3:c->peak_z>=0?1:2;
}
static bool acceleration_tick(dt_controller *c,const dt_imu_sample *s,float speed,uint32_t elapsed,dt_packet *out){
 float g=magnitude(s->acceleration)/9.80665f,previous=c->acceleration_previous;
 bool maximum=c->acceleration_count>=2&&previous>c->acceleration_before&&previous>=g&&previous>=c->config.trigger_g;
 c->acceleration_before=previous;c->acceleration_previous=g;
 if(c->acceleration_count<2)c->acceleration_count++;
 // Confirm the previous sample using one later sample, before including its gyro.
 if(c->swinging&&maximum){
  uint32_t peak_at=s->time_ms-elapsed,duration=peak_at-c->swing_started;
  if(duration>=c->config.min_swing_ms&&duration<c->config.max_swing_ms){
   uint8_t kind=classify(c,duration);
   c->swinging=false;c->latched=true;c->last_swing=s->time_ms;
   if(kind){packet(c,out,1,kind,peak_at);out->event_id=next_event(c);
    out->power=(uint8_t)lroundf(1+99*clamp((previous-c->config.trigger_g)/(c->config.max_power_g-c->config.trigger_g),0,1));
    out->yaw_cdeg=(int16_t)lroundf(c->yaw*100);out->pitch_cdeg=(int16_t)lroundf(c->pitch*100);return true;}
  }
 }
 if(!c->swinging){
  if(speed<c->config.release_dps&&g<c->config.release_g)c->latched=false;
  if(!c->latched&&speed>=c->config.trigger_dps&&(uint32_t)(s->time_ms-c->last_swing)>=c->config.cooldown_ms){
   c->swinging=c->latched=true;c->swing_started=s->time_ms;c->yaw=c->pitch=0;c->peak=speed;c->peak_y=c->filtered[1];c->peak_z=c->filtered[2];
  }
 }
 if(c->swinging){
  c->yaw=clamp(c->yaw+c->filtered[2]*elapsed/1000.0f,-180,180);c->pitch=clamp(c->pitch+c->filtered[1]*elapsed/1000.0f,-90,90);
  if(speed>c->peak){c->peak=speed;c->peak_y=c->filtered[1];c->peak_z=c->filtered[2];}
  if((uint32_t)(s->time_ms-c->swing_started)>=c->config.max_swing_ms||speed<c->config.release_dps){c->swinging=false;c->last_swing=s->time_ms;}
 }
 return false;
}
bool dt_controller_intent(dt_controller *c,uint8_t kind,uint32_t now,dt_packet *out){
 if(!c||c->input_profile>=DT_INPUT_PHYSICAL_MOTION||!out||!c->ready||!c->calibrated||c->calibrating||kind<1||kind>8)return false;
 packet(c,out,1,kind,now);out->event_id=next_event(c);out->power=50;return true;
}
bool dt_controller_tick(dt_controller *c,dt_packet *out){
 if(!c||!out||(c->input_profile==DT_INPUT_BUTTONS||c->input_profile==DT_INPUT_PHYSICAL_BUTTONS))return false;
 dt_imu_sample s={0};bool valid=c->imu.read(c->imu.context,&s)&&finite_sample(&s);
 if(!valid){bool changed=c->ready;c->ready=c->calibrated=c->swinging=false;clear_window(c);if(c->calibrating){c->calibrating=false;ack(c,out,3,c->last_time);remember(c,3);return true;}if(changed){packet(c,out,3,0,c->last_time);return true;}return false;}
 uint32_t elapsed=c->has_time?(uint32_t)(s.time_ms-c->last_time):0;
 if(c->has_time&&elapsed>0x7fffffffU)return false; // old sample, not a 32-bit clock wrap
 if(c->has_time&&elapsed>c->config.max_gap_ms){c->calibrated=c->swinging=false;c->samples=0;memset(c->sum,0,sizeof(c->sum));clear_window(c);memset(c->filtered,0,sizeof(c->filtered));}
 if(c->has_time&&s.time_ms==c->last_time)return false;
 c->ready=true;c->last_time=s.time_ms;c->has_time=true;
 retain(c,&s);
 if(c->calibrating){
  if((uint32_t)(s.time_ms-c->calibration_started)>=5000){c->calibrating=false;ack(c,out,3,s.time_ms);remember(c,3);return true;}
  if(magnitude(s.gyro_dps)>15||fabsf(magnitude(s.acceleration)-9.80665f)>1){c->samples=0;memset(c->sum,0,sizeof(c->sum));return false;}
  for(int i=0;i<3;i++)c->sum[i]+=s.gyro_dps[i];
  if(++c->samples>=c->config.calibration_samples){for(int i=0;i<3;i++)c->bias[i]=c->sum[i]/c->samples;c->calibrated=true;c->calibrating=false;clear_window(c);memset(c->filtered,0,sizeof(c->filtered));ack(c,out,0,s.time_ms);remember(c,0);return true;}return false;
 }
 if(c->calibrated){
  float mapped[3]={0};for(int row=0;row<3;row++){for(int col=0;col<3;col++)mapped[row]+=c->config.axis[row*3+col]*(s.gyro_dps[col]-c->bias[col]);}
  if(c->config.left_handed)mapped[2]=-mapped[2];
  for(int i=0;i<3;i++)c->filtered[i]+=c->config.filter_alpha*(mapped[i]-c->filtered[i]);
  float speed=magnitude(c->filtered);
  if(c->config.acceleration_peak){if(acceleration_tick(c,&s,speed,elapsed,out))return true;}
  else {
  if(!c->swinging){
   if(speed<c->config.release_dps)c->latched=false;
   if(!c->latched&&speed>=c->config.trigger_dps&&(uint32_t)(s.time_ms-c->last_swing)>=c->config.cooldown_ms){
    c->swinging=true;c->latched=true;c->swing_started=s.time_ms;c->yaw=c->pitch=0;c->peak=speed;c->peak_y=c->filtered[1];c->peak_z=c->filtered[2];
   }
  }
  if(c->swinging){
   float dt=elapsed/1000.0f;c->yaw=clamp(c->yaw+c->filtered[2]*dt,-180,180);c->pitch=clamp(c->pitch+c->filtered[1]*dt,-90,90);
   if(speed>c->peak){c->peak=speed;c->peak_y=c->filtered[1];c->peak_z=c->filtered[2];}
   uint32_t duration=(uint32_t)(s.time_ms-c->swing_started);
   if(duration>=c->config.max_swing_ms){c->swinging=false;c->last_swing=s.time_ms;}
   else if(speed<c->config.release_dps){
    c->swinging=false;c->last_swing=s.time_ms;
    uint8_t kind=duration>=c->config.min_swing_ms?classify(c,duration):0;
    if(kind){packet(c,out,c->input_profile==DT_INPUT_PHYSICAL_MOTION?DT_PACKET_MOTION:1,kind,s.time_ms);out->event_id=next_event(c);
     out->power=c->config.recognize_gestures?(uint8_t)lroundf(1+99*clamp((c->gesture_result.features.peak_g-c->config.trigger_g)/(c->config.max_power_g-c->config.trigger_g),0,1)):(uint8_t)clamp(100*(c->peak-c->config.release_dps)/(c->config.max_power_dps-c->config.release_dps),1,100);
     out->yaw_cdeg=(int16_t)lroundf(c->yaw*100);out->pitch_cdeg=(int16_t)lroundf(c->pitch*100);return true;}
   }
  }
  }
 }
 if((uint32_t)(s.time_ms-c->status_at)>=1000){c->status_at=s.time_ms;packet(c,out,3,0,s.time_ms);return true;}return false;
}
