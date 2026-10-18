#include "dt_ble.h"
#include "dt_button.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(racket_buttons,LOG_LEVEL_INF);
// No guessed GPIOs: a reviewed board overlay must expose these optional aliases.
static const struct gpio_dt_spec pins[5]={
#ifdef DT_BUTTON_PROFILE
 [0]=GPIO_DT_SPEC_GET(DT_ALIAS(racket_serve),gpios),
 [1]=GPIO_DT_SPEC_GET(DT_ALIAS(racket_forehand),gpios),
 [2]=GPIO_DT_SPEC_GET(DT_ALIAS(racket_backhand),gpios),
 [3]=GPIO_DT_SPEC_GET(DT_ALIAS(racket_skill),gpios),
#else
#if DT_NODE_HAS_STATUS(DT_ALIAS(racket_confirm),okay)
 [0]=GPIO_DT_SPEC_GET(DT_ALIAS(racket_confirm),gpios),
#endif
#if DT_NODE_HAS_STATUS(DT_ALIAS(racket_start),okay)
 [1]=GPIO_DT_SPEC_GET(DT_ALIAS(racket_start),gpios),
#endif
#if DT_NODE_HAS_STATUS(DT_ALIAS(racket_pause),okay)
 [2]=GPIO_DT_SPEC_GET(DT_ALIAS(racket_pause),gpios),
#endif
#if DT_NODE_HAS_STATUS(DT_ALIAS(racket_skill),okay)
 [3]=GPIO_DT_SPEC_GET(DT_ALIAS(racket_skill),gpios),
#endif
#if DT_NODE_HAS_STATUS(DT_ALIAS(racket_shot),okay)
 [4]=GPIO_DT_SPEC_GET(DT_ALIAS(racket_shot),gpios),
#endif
#endif
};
static dt_button states[5];static bool enabled[5];
void dt_buttons_init(void){for(unsigned i=0;i<5;i++)if(pins[i].port){enabled[i]=gpio_is_ready_dt(&pins[i])&&gpio_pin_configure_dt(&pins[i],GPIO_INPUT)==0;if(!enabled[i])LOG_WRN("Button %u unavailable",i);}}
bool dt_buttons_poll(dt_controller *c,uint32_t now,dt_packet *p){
 for(unsigned i=0;i<5;i++){
  if(!enabled[i])continue;
  int value=gpio_pin_get_dt(&pins[i]);if(value<0)continue;
  if(!dt_button_poll(&states[i],value!=0,now))continue;
 #ifdef DT_BUTTON_PROFILE
  static const uint8_t kinds[]={DT_BUTTON_SERVE,DT_BUTTON_FOREHAND,DT_BUTTON_BACKHAND,DT_BUTTON_SKILL};
  return dt_controller_button(c,kinds[i],now,p);
 #else
  if(i<3)return dt_controller_button(c,(uint8_t)(i+1),now,p);
  if(i==3)return dt_controller_intent(c,8,now,p);
  static const uint8_t modes[]={0,4,5,6,7};unsigned index=0;
  while(index<4&&modes[index]!=c->shot_mode)index++;
  dt_controller_set_mode(c,modes[(index+1)%5]);LOG_INF("Shot profile %u",c->shot_mode);
 #endif
 }
 return false;
}
