#include "dt_lsm6ds3tr_c.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct {
 uint8_t reg[256];
 unsigned calls,fail_at,waits,writes;
 bool stuck_reset,ignore_configuration;
} fixture;
static bool read_reg(void *context,uint8_t reg,uint8_t *out,size_t size){
 fixture *f=context;
 assert((size_t)reg+size<=sizeof f->reg);
 if(++f->calls==f->fail_at){if(size)out[0]=0xff;return false;}
 memcpy(out,f->reg+reg,size);
 if(reg==0x22)f->reg[0x1e]=0;
 return true;
}
static bool write_reg(void *context,uint8_t reg,uint8_t value){
 fixture *f=context;f->writes++;
 if(++f->calls==f->fail_at)return false;
 if(f->ignore_configuration && !(reg==0x12 && value==1))return true;
 if(reg==0x12 && value==1){
  memset(f->reg+0x10,0,3);
  f->reg[0x12]=1;
 }else f->reg[reg]=value;
 return true;
}
static void sleep_ms(void *context,unsigned ms){
 fixture *f=context;assert(ms==1);f->waits++;
 if(!f->stuck_reset)f->reg[0x12]&=~1;
}
static dt_lsm6ds3tr_c_bus bus(fixture *f){return (dt_lsm6ds3tr_c_bus){f,read_reg,write_reg,sleep_ms};}
static fixture fresh(void){fixture f={0};f.reg[0x0f]=0x6a;return f;}
static void put(fixture *f,unsigned reg,int value){
 uint16_t raw=(uint16_t)value;f->reg[reg]=(uint8_t)raw;f->reg[reg+1]=(uint8_t)(raw>>8);
}
int main(void){
 fixture f=fresh();dt_lsm6ds3tr_c_bus b=bus(&f);
 assert(dt_lsm6ds3tr_c_init(&b));
 assert(f.reg[0x12]==0x44 && f.reg[0x10]==0x64 && f.reg[0x11]==0x6c);
 unsigned operations=f.calls;
 for(unsigned i=1;i<=operations;i++){
  f=fresh();f.fail_at=i;b=bus(&f);assert(!dt_lsm6ds3tr_c_init(&b));
 }
 for(unsigned id=0;id<256;id++)if(id!=0x6a){
  f=fresh();f.reg[0x0f]=(uint8_t)id;b=bus(&f);
  assert(!dt_lsm6ds3tr_c_init(&b));assert(f.writes==0);
 }
 f=fresh();f.stuck_reset=true;b=bus(&f);
 assert(!dt_lsm6ds3tr_c_init(&b));assert(f.waits==20);
 f=fresh();f.ignore_configuration=true;b=bus(&f);assert(!dt_lsm6ds3tr_c_init(&b));
 assert(!dt_lsm6ds3tr_c_init(NULL));
 b=(dt_lsm6ds3tr_c_bus){0};assert(!dt_lsm6ds3tr_c_init(&b));

 f=fresh();b=bus(&f);assert(dt_lsm6ds3tr_c_init(&b));
 /* Gyro first, accel second. Exercise signed extremes and distinct axes. */
 put(&f,0x22,32767);put(&f,0x24,-32768);put(&f,0x26,-1);
 put(&f,0x28,2049);put(&f,0x2a,-2049);put(&f,0x2c,0);
 f.reg[0x1e]=3;dt_imu_sample sample={0};
 assert(dt_lsm6ds3tr_c_read(&b,UINT32_MAX,&sample));
 assert(fabsf(sample.gyro_dps[0]-2293.69f)<.001f);
 assert(fabsf(sample.gyro_dps[1]+2293.76f)<.001f);
 assert(fabsf(sample.gyro_dps[2]+.07f)<.0001f);
 assert(fabsf(sample.acceleration[0]-9.80582f)<.001f);
 assert(fabsf(sample.acceleration[1]+9.80582f)<.001f);
 assert(sample.acceleration[2]==0 && sample.time_ms==UINT32_MAX);
 dt_imu_sample saved=sample;
 for(unsigned status=0;status<3;status++){
  f.reg[0x1e]=(uint8_t)status;
  assert(!dt_lsm6ds3tr_c_read(&b,0,&sample));assert(!memcmp(&sample,&saved,sizeof sample));
 }
 for(unsigned offset=1;offset<=2;offset++){
  f.reg[0x1e]=3;f.fail_at=f.calls+offset;
  assert(!dt_lsm6ds3tr_c_read(&b,1,&sample));assert(!memcmp(&sample,&saved,sizeof sample));
 }
 f.fail_at=0;f.reg[0x1e]=3;
 assert(dt_lsm6ds3tr_c_read(&b,0,&sample));assert(sample.time_ms==0);
 assert(!dt_lsm6ds3tr_c_read(&b,10,&sample)); /* no stale replay */
 assert(!dt_lsm6ds3tr_c_read(&b,10,NULL));
 puts("LSM6DS3TR-C mock bus: identity, bounded reset, all init I/O failures, readback, units, signed axes, freshness and atomic output passed.");
 return 0;
}
