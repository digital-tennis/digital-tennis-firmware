#include "dt_controller.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
static bool read_row(void *context,dt_imu_sample *sample){*sample=*(dt_imu_sample *)context;return true;}
static bool number(const char *text,float *output){char *end;float value=strtof(text,&end);if(end==text||*end||!isfinite(value))return false;*output=value;return true;}
int main(int argc,char **argv){
 dt_motion_config config=dt_motion_peak_defaults();dt_gesture_context context=DT_CONTEXT_UNKNOWN;bool diagnostics=false;const char *path=NULL;
 for(int i=1;i<argc;i++)if(strcmp(argv[i],"--gestures")==0)config=dt_motion_gesture_defaults();
 for(int i=1;i<argc;i++){
  if(strcmp(argv[i],"--gestures")==0){config.acceleration_peak=false;config.recognize_gestures=true;}
  else if(strcmp(argv[i],"--left")==0)config.left_handed=true;
  else if(strcmp(argv[i],"--diagnostics")==0)diagnostics=true;
  else if(strcmp(argv[i],"--context=serve")==0)context=DT_CONTEXT_SERVE;
  else if(strcmp(argv[i],"--context=rally")==0)context=DT_CONTEXT_RALLY;
  else if(strcmp(argv[i],"--context=unknown")==0)context=DT_CONTEXT_UNKNOWN;
  // Host experiment overrides; board defaults come only from reviewed CSV.
  else if(strncmp(argv[i],"--trigger-dps=",14)==0){if(!number(argv[i]+14,&config.trigger_dps))return 2;}
  else if(strncmp(argv[i],"--release-dps=",14)==0){if(!number(argv[i]+14,&config.release_dps))return 2;}
  else if(strncmp(argv[i],"--filter-alpha=",15)==0){if(!number(argv[i]+15,&config.filter_alpha))return 2;}
  else if(strncmp(argv[i],"--axis=",7)==0){char extra;if(sscanf(argv[i]+7,"%f,%f,%f,%f,%f,%f,%f,%f,%f %c",&config.axis[0],&config.axis[1],&config.axis[2],&config.axis[3],&config.axis[4],&config.axis[5],&config.axis[6],&config.axis[7],&config.axis[8],&extra)!=9)return 2;}
  else if(argv[i][0]=='-'||path)return 2;
  else path=argv[i];
 }
 if(!path){fprintf(stderr,"usage: imu_replay [--gestures --context=unknown|serve|rally --left --axis=9commaValues --diagnostics --trigger-dps=N --release-dps=N --filter-alpha=N] samples.csv; first 65+ rows stationary\n");return 2;}
 FILE *file=fopen(path,"r");if(!file){perror(path);return 2;}
 dt_imu_sample sample={0};dt_controller c;dt_packet packet;char line[512];unsigned row=0;bool requested=false;
 if(!dt_controller_init(&c,(dt_imu_port){&sample,read_row},config))return 2;
 if(!dt_controller_set_context(&c,context))return 2;
 puts("time_ms,kind,power,yaw_cdeg,pitch_cdeg,packet_hex,candidates,reason,active_ms,sweep_deg,direction_x,direction_z,coherence,peak_g");
 while(fgets(line,sizeof(line),file)){
  row++;if(row==1&&strncmp(line,"time_ms,",8)==0)continue;
  char extra;unsigned long long time;
  int fields=sscanf(line,"%llu,%f,%f,%f,%f,%f,%f %c",&time,&sample.acceleration[0],&sample.acceleration[1],&sample.acceleration[2],&sample.gyro_dps[0],&sample.gyro_dps[1],&sample.gyro_dps[2],&extra);
  if(fields!=7||time>UINT32_MAX){fprintf(stderr,"invalid CSV row %u\n",row);fclose(file);return 2;}
  sample.time_ms=(uint32_t)time;
  for(unsigned i=0;i<3;i++)if(!isfinite(sample.acceleration[i])||!isfinite(sample.gyro_dps[i])){fprintf(stderr,"non-finite CSV row %u\n",row);fclose(file);return 2;}
  uint32_t attempts=c.gesture_attempts;bool emit=dt_controller_tick(&c,&packet);
  if(!requested&&c.ready){dt_packet command={.version=2,.type=128,.event_id=1,.battery=255};(void)dt_controller_command(&c,&command,sample.time_ms,&packet);requested=true;}
  if((emit&&packet.type==1)||(diagnostics&&attempts!=c.gesture_attempts)){
   if(emit&&packet.type==1){
    uint8_t wire[20];if(dt_encode(&packet,wire,20)!=DT_OK){fclose(file);return 2;}
    printf("%u,%u,%u,%d,%d,",packet.device_ms,packet.kind,packet.power,packet.yaw_cdeg,packet.pitch_cdeg);
    for(unsigned i=0;i<20;i++)printf("%02x",wire[i]);
   }else printf("%u,0,0,0,0,",sample.time_ms);
   const dt_gesture_features *f=&c.gesture_result.features;
   printf(",%u,%u,%.3f,%.3f,%.5f,%.5f,%.5f,%.5f",c.gesture_result.candidates,(unsigned)c.gesture_result.reason,f->active_ms,f->sweep_deg,f->direction_x,f->direction_z,f->coherence,f->peak_g);
   puts("");
  }
 }
 fclose(file);if(!c.calibrated){fprintf(stderr,"replay ended uncalibrated: check stationary prefix, gaps and sensor validity\n");return 1;}return 0;
}
