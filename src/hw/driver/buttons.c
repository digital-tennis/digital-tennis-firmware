#include "dt_ble.h"
#include "dt_button.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(racket_buttons,LOG_LEVEL_INF);
// No guessed GPIOs: a reviewed board overlay must expose these optional aliases.
static const struct gpio_dt_spec pins[5]={
#if DT_NODE_HAS_STATUS(DT_ALIAS(controller_button_1),okay)
 [0]=GPIO_DT_SPEC_GET(DT_ALIAS(controller_button_1),gpios),
#endif
#if DT_NODE_HAS_STATUS(DT_ALIAS(controller_button_2),okay)
 [1]=GPIO_DT_SPEC_GET(DT_ALIAS(controller_button_2),gpios),
#endif
#if DT_NODE_HAS_STATUS(DT_ALIAS(controller_button_3),okay)
 [2]=GPIO_DT_SPEC_GET(DT_ALIAS(controller_button_3),gpios),
#endif
#if DT_NODE_HAS_STATUS(DT_ALIAS(controller_button_4),okay)
 [3]=GPIO_DT_SPEC_GET(DT_ALIAS(controller_button_4),gpios),
#endif
#if DT_NODE_HAS_STATUS(DT_ALIAS(controller_button_5),okay)
 [4]=GPIO_DT_SPEC_GET(DT_ALIAS(controller_button_5),gpios),
#endif
};
static dt_button states[5];static bool enabled[5];
void dt_buttons_init(void){for(unsigned i=0;i<5;i++)if(pins[i].port){enabled[i]=gpio_is_ready_dt(&pins[i])&&gpio_pin_configure_dt(&pins[i],GPIO_INPUT)==0;if(!enabled[i])LOG_WRN("Button %u unavailable",i);}}
bool dt_buttons_poll(dt_controller *c,uint32_t now,dt_packet *p){
 for(unsigned i=0;i<5;i++){
  if(!enabled[i])continue;
  int value=gpio_pin_get_dt(&pins[i]);if(value<0)continue;
  dt_button_edge edge=dt_button_edge_poll(&states[i],value!=0,now);
  if(edge==DT_EDGE_NONE)continue;
  return dt_controller_button_edge(c,(uint8_t)(i+1),edge==DT_EDGE_DOWN,now,p);
 }
 return false;
}
