/** @file utility.c
 *  @brief This module contains utility functions
 *
 *  @author: Hang Yuan (hangyuan)
 *  @bug: No bugs found yet
 */
 
#include <stdlib.h>
#include <stdio.h>
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


/** partition information */
extern partition_t pt_info;
/** super block information */
extern superblock_t sb;
/** block group discriptor table */
extern struct ext2_group_desc* bg_desc_table;


/** @brief get inode entry address in the inode table given 
 *   inode number
 *  
 *  @param inode_num inode number
 *  @param base address of indoe entry (in byte)
 */
int get_inode_addr(int inode_num)
{
	/* remember: inode number starts from 1 */
	int group_index = (inode_num - 1) / sb.inodes_per_group;
	int inode_index = (inode_num - 1) % sb.inodes_per_group;

	int pt_base = pt_info.start_sec * SECTOR_SIZE;
	int table_offset = bg_desc_table[group_index].bg_inode_table 
	                 * sb.block_size;
	int inode_offset = inode_index * sb.inode_size;

	return pt_base + table_offset + inode_offset;
}


/** @brief check if a specific index in bitmap is allocated
 *  
 *  @int bitmap_base base address (in byte) of the bitmap
 *  @index bit index in the bitmap
 *  @return >0 allocated or 0 not allocated
 */
int check_bitmap(int bitmap_base, int index)
{
	unsigned char buf[sb.block_size];
	read_bytes(bitmap_base, buf, sb.block_size);
	
	int byte_index = index / 8;
	int offset = index % 8;

	return (buf[byte_index] & (1 << offset));
}


/** @brief translate i_mode to file type in ext2 directory 
 *   entry file type
 *  
 *  @param i_mode i_mode field in inode struct
 *  @return int
 */
int imode_to_filetype(__u16 i_mode)
{
	if(EXT2_S_ISOCK(i_mode))
		return EXT2_FT_SOCK;
	if(EXT2_S_ISLNK(i_mode))
		return EXT2_FT_SYMLINK;
	if(EXT2_S_ISREG(i_mode))
		return EXT2_FT_REG_FILE;
	if(EXT2_S_ISBLK(i_mode))
		return EXT2_FT_BLKDEV;
	if(EXT2_S_ISDIR(i_mode))
		return EXT2_FT_DIR;
	if(EXT2_S_ISCHR(i_mode))
		return EXT2_FT_CHRDEV;
	if(EXT2_S_ISFIFO(i_mode))
		return EXT2_FT_FIFO;

	return EXT2_FT_UNKNOWN;
}



/** @brief search a filename in a direct block of a directory
 *  
 *  @param block block buffer
 *  @param filename filename to search
 *  @return inode_num success or -1 fail
 */
int search_filename_in_dir_block(unsigned char* block, char* filename)
{
	__u32 inode_num = 0;
	__u16 entry_size = 0;
	__u8 name_len = 0;
	__u8 type = 0;
	char name[EXT2_NAME_LEN + 1];

	int dir_entry_base = 0;
	while(dir_entry_base < sb.block_size)
	{
		type = *(__u8*)(block + dir_entry_base + 7);
		/* no more directory entries in this block */
		if(type == EXT2_FT_UNKNOWN)
		{
			return -1;
		}
		entry_size = *(__u16*)(block + dir_entry_base + 4);
		name_len = *(__u8*)(block + dir_entry_base + 6);
		memcpy(name, block + dir_entry_base + 8, name_len);
		name[name_len] = '\0';
		
		
		/* if find the filename, return inode number */
		if(strcmp(filename, name) == 0)
		{
			inode_num = *(__u32*)(block + dir_entry_base + 0);
			return inode_num;
		}
		
		dir_entry_base += entry_size;
	}
	return -1;
}


/** @brief search a filename in a singly indirect block of a directory
 *  
 *  @param block block buffer
 *  @param filename filename to search
 *  @return inode_num success or -1 fail
 */
int search_filename_in_singly(unsigned int* block, char* filename)
{
 	unsigned char buf[sb.block_size];
	int inode_num = -1;
	int i = 0;

	for(i = 0; i < (sb.block_size / 4); i++)
	{
		if (block[i] == 0)
			break;

		read_bytes(pt_info.base + block[i] * sb.block_size,
		            buf, sb.block_size);
		inode_num = search_filename_in_dir_block(buf, filename);
		if (inode_num > 0)
			return inode_num;
	}
	return -1;
}


/** @brief search a filename in a doubly indirect block of a directory
 *  
 *  @param block block buffer
 *  @param filename filename to search
 *  @return inode_num success or -1 fail
 */
int search_filename_in_doubly(unsigned int* block, char* filename)
{
	unsigned int buf[sb.block_size];
	int inode_num = -1;
	int i = 0;
	
	for(i = 0; i < (sb.block_size / 4); i++)
	{
		if (block[i] == 0)
			break;

		read_bytes(pt_info.base + block[i] * sb.block_size,
		            buf, sb.block_size);
		inode_num = search_filename_in_singly(buf, filename);
		if (inode_num > 0)
			return inode_num;
	}
	return -1;
}


/** @brief search a filename in a triply indirect block of a directory
 *  
 *  @param block block buffer
 *  @param filename filename to search
 *  @return inode_num success or -1 fail
 */
int search_filename_in_triply(unsigned int* block, char* filename)
{
	unsigned int buf[sb.block_size];
	int inode_num = -1;
	int i = 0;
	
	for(i = 0; i < (sb.block_size / 4); i++)
	{
		if (block[i] == 0)
			break;

		read_bytes(pt_info.base + block[i] * sb.block_size,
		            buf, sb.block_size);
		inode_num = search_filename_in_doubly(buf, filename);
		if (inode_num > 0)
			return inode_num;
	}
	return -1;
}


/** @brief check if s is power of a
 *  
 *  @param s
 *  @param a
 *  @return int
 */
int ispowerof(int s, int a)
{
	if (s <= 0 || a <= 0)
		return 0;
	while (1)
	{
		if (s <= a)
			break;
		s = s / a;
	}
	if (s % a == 0)
		return 1;
	else
		return 0;
}
