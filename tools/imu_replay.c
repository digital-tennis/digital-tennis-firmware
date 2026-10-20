#include "dt_controller.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
static bool read_row(void *context,dt_imu_sample *sample){*sample=*(dt_imu_sample *)context;return true;}
int main(int argc,char **argv){
 if(argc!=2){fprintf(stderr,"usage: imu_replay samples.csv (time_ms,ax_mps2,ay_mps2,az_mps2,gx_dps,gy_dps,gz_dps; first 65+ rows stationary)\n");return 2;}
 FILE *file=fopen(argv[1],"r");if(!file){perror(argv[1]);return 2;}
 dt_imu_sample sample={0};dt_controller c;dt_packet packet;char line[512];unsigned row=0;bool requested=false;
 if(!dt_controller_init(&c,(dt_imu_port){&sample,read_row},dt_motion_peak_defaults()))return 2;
 puts("time_ms,kind,power,yaw_cdeg,pitch_cdeg,packet_hex");
 while(fgets(line,sizeof(line),file)){
  row++;if(row==1&&strncmp(line,"time_ms,",8)==0)continue;
  char extra;unsigned long long time;
  int fields=sscanf(line,"%llu,%f,%f,%f,%f,%f,%f %c",&time,&sample.acceleration[0],&sample.acceleration[1],&sample.acceleration[2],&sample.gyro_dps[0],&sample.gyro_dps[1],&sample.gyro_dps[2],&extra);
  if(fields!=7||time>UINT32_MAX){fprintf(stderr,"invalid CSV row %u\n",row);fclose(file);return 2;}
  sample.time_ms=(uint32_t)time;
  for(unsigned i=0;i<3;i++)if(!isfinite(sample.acceleration[i])||!isfinite(sample.gyro_dps[i])){fprintf(stderr,"non-finite CSV row %u\n",row);fclose(file);return 2;}
  bool emit=dt_controller_tick(&c,&packet);
  if(!requested&&c.ready){dt_packet command={.version=2,.type=128,.event_id=1,.battery=255};(void)dt_controller_command(&c,&command,sample.time_ms,&packet);requested=true;}
  if(emit&&packet.type==1){
   uint8_t wire[20];if(dt_encode(&packet,wire,20)!=DT_OK){fclose(file);return 2;}
   printf("%u,%u,%u,%d,%d,",packet.device_ms,packet.kind,packet.power,packet.yaw_cdeg,packet.pitch_cdeg);
   for(unsigned i=0;i<20;i++)printf("%02x",wire[i]);
   puts("");
  }
 }
 fclose(file);if(!c.calibrated){fprintf(stderr,"replay ended uncalibrated: check stationary prefix, gaps and sensor validity\n");return 1;}return 0;
}
