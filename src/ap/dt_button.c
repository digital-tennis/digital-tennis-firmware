#include "dt_button.h"
bool dt_button_poll(dt_button *b,bool pressed,uint32_t now){
 if(!b)return false;
 if(!b->initialized){*b=(dt_button){.initialized=true,.raw=pressed,.stable=pressed,.armed=!pressed,.changed_at=now};return false;}
 if(pressed!=b->raw){b->raw=pressed;b->changed_at=now;}
 if((uint32_t)(now-b->changed_at)<30||b->stable==b->raw)return false;
 b->stable=b->raw;
 if(!b->stable){b->armed=true;return false;}
 bool emit=b->armed;b->armed=false;return emit;
}
