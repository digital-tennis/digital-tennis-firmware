#include "dt_controller.h"
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
// Explicit developer build only. No address, bond data or game state is recorded.
// Never block sensor acquisition on slow UART; dropped samples keep their gaps.
typedef struct {uint32_t time_ms;int32_t values[6];} trace_row;
K_MSGQ_DEFINE(imu_rows,sizeof(trace_row),32,4);
void dt_imu_trace_submit(const dt_imu_sample *s){
 trace_row row={.time_ms=s->time_ms};
 for(unsigned i=0;i<6;i++){
  float v=i<3?s->acceleration[i]:s->gyro_dps[i-3];
  if(!isfinite(v)||fabsf(v)>8000)return;
  row.values[i]=(int32_t)lroundf(v*1000);
 }
 (void)k_msgq_put(&imu_rows,&row,K_NO_WAIT);
}
static void trace_worker(void *a,void *b,void *c){
 (void)a;(void)b;(void)c;trace_row row;
 for(;;)if(k_msgq_get(&imu_rows,&row,K_FOREVER)==0)
  printk("DTIMU,%u,%d,%d,%d,%d,%d,%d\n",row.time_ms,row.values[0],row.values[1],row.values[2],row.values[3],row.values[4],row.values[5]);
}
K_THREAD_DEFINE(imu_trace_thread,1536,trace_worker,NULL,NULL,NULL,9,0,0);
