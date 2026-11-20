#ifndef PHASESHIFT_PLUGIN_H
#define PHASESHIFT_PLUGIN_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PHASESHIFT_PLUGIN_ABI_VERSION 1u
#define PHASESHIFT_PLUGIN_OK 0
#define PHASESHIFT_PLUGIN_INVALID 1
#define PHASESHIFT_PLUGIN_ERROR 2
#define PHASESHIFT_PLUGIN_OUT_OF_MEMORY 3
/* Output must be freed by 'release' supplied by plugin; never freed by host allocator. */
typedef struct { const unsigned char *data; size_t len; } ps_input_v1;
typedef struct { unsigned char *data; size_t len; void (*release)(unsigned char *, size_t); } ps_output_v1;
typedef struct {
    uint32_t abi_version;
    const char *algorithm_id;
    int (*validate_input)(ps_input_v1);
    int (*run)(ps_input_v1, ps_output_v1 *);
    void (*reset)(void);
} ps_algorithm_v1;
typedef const ps_algorithm_v1 *(*ps_get_algorithm_v1)(void);
/* A plugin exports this symbol; it must catch all exceptions internally. */
const ps_algorithm_v1 *phaseshift_get_algorithm_v1(void);
#ifdef __cplusplus
}
#endif
#endif
