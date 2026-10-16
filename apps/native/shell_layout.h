#ifndef OPENXP_SHELL_LAYOUT_H
#define OPENXP_SHELL_LAYOUT_H
/* Aurora 4 shell geometry is shared by layout, mouse input and host tests.
 * Keep the logic in Ring 3; hit-tests do not depend on pixel colors. */
#define OXP_DESKTOP_COUNT 5u
#define OXP_DESKTOP_X 14u
#define OXP_DESKTOP_TOP 23u
#define OXP_DESKTOP_STEP 109u
#define OXP_DESKTOP_SLOT_W 101u
#define OXP_DESKTOP_SLOT_H 92u
#define OXP_MENU_X 4u
#define OXP_MENU_Y 356u
#define OXP_MENU_W 514u
#define OXP_MENU_H 370u
#define OXP_MENU_ROW_X 13u
#define OXP_MENU_ROW_Y 410u
#define OXP_MENU_COLUMN_W 247u
#define OXP_MENU_ROW_H 39u
#define OXP_MENU_ROW_STEP 40u
#define OXP_MENU_ITEMS 14u
static inline int oxp_desktop_hit(unsigned x,unsigned y){
    if(x<OXP_DESKTOP_X || x>=OXP_DESKTOP_X+OXP_DESKTOP_SLOT_W ||
       y<OXP_DESKTOP_TOP)return -1;
    unsigned offset=y-OXP_DESKTOP_TOP;
    unsigned index=offset/OXP_DESKTOP_STEP;
    if(index>=OXP_DESKTOP_COUNT || offset%OXP_DESKTOP_STEP>=OXP_DESKTOP_SLOT_H)return -1;
    return (int)index;
}
static inline int oxp_menu_hit(unsigned x,unsigned y){
    if(x<OXP_MENU_ROW_X || y<OXP_MENU_ROW_Y)return -1;
    unsigned cx=x-OXP_MENU_ROW_X;
    unsigned cy=y-OXP_MENU_ROW_Y;
    unsigned col=cx/OXP_MENU_COLUMN_W;
    unsigned row=cy/OXP_MENU_ROW_STEP;
    if(col>=2u || row>=7u || cx%OXP_MENU_COLUMN_W>=234u ||
       cy%OXP_MENU_ROW_STEP>=OXP_MENU_ROW_H)return -1;
    return (int)(col*7u+row);
}
static inline unsigned oxp_desktop_app(unsigned slot){
    static const unsigned programs[OXP_DESKTOP_COUNT]={1u,0u,5u,7u,12u};
    return slot<OXP_DESKTOP_COUNT?programs[slot]:14u;
}
static inline unsigned oxp_desktop_icon(unsigned slot){
    static const unsigned ids[OXP_DESKTOP_COUNT]={1u,5u,3u,4u,2u};
    return slot<OXP_DESKTOP_COUNT?ids[slot]:0u;
}
static inline unsigned oxp_menu_icon(unsigned app){
    static const unsigned ids[OXP_MENU_ITEMS]={5u,1u,4u,2u,6u,3u,1u,
                                               4u,7u,8u,9u,5u,2u,10u};
    return app<OXP_MENU_ITEMS?ids[app]:0u;
}
#endif
