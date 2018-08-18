/* *
 * Copyright (c) 2014, James S. Plank and Kevin Greenan
 * All rights reserved.
 *
 * Jerasure - A C/C++ Library for a Variety of Reed-Solomon and RAID-6 Erasure
 * Coding Techniques
 *
 * Revision 2.0: Galois Field backend now links to GF-Complete
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 *  - Neither the name of the University of Tennessee nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Jerasure's authors:

   Revision 2.x - 2014: James S. Plank and Kevin M. Greenan.
   Revision 1.2 - 2008: James S. Plank, Scott Simmerman and Catherine D. Schuman.
   Revision 1.0 - 2007: James S. Plank.
 */

/*

This program takes as input an inputfile, k, m, a coding
technique, w, and packetsize.  It creates k+m files from
the original file so that k of these files are parts of
the original file and m of the files are encoded based on
the given coding technique. The format of the created files
is the file name with "_k#" or "_m#" and then the extension.
(For example, inputfile test.txt would yield file "test_k1.txt".)
*/

/* #pragma once */

#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include "gf_rand.h"
#include "jerasure.h"
#include "reed_sol.h"
#include "cauchy.h"
#include "liberation.h"
#include "timing.h"
#include "erasure_encode.h"

#ifdef __cplusplus
extern "C" {
#endif

#define N 10

#ifndef NULL
#define NULL ((void *) 0)
#endif

#define str(x) #x
#define xstr(x) str(x)



enum Coding_Technique {Reed_Sol_Van, Reed_Sol_R6_Op, Cauchy_Orig, Cauchy_Good, Liberation, Blaum_Roth, Liber8tion, RDP, EVENODD, No_Coding};

const char *Methods[N] = {"reed_sol_van", "reed_sol_r6_op", "cauchy_orig", "cauchy_good", "liberation", "blaum_roth", "liber8tion", "no_coding"};

/* Global variables for signal handler */
int readins, n;
enum Coding_Technique method;

/* is_prime returns 1 if number if prime, 0 if not prime */
int is_prime(int w) {
	int prime55[] = {2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,
	    73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,
		    181,191,193,197,199,211,223,227,229,233,239,241,251,257};
	int i;
	for (i = 0; i < 55; i++) {
		if (w%prime55[i] == 0) {
			if (w == prime55[i]) return 1;
			else { return 0; }
		}
	}
	assert(0);
}

/* Handles ctrl-\ event */
void ctrl_bs_handler(int dummy) {
	time_t mytime;
	mytime = time(0);
	fprintf(stderr, "\n%s\n", ctime(&mytime));
	fprintf(stderr, "You just typed ctrl-\\ in encoder.c.\n");
	fprintf(stderr, "Total number of read ins = %d\n", readins);
	fprintf(stderr, "Current read in: %d\n", n);
	fprintf(stderr, "Method: %s\n\n", Methods[method]);
	signal(SIGQUIT, ctrl_bs_handler);
}

int jfread(void *ptr, int size, int nmembers, FILE *stream)
{
  if (stream != NULL) return fread(ptr, size, nmembers, stream);

  MOA_Fill_Random_Region(ptr, size);
  return size;
}

/* Function with behaviour like `mkdir -p'  */
/* int mkpath(const char *s, mode_t mode){ */
/*     char *q, *r = NULL, *path = NULL, *up = NULL; */
/*     int rv; */
/*  */
/*     rv = -1; */
/*     if (strcmp(s, ".") == 0 || strcmp(s, "/") == 0) */
/*         return (0); */
/*  */
/*     if ((path = strdup(s)) == NULL) */
/*         exit(1); */
/*  */
/*     if ((q = strdup(s)) == NULL) */
/*         exit(1); */
/*  */
/*     if ((r = dirname(q)) == NULL) */
/*         goto out; */
/*  */
/*     if ((up = strdup(r)) == NULL) */
/*         exit(1); */
/*  */
/*     if ((mkpath(up, mode) == -1) && (errno != EEXIST)) */
/*         goto out; */
/*  */
/*     if ((mkdir(path, mode) == -1) && (errno != EEXIST)) */
/*         rv = -1; */
/*     else */
/*         rv = 0; */
/*  */
/* out: */
/*     if (up != NULL) */
/*         free(up); */
/*     free(q); */
/*     free(path); */
/*     return (rv); */
/* } */

static void mkpath(const char *dir) {
        char tmp[256];
        char *p = NULL;
        size_t len;

        snprintf(tmp, sizeof(tmp),"%s",dir);
        len = strlen(tmp);
        if(tmp[len - 1] == '/')
                tmp[len - 1] = 0;
        for(p = tmp + 1; *p; p++)
                if(*p == '/') {
                        *p = 0;
                        mkdir(tmp, S_IRWXU);
                        *p = '/';
                }
        mkdir(tmp, S_IRWXU);
}

bool EncodeUsingErasure(const char* dirCoding ,char* curdir_path, char* inFile, int k, int m, char* codingType, int w, int packetsize ,int buffersize )
{
	FILE *fp, *fp2;				// file pointers
//	char *block;				// padding file
	char *paddingFile;
	int size, newsize;			// size of file and temp size
	struct stat status;			// finding file size


	enum Coding_Technique tech;		// coding technique (parameter)
//	int k, m, w, packetsize;		// parameters
//	int buffersize;					// paramter
	int i;						// loop control variables
//	int blocksize;					// size of k+m files
	int kPlusM;
	int total;
	int extra;

	/* Jerasure Arguments */
	char **data;
	char **coding;
	int *matrix;
	int *bitmatrix;
	int **schedule;

	/* Creation of file name variables */
	char temp[5];
	char *s1, *s2, *extension;
	char *fname;
	int md;
	char *curdir;

	/* Find buffersize */
	int up, down;


	signal(SIGQUIT, ctrl_bs_handler);

	matrix = NULL;
	bitmatrix = NULL;
	schedule = NULL;

	/* Conversion of parameters and error checking */
	if (k == 0 || k <= 0) {
		fprintf(stderr,  "Invalid value for k\n");
		return false;

	}
	if (m == 0 || m < 0) {
		fprintf(stderr,  "Invalid value for m\n");
		return false;
	}
	if (w == 0 || w <= 0) {
		fprintf(stderr,  "Invalid value for w.\n");
		return false;
	}


	/* Determine proper buffersize by finding the closest valid buffersize to the input value  */
	if (buffersize != 0) {
		if (packetsize != 0 && buffersize%(sizeof(long)*w*k*packetsize) != 0) {
			up = buffersize;
			down = buffersize;
			while (up%(sizeof(long)*w*k*packetsize) != 0 && (down%(sizeof(long)*w*k*packetsize) != 0)) {
				up++;
				if (down == 0) {
					down--;
				}
			}
			if (up%(sizeof(long)*w*k*packetsize) == 0) {
				buffersize = up;
			}
			else {
				if (down != 0) {
					buffersize = down;
				}
			}
		}
		else if (packetsize == 0 && buffersize%(sizeof(long)*w*k) != 0) {
			up = buffersize;
			down = buffersize;
			while (up%(sizeof(long)*w*k) != 0 && down%(sizeof(long)*w*k) != 0) {
				up++;
				down--;
			}
			if (up%(sizeof(long)*w*k) == 0) {
				buffersize = up;
			}
			else {
				buffersize = down;
			}
		}
	}

	/* Setting of coding technique and error checking */
	if (strcmp(codingType, "no_coding") == 0) {
		tech = No_Coding;
	}
	else if (strcmp(codingType, "reed_sol_van") == 0) {
		tech = Reed_Sol_Van;
		if (w != 8 && w != 16 && w != 32) {
			fprintf(stderr,  "w must be one of {8, 16, 32}\n");
			return false;
		}
	}
	else if (strcmp(codingType, "reed_sol_r6_op") == 0) {
		if (m != 2) {
			fprintf(stderr,  "m must be equal to 2\n");
			return false;
		}
		if (w != 8 && w != 16 && w != 32) {
			fprintf(stderr,  "w must be one of {8, 16, 32}\n");
			return false;
		}
		tech = Reed_Sol_R6_Op;
	}
	else if (strcmp(codingType, "cauchy_orig") == 0) {
		tech = Cauchy_Orig;
		if (packetsize == 0) {
			fprintf(stderr, "Must include packetsize.\n");
			return false;
		}
	}
	else if (strcmp(codingType, "cauchy_good") == 0) {
		tech = Cauchy_Good;
		if (packetsize == 0) {
			fprintf(stderr, "Must include packetsize.\n");
			return false;
		}
	}
	else if (strcmp(codingType, "liberation") == 0) {
		if (k > w) {
			fprintf(stderr,  "k must be less than or equal to w\n");
			return false;
		}
		if (w <= 2 || !(w%2) || !is_prime(w)) {
			fprintf(stderr,  "w must be greater than two and w must be prime\n");
			return false;
		}
		if (packetsize == 0) {
			fprintf(stderr, "Must include packetsize.\n");
			return false;
		}
		if ((packetsize%(sizeof(long))) != 0) {
			fprintf(stderr,  "packetsize must be a multiple of sizeof(long)\n");
			return false;
		}
		tech = Liberation;
	}
	else if (strcmp(codingType, "blaum_roth") == 0) {
		if (k > w) {
			fprintf(stderr,  "k must be less than or equal to w\n");
			return false;
		}
		if (w <= 2 || !((w+1)%2) || !is_prime(w+1)) {
			fprintf(stderr,  "w must be greater than two and w+1 must be prime\n");
			return false;
		}
		if (packetsize == 0) {
			fprintf(stderr, "Must include packetsize.\n");
			return false;
		}
		if ((packetsize%(sizeof(long))) != 0) {
			fprintf(stderr,  "packetsize must be a multiple of sizeof(long)\n");
			return false;
		}
		tech = Blaum_Roth;
	}
	else if (strcmp(codingType, "liber8tion") == 0) {
		if (packetsize == 0) {
			fprintf(stderr, "Must include packetsize\n");
			return false;
		}
		if (w != 8) {
			fprintf(stderr, "w must equal 8\n");
			return false;
		}
		if (m != 2) {
			fprintf(stderr, "m must equal 2\n");
			return false;
		}
		if (k > w) {
			fprintf(stderr, "k must be less than or equal to w\n");
			return false;
		}
		tech = Liber8tion;
	}
	else {
		fprintf(stderr,  "Not a valid coding technique. Choose one of the following: reed_sol_van, reed_sol_r6_op, cauchy_orig, cauchy_good, liberation, blaum_roth, liber8tion, no_coding\n");
		return false;
	}


	/* Set global variable method for signal handler */
	method = tech;

	/* Get current working directory for construction of file names */

	curdir = (char*)malloc(sizeof(char)*1000);
	strcpy(curdir,curdir_path);
	/* assert(curdir == getcwd(curdir, 1000)); */

	/* Open file and error check */
	fp = fopen(inFile, "rb");
	if (fp == NULL) {
		fprintf(stderr,  "Unable to open file.\n");
                printf("Error %d \n", errno);
		return false;
	}

	/* Create Coding directory */
	/* i = mkdir("Coding", S_IRWXU); */
	/* if (i == -1 && errno != EEXIST) { */
	/* 	fprintf(stderr, "Unable to create Coding directory.\n"); */
	/* 	return false; */
	/* } */

        mkpath(dirCoding);
	/* if (i == -1 && errno != EEXIST) { */
	/* 	fprintf(stderr, "Unable to create Coding directory.\n"); */
	/* 	return false; */
	/* } */


	/* Determine original size of file */
	stat(inFile, &status);
	size = status.st_size;

	newsize = size;

	/* Find new size by determining next closest multiple */
	if (packetsize != 0) {
		if (size%(k*w*packetsize*sizeof(long)) != 0) {
			while (newsize%(k*w*packetsize*sizeof(long)) != 0)
				newsize++;
		}
	}
	else {
		if (size%(k*w*sizeof(long)) != 0) {
			while (newsize%(k*w*sizeof(long)) != 0)
				newsize++;
		}
	}

	if (buffersize != 0) {
		while (newsize%buffersize != 0) {
			newsize++;
		}
	}


	/* Determine size of k+m files */
	kPlusM = newsize/k;

	/* Allow for buffersize and determine number of read-ins */
	if (size > buffersize && buffersize != 0) {
		if (newsize%buffersize != 0) {
			readins = newsize/buffersize;
		}
		else {
			readins = newsize/buffersize;
		}
		paddingFile = (char *)malloc(sizeof(char)*buffersize);
		kPlusM = buffersize/k;
	}
	else {
		readins = 1;
		buffersize = size;
		paddingFile = (char *)malloc(sizeof(char)*newsize);
	}

	/* Break inputfile name into the filename and extension */
	s1 = (char*)malloc(sizeof(char)*(strlen(inFile)+20));
	s2 = strrchr(inFile, '/');
	if (s2 != NULL) {
		s2++;
		strcpy(s1, s2);
	}
	else {
		strcpy(s1, inFile);
	}
	s2 = strchr(s1, '.');
	if (s2 != NULL) {
          extension = strdup(s2);
          *s2 = '\0';
	} else {
          extension = strdup("");
        }

	/* Allocate for full file name */
	fname = (char*)malloc(sizeof(char)*(strlen(inFile)+strlen(curdir)+20));
	sprintf(temp, "%d", k);
	md = strlen(temp);

	/* Allocate data and coding */
	data = (char **)malloc(sizeof(char*)*k);
	coding = (char **)malloc(sizeof(char*)*m);
	for (i = 0; i < m; i++) {
		coding[i] = (char *)malloc(sizeof(char)*kPlusM);
                if (coding[i] == NULL) { perror("malloc"); return false; }
	}



	/* Create coding matrix or bitmatrix and schedule */
	switch(tech) {
		case No_Coding:
			break;
		case Reed_Sol_Van:
			matrix = reed_sol_vandermonde_coding_matrix(k, m, w);
			break;
		case Reed_Sol_R6_Op:
			break;
		case Cauchy_Orig:
			matrix = cauchy_original_coding_matrix(k, m, w);
			bitmatrix = jerasure_matrix_to_bitmatrix(k, m, w, matrix);
			schedule = jerasure_smart_bitmatrix_to_schedule(k, m, w, bitmatrix);
			break;
		case Cauchy_Good:
			matrix = cauchy_good_general_coding_matrix(k, m, w);
			bitmatrix = jerasure_matrix_to_bitmatrix(k, m, w, matrix);
			schedule = jerasure_smart_bitmatrix_to_schedule(k, m, w, bitmatrix);
			break;
		case Liberation:
			bitmatrix = liberation_coding_bitmatrix(k, w);
			schedule = jerasure_smart_bitmatrix_to_schedule(k, m, w, bitmatrix);
			break;
		case Blaum_Roth:
			bitmatrix = blaum_roth_coding_bitmatrix(k, w);
			schedule = jerasure_smart_bitmatrix_to_schedule(k, m, w, bitmatrix);
			break;
		case Liber8tion:
			bitmatrix = liber8tion_coding_bitmatrix(k);
			schedule = jerasure_smart_bitmatrix_to_schedule(k, m, w, bitmatrix);
			break;
		case RDP:
		case EVENODD:
			assert(0);
	}



	/* Read in data until finished */
	n = 1;
	total = 0;

	while (n <= readins) {
		/* Check if padding is needed, if so, add appropriate
		   number of zeros */
		if (total < size && total+buffersize <= size) {
			total += jfread(paddingFile, sizeof(char), buffersize, fp);
		}
		else if (total < size && total+buffersize > size) {
			extra = jfread(paddingFile, sizeof(char), buffersize, fp);
			for (i = extra; i < buffersize; i++) {
				paddingFile[i] = '0';
			}
		}
		else if (total == size) {
			for (i = 0; i < buffersize; i++) {
				paddingFile[i] = '0';
			}
		}

		/* Set pointers to point to file data */
		for (i = 0; i < k; i++) {
			data[i] = paddingFile+(i*kPlusM);
		}

		/* Encode according to coding method */
		switch(tech) {
			case No_Coding:
				break;
			case Reed_Sol_Van:
				jerasure_matrix_encode(k, m, w, matrix, data, coding, kPlusM);
				break;
			case Reed_Sol_R6_Op:
				reed_sol_r6_encode(k, w, data, coding, kPlusM);
				break;
			case Cauchy_Orig:
				jerasure_schedule_encode(k, m, w, schedule, data, coding, kPlusM, packetsize);
				break;
			case Cauchy_Good:
				jerasure_schedule_encode(k, m, w, schedule, data, coding, kPlusM, packetsize);
				break;
			case Liberation:
				jerasure_schedule_encode(k, m, w, schedule, data, coding, kPlusM, packetsize);
				break;
			case Blaum_Roth:
				jerasure_schedule_encode(k, m, w, schedule, data, coding, kPlusM, packetsize);
				break;
			case Liber8tion:
				jerasure_schedule_encode(k, m, w, schedule, data, coding, kPlusM, packetsize);
				break;
			case RDP:
			case EVENODD:
				assert(0);
		}

		/* Write data and encoded data to k+m files */
		for	(i = 1; i <= k; i++) {
			if (fp == NULL) {
				bzero(data[i-1], kPlusM);
 			} else {
				sprintf(fname, "%s/Coding/%s_k%0*d%s", curdir, s1, md, i, extension);
				if (n == 1) {
					fp2 = fopen(fname, "wb");
				}
				else {
					fp2 = fopen(fname, "ab");
				}
				fwrite(data[i-1], sizeof(char), kPlusM, fp2);
				fclose(fp2);
			}

		}
		for	(i = 1; i <= m; i++) {
			if (fp == NULL) {
				bzero(data[i-1], kPlusM);
 			} else {
				sprintf(fname, "%s/Coding/%s_m%0*d%s", curdir, s1, md, i, extension);
				if (n == 1) {
					fp2 = fopen(fname, "wb");
				}
				else {
					fp2 = fopen(fname, "ab");
				}
				fwrite(coding[i-1], sizeof(char), kPlusM, fp2);
				fclose(fp2);
			}
		}
		n++;
	}

	/* Create metadata file */
        if (fp != NULL) {
		sprintf(fname, "%s/Coding/%s_meta.txt", curdir, s1);
		fp2 = fopen(fname, "wb");
		fprintf(fp2, "%s\n", inFile);
		fprintf(fp2, "%d\n", size);
		fprintf(fp2, "%d %d %d %d %d\n", k, m, w, packetsize, buffersize);
		fprintf(fp2, "%s\n",xstr(tech));
		fprintf(fp2, "%d\n", tech);
		fprintf(fp2, "%d\n", readins);
		fclose(fp2);
	}


	/* Free allocated memory */
	free(s1);
	free(fname);
	free(paddingFile);
	free(curdir);

	return true;

}

//int main(int argc, char **arg)
//{
//	return 0;
//}



#ifdef __cplusplus
}
#endif
