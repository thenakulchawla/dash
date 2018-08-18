#ifndef ERASURE_DECODE_H
#define ERASURE_DECODE_H

#ifdef _cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>


/* Function prototype */
void ctrl_bs_handler(int dummy);
bool DecodeUsingErasure ();

#endif // _cplusplus

#endif // ERASURE_DECODE_H

