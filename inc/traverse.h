#ifndef _TRAVERSE_DIR_H_
#define _TRAVERSE_DIR_H_

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#include "genhd.h"
#include "ext2_fs.h"


void traverse_dir(unsigned int inode_num, unsigned int parent);

void traverse_direct_block(int disk_offset,
                           int block_num,
						   unsigned char* buf, 
                           unsigned int current_dir, 
                           unsigned int parent_dir);

void traverse_singly(unsigned int* singly_buf, 
                     unsigned int current_dir, 
                     unsigned int parent_dir);

void traverse_doubly(unsigned int* doubly_buf, 
                     unsigned int current_dir, 
                     unsigned int parent_dir);

void traverse_triply(unsigned int* triply_buf, 
                     unsigned int current_dir, 
                     unsigned int parent_dir);


#endif


