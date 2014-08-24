#ifndef _PARSE_PARTITION_H_
#define _PARSE_PARTITION_H_

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#include "genhd.h"
#include "ext2_fs.h"

#define EXT2_S_ISOCK(m) (((m)&(0xf000)) == (EXT2_S_IFSOCK))
#define EXT2_S_ISLNK(m) (((m)&(0xf000)) == (EXT2_S_IFLNK))
#define EXT2_S_ISREG(m) (((m)&(0xf000)) == (EXT2_S_IFREG))
#define EXT2_S_ISBLK(m) (((m)&(0xf000)) == (EXT2_S_IFBLK))
#define EXT2_S_ISDIR(m) (((m)&(0xf000)) == (EXT2_S_IFDIR))
#define EXT2_S_ISCHR(m) (((m)&(0xf000)) == (EXT2_S_IFCHR))
#define EXT2_S_ISFIFO(m) (((m)&(0xf000)) == (EXT2_S_IFIFO))

int get_inode_addr(int inode_num);
int check_bitmap(int bitmap_base, int index);
int imode_to_filetype(__u16 i_mode);
int ispowerof(int s, int a);


int search_filename_in_dir_block(unsigned char* block, char* filename);

int search_filename_in_singly(unsigned int* block, char* filename);

int search_filename_in_doubly(unsigned int* block, char* filename);

int search_filename_in_triply(unsigned int* block, char* filename);

#endif


