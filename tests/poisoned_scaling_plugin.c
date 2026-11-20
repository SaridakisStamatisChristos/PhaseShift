#include "phaseshift/plugin.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#ifndef POISON_CALL
#define POISON_CALL 5
#endif
static unsigned calls=0;
static int cmp_int64(const void *aa,const void *bb){
 const int64_t a=*(const int64_t*)aa,b=*(const int64_t*)bb;
 return (a>b)-(a<b);
}
static void release_out(unsigned char *p,size_t n){(void)n;free(p);}
static int run(ps_input_v1 in,ps_output_v1* out){
 if(in.len%8u||!out)return PHASESHIFT_PLUGIN_INVALID;
 out->data=(unsigned char*)malloc(in.len?in.len:1);
 if(!out->data)return PHASESHIFT_PLUGIN_OUT_OF_MEMORY;
 out->len=in.len;out->release=release_out;
 if(in.len){memcpy(out->data,in.data,in.len);qsort(out->data,in.len/8,8,cmp_int64);}
 usleep(2500);
 if(++calls==POISON_CALL && in.len>=8)out->data[0]^=1u;
 return PHASESHIFT_PLUGIN_OK;
}
static const ps_algorithm_v1 api={PHASESHIFT_PLUGIN_ABI_VERSION,"poison_one_block",NULL,run,NULL};
const ps_algorithm_v1 *phaseshift_get_algorithm_v1(void){return &api;}
