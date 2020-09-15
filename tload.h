#ifndef TLOAD_H
#define TLOAD_H

#include <stdio.h>
#include "pm.h"

#define INVALID_LINE -1
#define END_LINE 0
#define VALID_LINE 1

#define NB_FIELD 7

#ifdef __cplusplus
extern "C"{
#endif

int load_ndn_trace_line(FILE *fp, struct packet_model *pm);
int load_ndn_trace(const char *file, struct packet_model pms[]);

#ifdef __cplusplus
}
#endif

#endif
