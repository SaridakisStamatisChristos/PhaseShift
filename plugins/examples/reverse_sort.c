#include "phaseshift/plugin.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static int validate(ps_input_v1 in){return (in.len%8u==0u && in.len<=1048576u)?PHASESHIFT_PLUGIN_OK:PHASESHIFT_PLUGIN_INVALID;}
static void release_out(unsigned char* p,size_t n){(void)n;free(p);}
static int run(ps_input_v1 in,ps_output_v1* out){
 if(!out||validate(in)!=PHASESHIFT_PLUGIN_OK)return PHASESHIFT_PLUGIN_INVALID;
 out->data=malloc(in.len ? in.len:1u);if(!out->data)return PHASESHIFT_PLUGIN_ERROR;
 out->len=in.len;out->release=release_out;
 /* canonical array encoding: little-endian signed 64-bit words */
 size_t n=in.len/8u;
 for(size_t i=0;i<n;i++)memcpy(out->data+8u*i,in.data+8u*(n-1u-i),8u);
 return PHASESHIFT_PLUGIN_OK;
}
static const ps_algorithm_v1 api={PHASESHIFT_PLUGIN_ABI_VERSION,"reverse_sort_example",validate,run,NULL};
const ps_algorithm_v1* phaseshift_get_algorithm_v1(void){return &api;}
