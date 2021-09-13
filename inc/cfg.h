#ifndef CFG_H
#define CFG_H

#include "types.h"

#ifdef __cplusplus
extern "C" { //}
#endif

struct Cfg *read_cfg(const char *path);

void print_cfg(struct Cfg *cfg);

#if __cplusplus
} // extern "C"
#endif

#endif // CFG_H
