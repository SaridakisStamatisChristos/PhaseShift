#include "phaseshift/plugin.h"
#include <stdlib.h>
#include <string.h>
#ifndef THRESHOLD_MODE
#define THRESHOLD_MODE 0
#endif
static int validate(ps_input_v1 in){return in.len%8u==0u?PHASESHIFT_PLUGIN_OK:PHASESHIFT_PLUGIN_INVALID;}
static void release_out(unsigned char* p,size_t n){(void)n;free(p);}
static int run(ps_input_v1 in,ps_output_v1* out){
 if(!out || validate(in)!=PHASESHIFT_PLUGIN_OK)return PHASESHIFT_PLUGIN_INVALID;
 if(in.len/8u>=2u){
#if THRESHOLD_MODE == 1
  return PHASESHIFT_PLUGIN_OUT_OF_MEMORY;
#else
  return PHASESHIFT_PLUGIN_ERROR;
#endif
 }
 out->data=(unsigned char*)malloc(in.len?in.len:1u);
 if(!out->data)return PHASESHIFT_PLUGIN_OUT_OF_MEMORY;
 out->len=in.len;out->release=release_out;
 if(in.len)memcpy(out->data,in.data,in.len);
 return PHASESHIFT_PLUGIN_OK;
}
static const ps_algorithm_v1 api={PHASESHIFT_PLUGIN_ABI_VERSION,"threshold_resource_fixture",validate,run,NULL};
const ps_algorithm_v1* phaseshift_get_algorithm_v1(void){return &api;}
