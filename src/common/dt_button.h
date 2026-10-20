#ifndef DT_BUTTON_H
#define DT_BUTTON_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { bool initialized,raw,stable,armed,reported; uint32_t changed_at; } dt_button;
typedef enum { DT_EDGE_NONE,DT_EDGE_DOWN,DT_EDGE_UP } dt_button_edge;
dt_button_edge dt_button_edge_poll(dt_button *button,bool pressed,uint32_t now);
// A button held at boot must be released before it can produce an event.
bool dt_button_poll(dt_button *button,bool pressed,uint32_t now);
#endif
