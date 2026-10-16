#ifndef OPENXP_FILE_DROP_H
#define OPENXP_FILE_DROP_H
#include "file_workflow.h"
#include "advanced_input.h"
/* Intra-Explorer drag. The only writable target is Documents/HOME.
 * System files are copied; HOME is already the same directory as Documents. */
static inline int oxp_drop_documents_target(unsigned x,unsigned y,unsigned side,
                                             unsigned client_h){
    return x<side && y>=67u && y<84u && client_h>=103u;
}
static inline int oxp_drop_source_row(unsigned x,unsigned y,unsigned side,
                                      unsigned client_w,unsigned client_h,
                                      unsigned scroll){
    if(x<side+3u || x>=client_w-22u || y<49u || client_h<=69u ||
       y>=client_h-19u)return -1;
    return (int)(scroll+(y-49u)/15u);
}
static inline int oxp_drop_should_copy(const char *source){
    return source && *source && !oxp_doc_home(source);
}
#endif
