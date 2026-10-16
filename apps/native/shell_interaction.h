#ifndef OPENXP_SHELL_INTERACTION_H
#define OPENXP_SHELL_INTERACTION_H
/* Release-only activation: moving outside the original target cancels its
 * action. This pure helper is shared by the Ring 3 shell and host tests. */
static inline int oxp_shell_finish_press(int *pressed,int release_target){
    if(!pressed)return -1;
    int hit=*pressed;
    *pressed=-1;
    return hit>=0 && hit==release_target?hit:-1;
}
#endif
