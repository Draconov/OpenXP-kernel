#ifndef OPENXP_INPUT_H
#define OPENXP_INPUT_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { INPUT_NONE=0, INPUT_KEY, INPUT_MOVE, INPUT_BUTTON } input_kind_t;
typedef enum {
    KEY_NONE=0, KEY_CHARACTER, KEY_ENTER, KEY_TAB, KEY_BACKSPACE, KEY_ESCAPE,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_HOME, KEY_END, KEY_DELETE,
    KEY_F2, KEY_F4, KEY_F5, KEY_WINDOWS
} input_keycode_t;
enum { KEYMOD_SHIFT=1, KEYMOD_CTRL=2, KEYMOD_ALT=4, KEYMOD_WIN=8 };
typedef struct {
    input_kind_t kind;
    char key; int x,y; bool left,right;
    input_keycode_t code;
    uint8_t modifiers;
} input_event_t;
typedef struct {
    uint8_t state, bytes[3]; int x,y,width,height;
    bool left,right,extended,shift,caps,ctrl,alt,win;
    bool left_shift,right_shift;
} input_device_t;
void input_state_init(input_device_t *state,int width,int height);
/* Pure scancode and mouse packet decoders, individually host-tested. */
bool input_decode_keyboard(input_device_t *state,uint8_t code,input_event_t *out);
bool input_decode_mouse(input_device_t *state,uint8_t byte,input_event_t *out);
/* One producer (IRQ1/IRQ12 with interrupts disabled) and one foreground consumer.
   Raw PS/2 bytes are decoded OUTSIDE interrupt context. */
enum { INPUT_RAW_CAPACITY = 256 };
typedef struct { uint8_t value, auxiliary; } input_raw_byte_t;
typedef struct {
    input_raw_byte_t bytes[INPUT_RAW_CAPACITY];
    volatile unsigned head, tail, dropped;
} input_raw_queue_t;
void input_queue_reset(input_raw_queue_t *q);
bool input_queue_push(input_raw_queue_t *q,uint8_t byte,bool auxiliary);
bool input_queue_pop(input_raw_queue_t *q,input_raw_byte_t *byte);
void input_hardware_init(void);
bool input_irq_ready(void);
/* Called by IRQ stubs; never calls the GUI or allocates memory. */
void input_irq_dispatch(unsigned irq);

/* Returns true if ONE controller byte was consumed (event may be INPUT_NONE).
   This distinction is essential for draining all PS/2 packet bytes promptly. */
bool input_poll(input_device_t *state,input_event_t *out);
#endif
