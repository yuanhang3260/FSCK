#ifndef _FSCK_H_
#define _FSCK_H_

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#include "genhd.h"
#include "ext2_fs.h"
#include "utility.h"


#define PARTITION_ENTRY_SIZE  16
#define MBR_SIZE   512
#define EBR_SIZE   512
#define BOOTSTRAP_SIZE 0x1be
#define FIX_SELF 0
#define FIX_PARENT 1


/** @brief partition information */
typedef struct partition_info
{
	int partition_num;
	int type;
	int start_sec;
	int base;
	int length;
} partition_t;

/** @brief super block information */
typedef struct superblock_info 
{
	int block_size;
	int inode_size;

	int	num_blocks;
	int blocks_per_group;

	int num_inodes;
	int inodes_per_group;

	int num_groups;	
} superblock_t;


// ********** read information *********** //
int fsck_partition_init(int partition_num);

int read_partition_info(int partition_num, partition_t *pt);

int read_superblock_info(int partition_num);

int read_bg_desc_table();


// *************** fixing *************** //
int fix_fs(int partition_num);

void fix_unreferenced_inode();

void fix_link_counts();

void fix_block_map();


// *************** Utilities *************** //
int get_inode_by_filepath(const char* filepath);

int get_parent_id(struct ext2_inode* inode);

int put_into_lostfound(int inode_num);

void set_inode_num(unsigned int inode_num, unsigned int parent, 
                   int addr, int fix_flag);






#endif


