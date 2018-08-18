#ifndef ERASURE_ENCODE_H
#define ERASURE_ENCODE_H

#include <stdint.h>
#include "gf_complete.h"

#ifdef __cplusplus
extern "C" {
#endif



/* Function prototypes */
int is_prime(int w);
void ctrl_bs_handler(int dummy);
static void mkpath(const char *path);
bool EncodeUsingErasure(const char* dirCoding ,char* curdir_path, char* inFile, int k, int m, char* codingType, int w, int packetsize ,int buffersize );



#ifdef __cplusplus
}

#endif // _cpluscplus

#endif // ERASURE_ENCODE_H
