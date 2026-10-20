#ifndef DT_GESTURE_H
#define DT_GESTURE_H
#include <stdbool.h>
#include "dt_imu.h"
// Physical features only: no match state enters this API.
typedef enum {DT_GESTURE_OK,DT_GESTURE_SHORT,DT_GESTURE_UNMATCHED,DT_GESTURE_AMBIGUOUS,DT_GESTURE_RESERVED,DT_GESTURE_INVALID} dt_gesture_reason;
typedef struct {float direction_x,direction_z,sweep_deg,active_ms,coherence,peak_g,peak_dps;} dt_gesture_features;
typedef struct {uint8_t kind;uint16_t candidates;dt_gesture_reason reason;dt_gesture_features features;} dt_gesture_result;
typedef struct {uint8_t kind;float min_x,max_x,min_z,max_z,min_abs_z,max_abs_z,min_sweep,max_sweep,min_ms,max_ms,min_coherence,min_g,max_g;} dt_gesture_rule;
dt_gesture_result dt_gesture_classify(const dt_imu_sample *samples,unsigned count,const float axis[9],const float bias[3],bool left_handed,float release_dps);
dt_gesture_result dt_gesture_classify_ring(const dt_imu_sample *samples,unsigned count,unsigned head,unsigned capacity,const float axis[9],const float bias[3],bool left_handed,float release_dps);
#endif
