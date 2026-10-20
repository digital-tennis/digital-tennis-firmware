#include "dt_button.h"
dt_button_edge dt_button_edge_poll(dt_button *b,bool pressed,uint32_t now){
 if(!b)return DT_EDGE_NONE;
 if(!b->initialized){*b=(dt_button){.initialized=true,.raw=pressed,.stable=pressed,.armed=!pressed,.changed_at=now};return DT_EDGE_NONE;}
 if(pressed!=b->raw){b->raw=pressed;b->changed_at=now;}
 if((uint32_t)(now-b->changed_at)<30||b->stable==b->raw)return DT_EDGE_NONE;
 b->stable=b->raw;
 if(!b->stable){bool release=b->reported;b->reported=false;b->armed=true;return release?DT_EDGE_UP:DT_EDGE_NONE;}
 bool emit=b->armed;b->armed=false;b->reported=emit;return emit?DT_EDGE_DOWN:DT_EDGE_NONE;
}
bool dt_button_poll(dt_button *b,bool pressed,uint32_t now){return dt_button_edge_poll(b,pressed,now)==DT_EDGE_DOWN;}
