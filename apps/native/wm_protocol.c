#include "wm_protocol.h"

static void bytes_copy(char *to,const char *from,size_t count){
    for(size_t i=0;i<count;++i)to[i]=from[i];
}
static void title_copy(char to[WM_TITLE_SIZE],const char from[WM_TITLE_SIZE]){
    unsigned i=0;
    while(i+1<WM_TITLE_SIZE && from[i] && (uint8_t)from[i]>=32u && (uint8_t)from[i]<=126u){
        to[i]=from[i];++i;
    }
    to[i]=0;
    for(++i;i<WM_TITLE_SIZE;++i)to[i]=0;
}
static uint32_t id_for(const wm_window_t *w,unsigned slot){
    return ((w->generation & UINT32_C(0x00ffffff))<<8)|(slot+1u);
}
static void release(wm_window_t *w){
    uint32_t next=(w->generation+1u)&UINT32_C(0x00ffffff);
    if(!next)next=1u;
    *w=(wm_window_t){0};w->generation=next;
}
static wm_window_t *owned(wm_state_t *s,uint64_t owner,uint32_t id){
    unsigned slot=(id & 0xffu);
    if(!owner||!slot||slot>WM_MAX_WINDOWS)return NULL;
    wm_window_t *w=&s->windows[slot-1u];
    return w->used && w->generation==(id>>8) && w->owner==owner?w:NULL;
}
static void detail(const wm_window_t *w,unsigned slot,wm_packet_t *reply){
    reply->window_id=id_for(w,slot);
    reply->x=w->x;reply->y=w->y;
    reply->width=w->width;reply->height=w->height;
    bytes_copy(reply->title,w->title,WM_TITLE_SIZE);
}
void wm_state_init(wm_state_t *s){
    if(!s)return;
    *s=(wm_state_t){0};s->next_z=1;
    for(unsigned i=0;i<WM_MAX_WINDOWS;++i)s->windows[i].generation=1;
}
void wm_state_disconnect(wm_state_t *s,uint64_t owner){
    if(!s||!owner)return;
    for(unsigned i=0;i<WM_MAX_WINDOWS;++i)
        if(s->windows[i].used && s->windows[i].owner==owner)release(&s->windows[i]);
}
uint32_t wm_state_active_window(const wm_state_t *s){
    if(!s)return 0;
    uint32_t z=0,id=0;
    for(unsigned i=0;i<WM_MAX_WINDOWS;++i)
        if(s->windows[i].used && s->windows[i].z>=z){
            z=s->windows[i].z;id=id_for(&s->windows[i],i);
        }
    return id;
}
static void raise(wm_state_t *s,wm_window_t *w){
    /* Keep ordering defined even after the counter wraps. */
    if(s->next_z>=UINT32_MAX-WM_MAX_WINDOWS-1u){
        for(unsigned i=0;i<WM_MAX_WINDOWS;++i){
            wm_window_t *entry=&s->windows[i];
            if(entry->used)entry->z=1u+(entry->z%WM_MAX_WINDOWS);
        }
        s->next_z=WM_MAX_WINDOWS+2u;
    }
    w->z=s->next_z++;
}
void wm_state_process(wm_state_t *s,uint64_t owner,const wm_packet_t *request,wm_packet_t *reply){
    if(!reply)return;
    *reply=(wm_packet_t){.magic=WM_MAGIC,.version=WM_VERSION,.status=WM_BAD_REQUEST};
    if(!s||!request)return;
    reply->operation=request->operation|WM_REPLY_FLAG;
    if(!owner||request->magic!=WM_MAGIC||request->version!=WM_VERSION ||
       (request->operation & WM_REPLY_FLAG))return;
    if(request->operation==WM_PING){reply->status=WM_OK;return;}
    if(request->operation==WM_CREATE){
        /* One native window per client PID/channel in the current broker.
         * Never advertise multiple logical windows backed by the same surface. */
        for(unsigned i=0;i<WM_MAX_WINDOWS;++i)
            if(s->windows[i].used && s->windows[i].owner==owner){
                reply->status=WM_NO_SLOTS;return;
            }
        if(request->width<120u||request->width>640u ||
           request->height<80u||request->height>480u ||
           request->width>WM_DESKTOP_WIDTH || request->height>WM_DESKTOP_HEIGHT ||
           request->x>WM_DESKTOP_WIDTH-request->width ||
           request->y>WM_DESKTOP_HEIGHT-request->height){
            reply->status=WM_BAD_GEOMETRY;return;
        }
        unsigned slot=0;
        while(slot<WM_MAX_WINDOWS&&s->windows[slot].used)++slot;
        if(slot==WM_MAX_WINDOWS){reply->status=WM_NO_SLOTS;return;}
        wm_window_t *w=&s->windows[slot];
        w->owner=owner;w->used=1;
        w->x=request->x;w->y=request->y;
        w->width=request->width;w->height=request->height;
        title_copy(w->title,request->title);
        raise(s,w);detail(w,slot,reply);
        reply->status=WM_OK;return;
    }
    wm_window_t *w=owned(s,owner,request->window_id);
    if(!w){reply->status=WM_NOT_FOUND;return;}
    unsigned slot=(request->window_id&0xffu)-1u;
    switch(request->operation){
    case WM_MOVE:
        if(request->x>WM_DESKTOP_WIDTH-w->width ||
           request->y>WM_DESKTOP_HEIGHT-w->height){
            reply->status=WM_BAD_GEOMETRY;return;
        }
        w->x=request->x;w->y=request->y;break;
    case WM_FOCUS:raise(s,w);break;
    case WM_QUERY:break;
    case WM_CLOSE:
        release(w);reply->window_id=request->window_id;
        reply->status=WM_OK;return;
    default:return;
    }
    detail(w,slot,reply);reply->status=WM_OK;
}
