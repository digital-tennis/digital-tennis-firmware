#include "dt_controller.h"
#include <math.h>
#include <string.h>
static float magnitude(const float *v){return sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);}
static float clamp(float x,float low,float high){return fminf(high,fmaxf(low,x));}
static bool finite_sample(const dt_imu_sample *s){for(int i=0;i<3;i++){if(!isfinite(s->acceleration[i])||!isfinite(s->gyro_dps[i])||fabsf(s->acceleration[i])>200||fabsf(s->gyro_dps[i])>8000)return false;}return true;}
static void packet(dt_controller *c,dt_packet *p,uint8_t type,uint8_t kind,uint32_t now){memset(p,0,sizeof(*p));p->version=2;p->type=type;p->flags=(c->ready?2:0)|(c->calibrated?1:0)|(c->config.left_handed?4:0);p->sequence=++c->sequence;p->device_ms=now;p->battery=255;p->kind=kind;}
static void remember(dt_controller *c,uint8_t result){unsigned i=c->history_next;c->command_history[i]=c->command_id;c->command_results[i]=result;c->history_next=(i+1)%64;if(c->history_count<64)c->history_count++;}
static void ack(dt_controller *c,dt_packet *p,uint8_t result,uint32_t now){packet(c,p,129,result,now);p->event_id=c->command_id;}
dt_motion_config dt_motion_defaults(void){dt_motion_config c={{1,0,0,0,1,0,0,0,1},.25f,220,80,180,200,64,false,100,20,1000,2000};return c;}
bool dt_controller_init(dt_controller *c,dt_imu_port port,dt_motion_config config){
 if(!c||!port.read||!isfinite(config.filter_alpha)||config.filter_alpha<=0||config.filter_alpha>1||!isfinite(config.trigger_dps)||config.trigger_dps<=0||!isfinite(config.release_dps)||config.release_dps<0||config.release_dps>=config.trigger_dps||!isfinite(config.serve_dps)||config.serve_dps<=0||config.calibration_samples<8||config.calibration_samples>4096||config.cooldown_ms<20||config.cooldown_ms>5000||config.max_gap_ms<10||config.max_gap_ms>1000||config.min_swing_ms<10||config.max_swing_ms<=config.min_swing_ms||config.max_swing_ms>5000||!isfinite(config.max_power_dps)||config.max_power_dps<=config.trigger_dps)return false;
 for(int row=0;row<3;row++){for(int col=0;col<3;col++){float dot=0;for(int k=0;k<3;k++){float v=config.axis[row*3+k];if(!isfinite(v))return false;dot+=v*config.axis[col*3+k];}if(fabsf(dot-(row==col?1:0))>.001f)return false;}}
 memset(c,0,sizeof(*c));c->imu=port;c->config=config;return true;
}
bool dt_controller_command(dt_controller *c,const dt_packet *command,uint32_t now,dt_packet *response){
 if(!c||!command||!response||dt_validate(command)!=DT_OK)return false;
 if(command->type==130){packet(c,response,131,(uint8_t)c->input_profile,now);response->event_id=command->event_id;return true;}
 if(command->type!=128)return false;
 if(c->calibrating){packet(c,response,129,1,now);response->event_id=command->event_id;return true;}
 for(unsigned i=0;i<c->history_count;i++){if(c->command_history[i]==command->event_id){packet(c,response,129,c->command_results[i],now);response->event_id=command->event_id;return true;}}
 c->command_id=command->event_id;
 if(!c->ready){ack(c,response,2,now);remember(c,2);return true;}
 c->calibrating=true;c->calibrated=false;c->latched=c->swinging=false;c->samples=0;c->calibration_started=now;memset(c->sum,0,sizeof(c->sum));return false;
}
bool dt_controller_button(dt_controller *c,uint8_t kind,uint32_t now,dt_packet *out){
 if(!c||!out||kind<1||kind>7||(kind>3&&c->input_profile!=DT_INPUT_BUTTONS))return false;
 packet(c,out,2,kind,now);out->event_id=++c->event_id;return true;
}
bool dt_controller_set_mode(dt_controller *c,uint8_t kind){
 if(!c||kind>8)return false;
 c->shot_mode=kind;c->swinging=false;c->latched=true;return true;
}
bool dt_controller_set_rules(dt_controller *c,const dt_motion_rule *rules,unsigned count){
 if(!c||count>8||(count&&!rules))return false;
 for(unsigned i=0;i<count;i++){
  const dt_motion_rule *r=&rules[i];
  if(r->kind<1||r->kind>8||!isfinite(r->min_yaw)||!isfinite(r->max_yaw)||!isfinite(r->min_pitch)||!isfinite(r->max_pitch)||!isfinite(r->min_peak)||!isfinite(r->max_peak)||r->min_yaw < -180||r->max_yaw>180||r->min_yaw>r->max_yaw||r->min_pitch < -90||r->max_pitch>90||r->min_pitch>r->max_pitch||r->min_peak<0||r->min_peak>r->max_peak||r->min_ms>r->max_ms||r->max_ms>5000)return false;
 }
 if(count){memcpy(c->rules,rules,count*sizeof(*rules));}c->rule_count=count;c->swinging=false;c->latched=true;return true;
}
static uint8_t classify(const dt_controller *c,uint32_t duration){
 if(c->shot_mode)return c->shot_mode;
 uint8_t found=0;
 for(unsigned i=0;i<c->rule_count;i++){
  const dt_motion_rule *r=&c->rules[i];
  if(c->yaw>=r->min_yaw&&c->yaw<=r->max_yaw&&c->pitch>=r->min_pitch&&c->pitch<=r->max_pitch&&c->peak>=r->min_peak&&c->peak<=r->max_peak&&duration>=r->min_ms&&duration<=r->max_ms){if(found&&found!=r->kind)return 0;found=r->kind;}
 }
 if(c->rule_count)return found; // A configured profile rejects unknown/ambiguous gestures.
 return c->peak_y>=c->config.serve_dps?3:c->peak_z>=0?1:2;
}
bool dt_controller_intent(dt_controller *c,uint8_t kind,uint32_t now,dt_packet *out){
 if(!c||!out||!c->ready||!c->calibrated||c->calibrating||kind<1||kind>8)return false;
 packet(c,out,1,kind,now);out->event_id=++c->event_id;out->power=50;return true;
}
bool dt_controller_tick(dt_controller *c,dt_packet *out){
 if(!c||!out||c->input_profile==DT_INPUT_BUTTONS)return false;
 dt_imu_sample s={0};bool valid=c->imu.read(c->imu.context,&s)&&finite_sample(&s);
 if(!valid){bool changed=c->ready;c->ready=c->calibrated=c->swinging=false;if(c->calibrating){c->calibrating=false;ack(c,out,3,c->last_time);remember(c,3);return true;}if(changed){packet(c,out,3,0,c->last_time);return true;}return false;}
 uint32_t elapsed=c->has_time?(uint32_t)(s.time_ms-c->last_time):0;
 if(c->has_time&&elapsed>c->config.max_gap_ms){c->calibrated=c->swinging=false;memset(c->filtered,0,sizeof(c->filtered));}
 if(c->has_time&&s.time_ms==c->last_time)return false;
 c->ready=true;c->last_time=s.time_ms;c->has_time=true;
 if(c->calibrating){
  if((uint32_t)(s.time_ms-c->calibration_started)>=5000){c->calibrating=false;ack(c,out,3,s.time_ms);remember(c,3);return true;}
  if(magnitude(s.gyro_dps)>15||fabsf(magnitude(s.acceleration)-9.80665f)>1){c->samples=0;memset(c->sum,0,sizeof(c->sum));return false;}
  for(int i=0;i<3;i++)c->sum[i]+=s.gyro_dps[i];
  if(++c->samples>=c->config.calibration_samples){for(int i=0;i<3;i++)c->bias[i]=c->sum[i]/c->samples;c->calibrated=true;c->calibrating=false;memset(c->filtered,0,sizeof(c->filtered));ack(c,out,0,s.time_ms);remember(c,0);return true;}return false;
 }
 if(c->calibrated){
  float mapped[3]={0};for(int row=0;row<3;row++){for(int col=0;col<3;col++)mapped[row]+=c->config.axis[row*3+col]*(s.gyro_dps[col]-c->bias[col]);}
  if(c->config.left_handed)mapped[2]=-mapped[2];
  for(int i=0;i<3;i++)c->filtered[i]+=c->config.filter_alpha*(mapped[i]-c->filtered[i]);
  float speed=magnitude(c->filtered);
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
    if(kind){packet(c,out,1,kind,s.time_ms);out->event_id=++c->event_id;out->power=(uint8_t)clamp(100*(c->peak-c->config.release_dps)/(c->config.max_power_dps-c->config.release_dps),1,100);out->yaw_cdeg=(int16_t)lroundf(c->yaw*100);out->pitch_cdeg=(int16_t)lroundf(c->pitch*100);return true;}
   }
  }
 }
 if((uint32_t)(s.time_ms-c->status_at)>=1000){c->status_at=s.time_ms;packet(c,out,3,0,s.time_ms);return true;}return false;
}
