/* Deterministic, oracle-correct audit worker with optional OOM/sleep. */
#include "phaseshift/plugin.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#ifndef AUDIT_MODE
#define AUDIT_MODE 1
#endif
static int cmp_i64(const void* a,const void* b){
 int64_t left=0,right=0;memcpy(&left,a,sizeof(left));memcpy(&right,b,sizeof(right));
 return (left>right)-(left<right);
}
static int validate(ps_input_v1 in){return in.len%8u==0u?PHASESHIFT_PLUGIN_OK:PHASESHIFT_PLUGIN_INVALID;}
static void release(unsigned char* ptr,size_t n){(void)n;free(ptr);}
static int run(ps_input_v1 in,ps_output_v1* out){
 if(!out||validate(in)!=PHASESHIFT_PLUGIN_OK)return PHASESHIFT_PLUGIN_INVALID;
#if AUDIT_MODE == 1
 if(in.len/8u>=64u)return PHASESHIFT_PLUGIN_OUT_OF_MEMORY;
#elif AUDIT_MODE == 2
 struct timespec delay={1,200000000L};nanosleep(&delay,0);
#endif
 unsigned char* copy=(unsigned char*)malloc(in.len?in.len:1u);
 if(!copy)return PHASESHIFT_PLUGIN_OUT_OF_MEMORY;
 if(in.len)memcpy(copy,in.data,in.len);
#if AUDIT_MODE == 3
 /* Deliberately misbehaving plugin: fail the independent oracle only
    on already sorted inputs (the ascending morphology fixture). */
 int sorted=1;
 for(size_t i=1;i<in.len/8u;i++)
  if(cmp_i64(in.data+8u*(i-1u),in.data+8u*i)>0){sorted=0;break;}
#endif
 qsort(copy,in.len/8u,8u,cmp_i64);
#if AUDIT_MODE == 3
 if(sorted && in.len>=16u){
  unsigned char tmp[8];memcpy(tmp,copy,8);memcpy(copy,copy+8,8);memcpy(copy+8,tmp,8);
 }
#endif
 out->data=copy;out->len=in.len;out->release=release;
 return PHASESHIFT_PLUGIN_OK;
}
static const ps_algorithm_v1 api={PHASESHIFT_PLUGIN_ABI_VERSION,"audit_boundary_plugin",validate,run,NULL};
const ps_algorithm_v1* phaseshift_get_algorithm_v1(void){return &api;}
