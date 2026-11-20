/* Deterministic synthetic scaling fixture: sorts correctly but intentionally
 * incurs a sharply different finite-size delay above the frozen threshold.
 * This exercises the real scaling worker and SQLite holdout pipeline; measured
 * delays are an engineered test signal, never a real Big-O discovery claim. */
#include "phaseshift/plugin.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int compare_int64(const void *a, const void *b) {
  int64_t l = 0, r = 0;
  memcpy(&l, a, sizeof l);
  memcpy(&r, b, sizeof r);
  return (l > r) - (l < r);
}
static int valid(ps_input_v1 in) {
  return in.len % sizeof(int64_t) == 0 ? PHASESHIFT_PLUGIN_OK : PHASESHIFT_PLUGIN_INVALID;
}
static void release(unsigned char *p, size_t size) { (void)size; free(p); }
static int run(ps_input_v1 in, ps_output_v1 *out) {
  if (!out || valid(in) != PHASESHIFT_PLUGIN_OK) return PHASESHIFT_PLUGIN_INVALID;
  const size_t n = in.len / sizeof(int64_t);
  /* 2 ms below threshold, a cubic-sized artificial delay above it. */
  double us = 2000.0;
  if (n > 96u) {
    const double x = (double)n / 96.0;
    us *= x*x*x;
  }
  if (us > 250000.0) us = 250000.0;
  struct timespec wait_time = {(time_t)(us / 1000000.0),
                               (long)((us - (double)(time_t)(us / 1000000.0)*1000000.0)*1000.0)};
  while (nanosleep(&wait_time, &wait_time) != 0) {
    if (errno != EINTR) return PHASESHIFT_PLUGIN_ERROR;
  }
  unsigned char *buf = (unsigned char *)malloc(in.len ? in.len : 1u);
  if (!buf) return PHASESHIFT_PLUGIN_OUT_OF_MEMORY;
  if (in.len) memcpy(buf, in.data, in.len);
  qsort(buf, n, sizeof(int64_t), compare_int64);
  out->data = buf;
  out->len = in.len;
  out->release = release;
  return PHASESHIFT_PLUGIN_OK;
}
static const ps_algorithm_v1 plugin = {
  PHASESHIFT_PLUGIN_ABI_VERSION,"synthetic_scaling_knee",valid,run,NULL};
const ps_algorithm_v1 *phaseshift_get_algorithm_v1(void) { return &plugin; }
