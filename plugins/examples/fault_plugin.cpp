#include "phaseshift/plugin.h"
#include <csignal>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <cstring>
#ifndef FAULT_MODE
#define FAULT_MODE 0
#endif
static int validate(ps_input_v1) {
#if FAULT_MODE == 5
 return PHASESHIFT_PLUGIN_INVALID;
#else
 return PHASESHIFT_PLUGIN_OK;
#endif
}
static void release(unsigned char* data,size_t){std::free(data);}
static unsigned int calls=0;
static int run(ps_input_v1 in,ps_output_v1* out) {
#if FAULT_MODE == 1
 std::raise(SIGSEGV);
#elif FAULT_MODE == 2
 std::this_thread::sleep_for(std::chrono::seconds(5));
#elif FAULT_MODE == 3
 out->data=static_cast<unsigned char*>(std::malloc(1));
 if(!out->data)return PHASESHIFT_PLUGIN_OUT_OF_MEMORY;
 out->len=100000000;out->release=&release;
 return PHASESHIFT_PLUGIN_OK;
#elif FAULT_MODE == 4
 throw std::runtime_error("intentional plugin exception");
#elif FAULT_MODE == 6
 throw std::bad_alloc();
#elif FAULT_MODE == 9
 std::this_thread::sleep_for(std::chrono::milliseconds(450));
 out->data=static_cast<unsigned char*>(std::malloc(in.len?in.len:1));
 if(!out->data)return PHASESHIFT_PLUGIN_OUT_OF_MEMORY;
 out->len=in.len;out->release=&release;
 if(in.len)std::memcpy(out->data,in.data,in.len);
 return PHASESHIFT_PLUGIN_OK;
#else
 out->data=static_cast<unsigned char*>(std::malloc(in.len?in.len:1));
 if(!out->data)return PHASESHIFT_PLUGIN_OUT_OF_MEMORY;
 out->len=in.len;out->release=&release;
 if(in.len)std::memcpy(out->data,in.data,in.len);
#if FAULT_MODE == 8
 ++calls;
 if(calls==3&&in.len>=8)out->data[0]^=1u; /* bad intermediate sample, valid final */
#endif
 return PHASESHIFT_PLUGIN_OK;
#endif
 return PHASESHIFT_PLUGIN_ERROR;
}
static const ps_algorithm_v1 api={
#if FAULT_MODE == 7
 99u,
#else
 PHASESHIFT_PLUGIN_ABI_VERSION,
#endif
 "fault",&validate,&run,nullptr};
extern "C" const ps_algorithm_v1* phaseshift_get_algorithm_v1(void){return &api;}
