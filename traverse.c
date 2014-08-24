/** @file traverse.c
 *  @brief This module contains functions used to traverse entire
 *   file system
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
#include "traverse.h"

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


/** @brief traverse directory and collect file and dir information 
 *
 *  @param inode_num inode number of current directory
 *  @param parent inode number of parent directory
 *  @return void
 */
void traverse_dir(unsigned int inode_num, unsigned int parent)
{
	struct ext2_inode inode;
	int inode_addr = 0;
	
	/* get inode addr (in byte) from inode number */
	inode_addr = get_inode_addr(inode_num);
	
	/* read inode information from inode table entry */
	read_bytes(inode_addr, &inode, sizeof(struct ext2_inode));

	/* if it's not a directory, return */
	if(!EXT2_S_ISDIR(inode.i_mode))
		return;

	unsigned char buf[sb.block_size]; /* 1024 bytes */
	/* search in direct blocks */
	int i = 0;
	for(i = 0; i < EXT2_NDIR_BLOCKS; i++)
	{
		if (inode.i_block[i] <= 0)
			continue;
		if (inode_num == 11)
			printf("traversing direct block[%d] of indoe 11\n", i);
			
		int disk_offset = pt_info.base + inode.i_block[i] * sb.block_size;
		read_bytes(disk_offset, buf, sb.block_size);

		/* traverse direct block[i] */
		traverse_direct_block(disk_offset, i, buf, inode_num, parent);
	}
	
	/* traverse singly indirect block */
	read_bytes(pt_info.base + inode.i_block[EXT2_IND_BLOCK] * sb.block_size, 
			    buf, sb.block_size);
	traverse_singly((unsigned int*)buf, inode_num, parent);
	
	/* traverse doubly indirect block */
	read_bytes(pt_info.base + inode.i_block[EXT2_DIND_BLOCK] * sb.block_size, 
			    buf, sb.block_size);
	traverse_doubly((unsigned int*)buf, inode_num, parent);
	
	/* traverse triply indirect block */
	read_bytes(pt_info.base + inode.i_block[EXT2_TIND_BLOCK] * sb.block_size, 
			    buf, sb.block_size);
	traverse_triply((unsigned int*)buf, inode_num, parent);

	return;
}


/** @brief traverse in direct block of a directory 
 *  
 *  @param buf block content
 *  @param current_dir inode number of current dir
 *  @param parent_dir inode nmber of parent dir
 */
void traverse_direct_block(int block_offset,
						   int block_num,
						   unsigned char* buf, 
                           unsigned int current_dir, 
                           unsigned int parent_dir)
{
	struct ext2_dir_entry_2 dir_entry;

	int dir_entry_base = 0;

	int cnt = 1;
	while(dir_entry_base < sb.block_size)
	{
		dir_entry.file_type = *(__u8*)(buf + dir_entry_base + 7);
		/* no more directory entries in this block */

		dir_entry.inode = *(__u32*)(buf + dir_entry_base + 0);
		dir_entry.rec_len = *(__u16*)(buf + dir_entry_base + 4);
		dir_entry.name_len = *(__u8*)(buf + dir_entry_base + 6);
		memcpy(dir_entry.name, buf + dir_entry_base + 8, dir_entry.name_len);
		dir_entry.name[dir_entry.name_len+1] = '\0';
		
		/* check '.' entry */
		if (cnt == 1 && block_num == 0)
		{
			if(strcmp(dir_entry.name, ".") != 0 || 
			          dir_entry.inode != current_dir)
			{
				printf("error in \".\" of dir %d should be %d\n", 
				        dir_entry.inode, current_dir);
				/* write back to disk */
				set_inode_num(current_dir, parent_dir, 
				              block_offset + dir_entry_base, FIX_SELF);
				/* update block buf */
				*(__u32*)(buf + dir_entry_base + 0) = current_dir;
			}
		}
		/* check '..' entry */
		if (cnt == 2 && block_num == 0)
		{
			if(strcmp(dir_entry.name, "..") != 0 || 
			          dir_entry.inode != parent_dir)
			{
				printf("error \"..\" in dir %d, should be %d\n", 
				        current_dir, parent_dir);
				/* write back to disk */
				set_inode_num(current_dir, parent_dir, 
				              block_offset + dir_entry_base, FIX_PARENT);
				/* update block buf */
				*(__u32*)(buf + dir_entry_base + 0) = parent_dir;
			}
		}
		/* get inode again after possible fix */
		dir_entry.inode = *(__u32*)(buf + dir_entry_base + 0);
		
		/* update local inode map */
		if (dir_entry.inode <= sb.num_inodes)
		{
			my_inode_map[dir_entry.inode] += 1;
			if (dir_entry.inode == 4099 || dir_entry.inode == 4100)
			{
				printf("adding my_inode_map[%d] in parent %d\n", current_dir, parent_dir);
			}
		}
		
		/* recursively traverse sub-directory in this folder */
		if (dir_entry.file_type == EXT2_FT_DIR 
		  && my_inode_map[dir_entry.inode] <= 1
		  && (cnt>2 || block_num > 0) )
			traverse_dir(dir_entry.inode, current_dir);
		
		dir_entry_base += dir_entry.rec_len;
		cnt++;
	}
	return;
}


/** @brief traverse in singly indirect block of a directory 
 *  
 *  @param buf block content
 *  @param current_dir inode number of current dir
 *  @param parent_dir inode nmber of parent dir
 */
void traverse_singly(unsigned int* singly_buf, 
                     unsigned int current_dir, 
                     unsigned int parent_dir)
{
	unsigned char direct_buf[sb.block_size];
	int i = 0;
	for(i = 0; i < (sb.block_size / 4); i++)
	{
		if (singly_buf[i] == 0)
			break;

		int disk_offset = pt_info.base + singly_buf[i] * sb.block_size;
		read_bytes(disk_offset, direct_buf, sb.block_size);
		traverse_direct_block(disk_offset, 1, direct_buf, 
		                      current_dir, parent_dir);
	}
	return;
}


/** @brief traverse in doubly indirect block of a directory 
 *  
 *  @param buf block content
 *  @param current_dir inode number of current dir
 *  @param parent_dir inode nmber of parent dir
 */
void traverse_doubly(unsigned int* doubly_buf, 
                     unsigned int current_dir, 
                     unsigned int parent_dir)
{
	unsigned int singly_buf[sb.block_size];
	int i = 0;
	for(i = 0; i < (sb.block_size / 4); i++)
	{
		if (doubly_buf[i] == 0)
			break;

		read_bytes(pt_info.base + doubly_buf[i] * sb.block_size,
		            singly_buf, sb.block_size);
		traverse_singly(singly_buf, current_dir, parent_dir);
	}
	return;
}


/** @brief traverse in triply indirect block of a directory 
 *  
 *  @param buf block content
 *  @param current_dir inode number of current dir
 *  @param parent_dir inode nmber of parent dir
 */
void traverse_triply(unsigned int* triply_buf, 
                     unsigned int current_dir, 
                     unsigned int parent_dir)
{
	unsigned int doubly_buf[sb.block_size];
	int i = 0;
	for(i = 0; i < (sb.block_size / 4); i++)
	{
		if (triply_buf[i] == 0)
			break;

		read_bytes(pt_info.base + triply_buf[i] * sb.block_size,
		            doubly_buf, sb.block_size);
		traverse_singly(doubly_buf, current_dir, parent_dir);
	}
	return;
}

