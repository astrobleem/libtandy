/* SPDX-License-Identifier: MIT */
#include <dos.h>
#include <string.h>
#include "lt_input_dos.h"
static unsigned input_flags(void);
#pragma aux input_flags = "pushf" "pop ax" value [ax] modify exact [ax]
static unsigned key_ready(void);
#pragma aux key_ready = "mov ah,1" "int 16h" "mov ax,0" \
 "jz short key_done" "inc ax" "key_done:" value [ax] modify [bx cx dx si di es]
static int installed(void)
{
 void (__interrupt __far *v)(void)=_dos_getvect(0x33);
 return (FP_SEG(v)||FP_OFF(v)) && *(unsigned char __far *)v!=0xcf;
}
static void stamp(lte_event *e,unsigned long ticks)
{e->tick_lo=(lte_u16)(ticks&65535UL);e->tick_hi=(lte_u16)((ticks>>16)&65535UL);}
void lti_dos_init(lti_dos_input *i){if(i)memset(i,0,sizeof(*i));}
lti_status lti_dos_mouse_enable(lti_dos_input *i,int confirmed)
{
 if(!i)return LTI_INVALID;
 if(!confirmed){i->mouse_enabled=0;i->mouse_seen=0;return LTI_OK;}
 if(!installed())return LTI_UNSUPPORTED;
 i->mouse_enabled=1;i->mouse_seen=0;return LTI_OK;
}
lti_status lti_dos_mouse(lti_dos_input *i,lte_event *e,unsigned long ticks)
{
 union REGS q;
 if(!i || !e || i->mouse_enabled>1 || i->mouse_seen>1)return LTI_INVALID;
 if(!(input_flags()&0x200U))return LTI_CONTEXT;
 if(!i->mouse_enabled || !installed())return LTI_UNSUPPORTED;
 memset(&q,0,sizeof(q));q.x.ax=3;int86(0x33,&q,&q);
 memset(e,0,sizeof(*e));e->type=LTE_EVENT_MOUSE_STATE;
 e->code=q.x.bx;e->x=(lte_i16)q.x.cx;e->y=(lte_i16)q.x.dx;
 e->flags=(lte_u8)(!i->mouse_seen);stamp(e,ticks);return LTI_OK;
}
lti_status lti_dos_pump(lti_dos_input *i,lte_event_queue *q,unsigned long ticks)
{
 union REGS regs;
 lte_event event;
 lti_status status;
 int delivered=0;
 if(!i || !q || !q->events || !q->capacity || q->capacity>64 ||
    q->head>=q->capacity || q->count>q->capacity ||
    i->mouse_enabled>1 || i->mouse_seen>1)return LTI_INVALID;
 if((unsigned long)FP_OFF(q->events)+(unsigned long)q->capacity*sizeof(lte_event)>65536UL)return LTI_INVALID;
 if(!(input_flags()&0x200U))return LTI_CONTEXT;
 if(q->count==q->capacity)return LTI_FULL;
 memset(&regs,0,sizeof(regs));
 if(key_ready()) {
  regs.h.ah=0;int86(0x16,&regs,&regs);
  memset(&event,0,sizeof(event));event.type=LTE_EVENT_KEY_PRESS;
  event.code=regs.h.al;event.data=regs.h.ah;stamp(&event,ticks);
  if(lte_event_push(q,&event)!=LTE_OK)return LTI_INVALID;
  delivered=1;
 }
 if(!i->mouse_enabled)return delivered?LTI_OK:LTI_EMPTY;
 if(q->count==q->capacity)return LTI_FULL;
 status=lti_dos_mouse(i,&event,ticks);if(status!=LTI_OK)return status;
 if(!i->mouse_seen || i->buttons!=event.code || i->x!=(unsigned short)event.x ||
    i->y!=(unsigned short)event.y) {
  if(lte_event_push(q,&event)!=LTE_OK)return LTI_INVALID;
  i->mouse_seen=1;i->buttons=event.code;i->x=(unsigned short)event.x;
  i->y=(unsigned short)event.y;delivered=1;
 }
 return delivered?LTI_OK:LTI_EMPTY;
}
