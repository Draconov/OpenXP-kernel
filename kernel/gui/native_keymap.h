#ifndef OPENXP_NATIVE_KEYMAP_H
#define OPENXP_NATIVE_KEYMAP_H
#include "editor_keycodes.h"
#include "input/input.h"
/* Only the focused native owner gets these events. The shell keeps its
 * global Win, Alt+Tab, and Ctrl+Esc shortcuts before this translation. */
static inline uint32_t native_editor_keymap(const input_event_t *e){
    unsigned mods=e->modifiers;
    if(mods&(KEYMOD_ALT|KEYMOD_WIN))return 0;
    char c=e->key;
    if(c>='A'&&c<='Z')c=(char)(c+('a'-'A'));
    if(mods&KEYMOD_CTRL){
        if(e->code==KEY_HOME)return (mods&KEYMOD_SHIFT)?OXP_KEY_SELECT_DOC_HOME:OXP_KEY_DOC_HOME;
        if(e->code==KEY_END)return (mods&KEYMOD_SHIFT)?OXP_KEY_SELECT_DOC_END:OXP_KEY_DOC_END;
        if(e->code!=KEY_CHARACTER)return 0;
        switch(c){
        case 'a':return OXP_KEY_SELECT_ALL;
        case 'c':return OXP_KEY_COPY;
        case 'x':return OXP_KEY_CUT;
        case 'v':return OXP_KEY_PASTE;
        case 'z':return (mods&KEYMOD_SHIFT)?OXP_KEY_REDO:OXP_KEY_UNDO;
        case 'y':return OXP_KEY_REDO;
        default:return 0;
        }
    }
    if(e->code==KEY_LEFT)return (mods&KEYMOD_SHIFT)?OXP_KEY_SELECT_LEFT:0;
    if(e->code==KEY_RIGHT)return (mods&KEYMOD_SHIFT)?OXP_KEY_SELECT_RIGHT:0;
    if(e->code==KEY_UP)return (mods&KEYMOD_SHIFT)?OXP_KEY_SELECT_UP:0;
    if(e->code==KEY_DOWN)return (mods&KEYMOD_SHIFT)?OXP_KEY_SELECT_DOWN:0;
    if(e->code==KEY_HOME)return (mods&KEYMOD_SHIFT)?OXP_KEY_SELECT_HOME:OXP_KEY_HOME;
    if(e->code==KEY_END)return (mods&KEYMOD_SHIFT)?OXP_KEY_SELECT_END:OXP_KEY_END;
    if(e->code==KEY_DELETE)return OXP_KEY_DELETE;
    return 0;
}
#endif
