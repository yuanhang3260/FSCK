/** @file directory.c
 *  @brief This module contains functions used to operate on the directory
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
#include "directory.h"

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


/** @brief find the end address of a directory entry 
 * 
 *  @param inode_num of the directory
 *  @return address of the end of directory entry
 */
unsigned int get_dir_entry_end(int inode_num, int newentry_size)
{
	struct ext2_inode inode;
	int inode_addr = 0;
	
	/* get inode addr (in byte) from inode number */
	inode_addr = get_inode_addr(inode_num);
	
	/* read inode information from inode table entry */
	read_bytes(inode_addr, &inode, sizeof(struct ext2_inode));

	/* if it's not a directory, return */
	if(!EXT2_S_ISDIR(inode.i_mode))
		return -1;

	unsigned char buf[sb.block_size]; /* 1024 bytes */
	/* search in direct blocks */
	int i = 0;
	int ret = -1;
	int found = 0;
	for(i = 0; i < EXT2_NDIR_BLOCKS; i++)
	{
		if (inode.i_block[i] <= 0)
			continue;
		
		int disk_offset = pt_info.base + inode.i_block[i] * sb.block_size;
		read_bytes(disk_offset, buf, sb.block_size);
		
		/* traverse direct block[i] */
		ret = find_dir_end_in_direct(disk_offset, buf, newentry_size);
		if (ret > 0)
		{
			found = 1;
			break;
		}
	}

	if (!found && inode.i_block[EXT2_IND_BLOCK] != 0)
	{	/* traverse singly indirect block */
		read_bytes(pt_info.base + inode.i_block[EXT2_IND_BLOCK]*sb.block_size, 
				    buf, sb.block_size);
		ret = find_dir_end_singly((unsigned int*)buf, newentry_size);
	}
	if (!found && inode.i_block[EXT2_DIND_BLOCK] != 0)
	{	/* traverse doubly indirect block */
		read_bytes(pt_info.base + inode.i_block[EXT2_DIND_BLOCK]*sb.block_size, 
				    buf, sb.block_size);
		ret = find_dir_end_doubly((unsigned int*)buf, newentry_size);
	}
	if (!found && inode.i_block[EXT2_TIND_BLOCK] != 0)
	{	/* traverse triply indirect block */
		read_bytes(pt_info.base + inode.i_block[EXT2_TIND_BLOCK]*sb.block_size, 
			 	    buf, sb.block_size);
		ret = find_dir_end_triply((unsigned int*)buf, newentry_size);
	}
	return ret;
}


/** @brief search the end of directory entry in a direct block
 *  
 *  @param block block buffer
 *  @param new directory entry size to be appended
 *  @return new entry address or -1 if dir end is 
 *   not found in this block.
 */
unsigned int find_dir_end_in_direct(unsigned int disk_offset,
                           			unsigned char* buf,
                           			int newentry_size)
{
	struct ext2_dir_entry_2 dir_entry;
	int dir_entry_base = 0;

	while(1)
	{
		dir_entry.rec_len = *(__u16*)(buf + dir_entry_base + 4);
		dir_entry.file_type = *(__u8*)(buf + dir_entry_base + 7);
		dir_entry.name_len = *(__u8*)(buf + dir_entry_base + 6);
		memcpy(dir_entry.name, buf + dir_entry_base + 8, dir_entry.name_len);
		dir_entry.name[dir_entry.name_len] = '\0';
		

		if (dir_entry_base + dir_entry.rec_len >= sb.block_size)
			break;

		dir_entry_base += dir_entry.rec_len;
	}

	/* check found */
	int name_size = (dir_entry.name_len - 1) / 4 * 4 + 4;
	
	int ret = -1;
	if (dir_entry_base + 8 + name_size + newentry_size < sb.block_size)
	{
		ret = (disk_offset + dir_entry_base + 8 + name_size);
		dir_entry.rec_len = 8 + name_size;
		dir_entry.inode = *(__u32*)(buf + dir_entry_base + 0);
		memcpy(dir_entry.name, buf + dir_entry_base + 8, dir_entry.name_len);
		dir_entry.name[dir_entry.name_len] = '\0';
		
		write_bytes(disk_offset + dir_entry_base, &dir_entry, 
		            dir_entry.rec_len);
	}
	return ret;
}


/** @brief traverse in singly indirect block of a directory 
 *  
 *  @param buf block content
 *  @param current_dir inode number of current dir
 *  @param parent_dir inode nmber of parent dir
 */
unsigned int find_dir_end_singly(unsigned int* singly_buf, int newentry_size)
{
	unsigned char direct_buf[sb.block_size];
	int i = 0;
	int ret = -1;
	for(i = 0; i < (sb.block_size / 4); i++)
	{
		if (singly_buf[i] == 0)
			break;

		int disk_offset = pt_info.base + singly_buf[i] * sb.block_size;
		read_bytes(disk_offset, direct_buf, sb.block_size);
		unsigned int ret = find_dir_end_in_direct(disk_offset,
		                            direct_buf, newentry_size);
		if (ret > 0)
			break;
	}
	return ret;
}


/** @brief traverse in doubly indirect block of a directory 
 *  
 *  @param buf block content
 *  @param current_dir inode number of current dir
 *  @param parent_dir inode nmber of parent dir
 */
unsigned int find_dir_end_doubly(unsigned int* doubly_buf, int newentry_size)
{
	unsigned int singly_buf[sb.block_size];
	int i = 0;
	int ret = -1;
	for(i = 0; i < (sb.block_size / 4); i++)
	{
		if (doubly_buf[i] == 0)
			break;

		read_bytes(pt_info.base + doubly_buf[i] * sb.block_size,
		            singly_buf, sb.block_size);
		unsigned int ret = find_dir_end_singly(singly_buf, newentry_size);
		if (ret > 0)
			break;
	}
	return ret;
}


/** @brief traverse in triply indirect block of a directory 
 *  
 *  @param buf block content
 *  @param current_dir inode number of current dir
 *  @param parent_dir inode nmber of parent dir
 */
unsigned int find_dir_end_triply(unsigned int* triply_buf, int newentry_size)
{
	unsigned int doubly_buf[sb.block_size];
	int i = 0;
	int ret = -1;
	for(i = 0; i < (sb.block_size / 4); i++)
	{
		if (triply_buf[i] == 0)
			break;

		read_bytes(pt_info.base + triply_buf[i] * sb.block_size,
		            doubly_buf, sb.block_size);
		unsigned int ret = find_dir_end_doubly(doubly_buf, newentry_size);
		if (ret > 0)
			break;
	}
	return ret;
}


