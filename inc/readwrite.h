#ifndef _READWRITE_H_
#define _READWRITE_H_

#include <inttypes.h>

#if defined(__FreeBSD__)
#define lseek64 lseek
#endif

#define SECTOR_SIZE 512

extern int64_t lseek64(int, int64_t, int);

void read_bytes(long base, void* into, int buf_len);
void write_bytes(long base, void* from, int buf_len);

void read_sector(long block, void *into, int buf_len);


#endif


