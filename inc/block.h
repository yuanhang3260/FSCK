#ifndef _BLOCK_H_
#define _BLOCK_H_

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#include "genhd.h"
#include "ext2_fs.h"


void mark_block(int inode_num);

unsigned int mark_block_singly(unsigned int* singly_buf);

unsigned int mark_block_doubly(unsigned int* doubly_buf);

unsigned int mark_block_triply(unsigned int* triply_buf);


#endif


