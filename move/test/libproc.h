/* SPDX-License-Identifier: GPL-3.0-only */
/* Linux stand-in for macOS's <libproc.h>, so host/m2k_draw.c builds unchanged in the Move's container for its --hash
 * (move/proof.sh puts move/test first on the include path). Its instruction counts read 0 here. */
#ifndef M2K_LIBPROC_SHIM
#define M2K_LIBPROC_SHIM
#include <stdint.h>
#include <string.h>
struct rusage_info_v4 { uint64_t ri_instructions; };
typedef void *rusage_info_t;
#define RUSAGE_INFO_V4 4
static inline int proc_pid_rusage(int pid, int flavor, rusage_info_t *buf)
{
    (void)pid; (void)flavor;
    memset(buf, 0, sizeof(struct rusage_info_v4));
    return 0;
}
#endif
