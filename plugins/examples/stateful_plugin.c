#include "phaseshift/plugin.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/* The canonical ascending array must be unchanged. The no-reset variant
 * deliberately poisons its second invocation. A per-call dlopen/dlclose
 * would hide this bug, so this also probes persistent plugin lifecycle. */
static uint64_t invocation_count = 0;
static int validate(ps_input_v1 in) {
 return in.len%8u==0u && in.len<=1048576u ? PHASESHIFT_PLUGIN_OK : PHASESHIFT_PLUGIN_INVALID;
}
static void release_out(unsigned char* b,size_t n){(void)n;free(b);}
static void reset_plugin(void){invocation_count=0;}
static int run(ps_input_v1 in,ps_output_v1* out){
 if(!out || validate(in)!=PHASESHIFT_PLUGIN_OK)return PHASESHIFT_PLUGIN_INVALID;
 out->data=(unsigned char*)malloc(in.len?in.len:1u);
 if(!out->data)return PHASESHIFT_PLUGIN_OUT_OF_MEMORY;
 out->len=in.len;out->release=release_out;
 if(in.len)memcpy(out->data,in.data,in.len);
 ++invocation_count;
 if(invocation_count>1 && in.len>=8)out->data[0] ^= 1u;
 return PHASESHIFT_PLUGIN_OK;
}
#ifndef STATEFUL_RESET
#define STATEFUL_RESET 0
#endif
static const ps_algorithm_v1 api={PHASESHIFT_PLUGIN_ABI_VERSION,"stateful_fixture",validate,run,
#if STATEFUL_RESET
reset_plugin
#else
NULL
#endif
};
const ps_algorithm_v1* phaseshift_get_algorithm_v1(void){return &api;}
