#ifndef OPENXP_INPUT_H
#define OPENXP_INPUT_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { INPUT_NONE=0, INPUT_KEY, INPUT_MOVE, INPUT_BUTTON } input_kind_t;
typedef struct { input_kind_t kind; char key; int x,y; bool left; } input_event_t;
typedef struct { uint8_t state, bytes[3]; int x,y,width,height; bool left,extended,shift,caps; } input_device_t;
void input_state_init(input_device_t *state,int width,int height);
/* Pure scancode and mouse packet decoders, individually host-tested. */
bool input_decode_keyboard(input_device_t *state,uint8_t code,input_event_t *out);
bool input_decode_mouse(input_device_t *state,uint8_t byte,input_event_t *out);
void input_hardware_init(void);
bool input_poll(input_device_t *state,input_event_t *out);
#endif
