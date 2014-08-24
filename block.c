/** @file block.c
 *  @brief This module contains functions used to mark blocks
 *   while traversing inodes
 *
 *  @author: Hang Yuan (hangyuan)
 *  @bug: No bugs found yet
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <inttypes.h>

#include "genhd.h"
#include "ext2_fs.h"
#include "utility.h"
#include "readwrite.h"
#include "fsck.h"
#include "block.h"

/*** global variables ***/
/** partition information */
extern partition_t pt_info;
/** superblock information */
extern superblock_t sb;
/** block group discriptor table */
extern struct ext2_group_desc* bg_desc_table;
/** local inode map */
extern int* my_inode_map;
/** local local map */
extern int* my_block_map;
/** disk bitmap */
extern unsigned char* bitmap;


/** @brief mark all allocated blocks of an inode
 *  
 *  @param inode_num inode number
 *  @return void
 */
void mark_block(int inode_num)
{
	struct ext2_inode inode;
	int inode_addr = 0;
	
	/* get inode addr (in byte) from inode number */
	inode_addr = get_inode_addr(inode_num);
	
	/* read inode information from inode table entry */
	read_bytes(inode_addr, &inode, sizeof(struct ext2_inode));

	/* skip symbolic link file shorter than 60 bytes */
	if (EXT2_S_ISLNK(inode.i_mode) && inode.i_size < 60)
		return;

	unsigned char buf[sb.block_size]; /* 1024 bytes */
	
	/* search in direct blocks */
	
	int i = 0;
	for(i = 0; i < EXT2_NDIR_BLOCKS; i++)
	{
		if (inode.i_block[i] <= 0)
			continue;
		
		my_block_map[inode.i_block[i]] = 1;
	}
	
	/* traverse singly indirect block */
	if (inode.i_block[EXT2_IND_BLOCK] > 0)
	{
		
		my_block_map[inode.i_block[EXT2_IND_BLOCK]] = 1;
		read_bytes(pt_info.base + inode.i_block[EXT2_IND_BLOCK] * sb.block_size, 
			    	buf, sb.block_size);
		mark_block_singly((unsigned int*)buf);
	}
	/* traverse doubly indirect block */
	if (inode.i_block[EXT2_DIND_BLOCK] > 0)
	{
		my_block_map[inode.i_block[EXT2_DIND_BLOCK]] = 1;
		read_bytes(pt_info.base + inode.i_block[EXT2_DIND_BLOCK] * sb.block_size, 
				    buf, sb.block_size);
		mark_block_doubly((unsigned int*)buf);
	}
	/* traverse triply indirect block */
	if (inode.i_block[EXT2_TIND_BLOCK] > 0)
	{
		my_block_map[inode.i_block[EXT2_TIND_BLOCK]] = 1;
		read_bytes(pt_info.base + inode.i_block[EXT2_TIND_BLOCK] * sb.block_size, 
					buf, sb.block_size);
		mark_block_triply((unsigned int*)buf);
	}
	return;
}


/** @brief traverse in singly indirect block of a directory and 
 *   marks allocated blocks
 *  
 *  @param buf block content
 *  @return unsigned int
 */
unsigned int mark_block_singly(unsigned int* singly_buf)
{
	int i = 0;
	int ret = -1;
	
	for(i = 0; i < (sb.block_size / 4); i++)
	{
		if (singly_buf[i] == 0)
			break;
		
		my_block_map[singly_buf[i]] = 1;
	}
	
	return ret;
}


/** @brief traverse in doubly indirect block of a directory and 
 *   marks allocated blocks
 *  
 *  @param buf block content
 *  @param parent_dir inode nmber of parent dir
 */
unsigned int mark_block_doubly(unsigned int* doubly_buf)
{
	int i = 0;
	int ret = -1;
	unsigned int singly_buf[sb.block_size];
	for(i = 0; i < (sb.block_size / 4); i++)
	{
		if (doubly_buf[i] == 0)
			break;

		my_block_map[doubly_buf[i]] = 1;
		read_bytes(pt_info.base + doubly_buf[i] * sb.block_size,
		            singly_buf, sb.block_size);
		mark_block_singly(singly_buf);
	}
	return ret;
}


/** @brief traverse in triply indirect block of a directory and 
 *   marks allocated blocks
 *  
 *  @param buf block content
 *  @param parent_dir inode nmber of parent dir
 */
unsigned int mark_block_triply(unsigned int* triply_buf)
{
	int i = 0;
	int ret = -1;
	unsigned int doubly_buf[sb.block_size];
	for(i = 0; i < (sb.block_size / 4); i++)
	{
		if (triply_buf[i] == 0)
			break;

		my_block_map[triply_buf[i]] = 1;
		read_bytes(pt_info.base + triply_buf[i] * sb.block_size,
		            doubly_buf, sb.block_size);
		mark_block_doubly(doubly_buf);
	}
	return ret;
}


