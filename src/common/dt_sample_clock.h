#ifndef DT_SAMPLE_CLOCK_H
#define DT_SAMPLE_CLOCK_H
#include <stdbool.h>
#include <stdint.h>
// Absolute deadlines: slow BLE/driver work never adds to the next 10ms period.
// Skip missed periods instead of reading the same sensor repeatedly to catch up.
typedef struct {uint32_t next_ms;bool started;} dt_sample_clock;
static inline bool dt_sample_due(dt_sample_clock *clock,uint32_t now){
 if(!clock->started){clock->started=true;clock->next_ms=now+10;return true;}
 if((int32_t)(now-clock->next_ms)<0)return false;
 clock->next_ms+=((uint32_t)(now-clock->next_ms)/10+1)*10;return true;
}
#endif
