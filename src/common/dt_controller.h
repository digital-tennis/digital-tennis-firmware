#ifndef DT_CONTROLLER_H
#define DT_CONTROLLER_H
#include <stdbool.h>
#include <stdint.h>
#include "dt_protocol.h"
typedef struct { float acceleration[3],gyro_dps[3]; uint32_t time_ms; } dt_imu_sample;
typedef struct { void *context; bool (*read)(void *context,dt_imu_sample *sample); } dt_imu_port;
typedef struct { float axis[9],filter_alpha,trigger_dps,release_dps,serve_dps; uint32_t cooldown_ms; unsigned calibration_samples; bool left_handed; uint32_t max_gap_ms,min_swing_ms,max_swing_ms; float max_power_dps; } dt_motion_config;
typedef struct { uint8_t kind; float min_yaw,max_yaw,min_pitch,max_pitch,min_peak,max_peak; uint32_t min_ms,max_ms; } dt_motion_rule;
typedef struct {
 dt_imu_port imu;dt_motion_config config;
 bool ready,calibrated,calibrating,latched,has_time,swinging;
 dt_input_profile input_profile; uint8_t shot_mode; uint32_t swing_started; float yaw,pitch,peak,peak_y,peak_z;
 dt_motion_rule rules[8];unsigned rule_count;
 float bias[3],sum[3],filtered[3];unsigned samples;
 uint16_t sequence,event_id,command_id;uint32_t last_time,last_swing,calibration_started,status_at;
 uint16_t command_history[64];uint8_t command_results[64];unsigned history_count,history_next;
} dt_controller;
dt_motion_config dt_motion_defaults(void);
bool dt_controller_init(dt_controller *controller,dt_imu_port port,dt_motion_config config);
// No raw sensor values or hardware player roles enter the game protocol.
bool dt_controller_tick(dt_controller *controller,dt_packet *output);
bool dt_controller_command(dt_controller *controller,const dt_packet *command,uint32_t now,dt_packet *response);
bool dt_controller_button(dt_controller *controller,uint8_t kind,uint32_t now,dt_packet *output);
bool dt_controller_set_mode(dt_controller *controller,uint8_t kind);
bool dt_controller_set_rules(dt_controller *controller,const dt_motion_rule *rules,unsigned count);
bool dt_controller_intent(dt_controller *controller,uint8_t kind,uint32_t now,dt_packet *output);
#endif
