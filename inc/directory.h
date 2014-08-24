#ifndef _DIRECTORY_H_
#define _DIRECTORY_H_

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#include "genhd.h"
#include "ext2_fs.h"


unsigned int get_dir_entry_end(int inode_num, int newentry_size);

unsigned int find_dir_end_in_direct(unsigned int disk_offset,
                           			unsigned char* block,
                           			int newentry_size);

unsigned int find_dir_end_singly(unsigned int* block, int newentry_size);

unsigned int find_dir_end_doubly(unsigned int* block, int newentry_size);

unsigned int find_dir_end_triply(unsigned int* block, int newentry_size);


#endif


