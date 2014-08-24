/** @file fsck.c
 *  @brief fsck module contains most important interface
 *   functions for myfsck program
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
#include "directory.h"
#include "block.h"

/*** global variables ***/
/** partition information */
partition_t pt_info;
/** superblock information */
superblock_t sb;
/** block group discriptor table */
struct ext2_group_desc* bg_desc_table = NULL;
/** local inode map */
int* my_inode_map = NULL;
/** local local map */
int* my_block_map = NULL;
/** disk bitmap */
unsigned char* bitmap;

/** @brief initialize partition and superblock information 
 *   of a given partition
 *
 *  @param partition_num partition number
 *  @return 0 success or -1 fail 
 */
int fsck_partition_init(int partition_num)
{
	if (read_partition_info(partition_num, &pt_info) == -1)
	{	
		printf("read superblock info of partition %d failed\n", 
		        partition_num);
		return -1;
	}
	if (read_superblock_info(partition_num) == -1)
	{
		printf("read superblock info of partition %d failed\n", 
		        partition_num);
		return -1;
	}
	if ((read_bg_desc_table(partition_num)) == -1)
	{
		printf("read bg descriptor table of partition %d failed\n",
		        partition_num);
		return -1;
	}
	return 0;
}


/** @brief read partition information into struct partition_t
 *
 *  @param partition_num partition number
 *  @param pt partition info struct pointer
 *  @return 0 success or -1 fail 
 */
int read_partition_info(int partition_num, partition_t *pt)
{
	if(partition_num <= 0)
		return -1;
	else
		pt->partition_num = partition_num;

	unsigned char buf[SECTOR_SIZE];
	unsigned int pe_offset = BOOTSTRAP_SIZE;
	
	/* read master boot record */
	read_sector(0, buf, SECTOR_SIZE);
	
	/* if the partition is a master partition */
	if(partition_num <= 4)
	{
		pe_offset += PARTITION_ENTRY_SIZE * (partition_num - 1);
		pt->type = buf[pe_offset + 0x4];
		pt->start_sec = *(int*)(buf + pe_offset + 0x8);
		pt->base = pt->start_sec * SECTOR_SIZE;
		pt->length = *(int*)(buf + pe_offset + 0xc);
		return 0;
	}
	
	/* if the partition is an extended partition */
	/* first find the master partition that is divided
	 * into ext partitions */
	int i;
	for(i = 1; i <= 4; i++)
	{
		pe_offset = BOOTSTRAP_SIZE + PARTITION_ENTRY_SIZE * (i - 1);
		int type = buf[pe_offset + 0x4];
		/* extended partition is found */
		if(type == DOS_EXTENDED_PARTITION)
			break;
	}
	/* no extended block found */
	if(i > 4)
		return -1;

	/* read extended partition information */
	int first_ebr = *(int*)(buf + BOOTSTRAP_SIZE + 16 * (i-1) +8);
	int partition_first_sec = first_ebr;
	
	/* locating the extended partition */
	for (i = 5; i < partition_num; i++)
	{
		/* read first EBR */
		read_sector(partition_first_sec, buf, SECTOR_SIZE);
		
		/* if check ebr partition entry 2 is 0 */
		if((*(int64_t*)(buf + BOOTSTRAP_SIZE + 16)) == 0 &&
		   (*(int64_t*)(buf + BOOTSTRAP_SIZE + 16 + 8)) == 0)
			break;

		/* get the start sector of of next ext partition */
		partition_first_sec = first_ebr + *(int*)(buf + BOOTSTRAP_SIZE + 24);
	}
	/* reach the end of ext blocks and the given partition doesn't exist. */
	if(i < partition_num)
	{
		//printf("partition %d does not exist\n", partition_num);
		return -1;
	}

	read_sector(partition_first_sec, buf, SECTOR_SIZE);
	/* read the infor of partition partition_num */
	pt->type = buf[BOOTSTRAP_SIZE + 4];
	pt->start_sec = partition_first_sec + *(int*)(buf + BOOTSTRAP_SIZE + 8);
	pt->base = pt->start_sec * SECTOR_SIZE;
	pt->length = *(int*)(buf + BOOTSTRAP_SIZE + 12);

	return 0;
}


/** @brief read superblock information into struct ext2_super_block
 *
 *  @param partition_num partition number
 *  @param sb superblock info struct pointer
 *  @return 0 success or -1 fail
 */
int read_superblock_info(int partition_num)
{
	partition_t pt;
	struct ext2_super_block sb_t; 

	if(read_partition_info(partition_num, &pt) == -1)
	{
		fprintf(stderr, "read_partition[%d] error!\n", partition_num);
		return -1;
	}

	/* read super block from offset 1024 of the partition start sector */
	read_sector(pt.start_sec + 1024/SECTOR_SIZE, &sb_t, 
	            sizeof(struct ext2_super_block));
	
	/** set global variable sb **/
	sb.block_size = EXT2_BLOCK_SIZE(&sb_t);
	sb.inode_size = EXT2_INODE_SIZE(&sb_t);

	sb.num_blocks = sb_t.s_blocks_count;
	sb.blocks_per_group = EXT2_BLOCKS_PER_GROUP(&sb_t);

	sb.num_inodes = sb_t.s_inodes_count;
	sb.inodes_per_group = EXT2_INODES_PER_GROUP(&sb_t);
	
	/* calculate number of blocks */
	sb.num_groups = (sb.num_blocks-1) / sb.blocks_per_group + 1;
	
	printf("************ partition %d *************\n", pt_info.partition_num);
	printf("start sector = %d  base = %d\n", pt_info.start_sec, pt_info.base);
	printf("block size = %d\n", sb.block_size);
	printf("inode size = %d\n\n", sb.inode_size);
	printf("number of blocks = %d\n", sb.num_blocks);
	printf("blocks per group = %d\n\n", sb.blocks_per_group);
	printf("number of inodes = %d\n", sb.num_inodes);
	printf("inodes per group = %d\n\n", sb.inodes_per_group);
	printf("number of groups = %d\n", sb.num_groups);
	printf("**************************************\n\n");
	return 0;
}



/** @brief read block group descriptor table 
 *
 *  @return 0 success or -1 if fail.
 */
int read_bg_desc_table()
{
	if(bg_desc_table != NULL)
		free(bg_desc_table);

	int bg_desc_size = sizeof(struct ext2_group_desc);
	
	/* allocate block group descriptor table (array) */
	bg_desc_table = 
	   (struct ext2_group_desc*)malloc(bg_desc_size * sb.num_groups);
	if (bg_desc_table == NULL)
		return -1;

	/* read block group table from sector 4 of the partition */
	read_sector(pt_info.start_sec + 2048/SECTOR_SIZE, 
	            bg_desc_table, bg_desc_size * sb.num_groups);

	return 0;
}



/** @brief check errors and fix them 
 *
 *  @param partition_num partition number
 *  @param 0 success or -1 if fail
 */
int fix_fs(int partition_num)
{
	if (fsck_partition_init(partition_num) == -1)
		return -1;
	
	my_inode_map = (int*)malloc((sb.num_inodes+1) * sizeof(int));
	/* initialize local inode map */
	int i = 0;
	for (; i<= sb.num_inodes; i++)
		my_inode_map[i] = 0;

	/* traverse and check file system */
	traverse_dir(EXT2_ROOT_INO, EXT2_ROOT_INO);
	
	/*** pass 2 - fix missing inodes ***/
	fix_unreferenced_inode();

	/* traverse again */
	for (i = 0; i<= sb.num_inodes; i++)
		my_inode_map[i] = 0;
	/* traverse and check file system */
	traverse_dir(EXT2_ROOT_INO, EXT2_ROOT_INO);

	/*** pass 3 - fix wrong link counts ***/
	fix_link_counts();

	/*** pass 4 - fix block map ***/
	fix_block_map();

	printf("\n");
	
	free(my_inode_map);
	free(my_block_map);
	return 0;
}


/** @brief fix unreferenced inode 
 *  
 *  @return void
 */
void fix_unreferenced_inode()
{
	int inode_addr;;
	struct ext2_inode inode;
	int i = 0, j = 0;
	int parent = 0, parent_missing = 0;

	int num = 0;
	/* get number of referenced inodes */
	for (i = 1; i<= sb.num_inodes; i++)
	{
		/* get inode addr (in byte) from inode number */
		inode_addr = get_inode_addr(i);
		
		/* read inode information from inode table entry */
		read_bytes(inode_addr, &inode, sizeof(struct ext2_inode));
		
		if (my_inode_map[i] == 0 && inode.i_links_count > 0)
			num++;
	}
	/* collect missing inodes, put them into uref_inodes array */
	int uref_inodes[num+1];
	int cnt = 1;
	for (i = 1; i<= sb.num_inodes; i++)
	{
		/* get inode addr (in byte) from inode number */
		inode_addr = get_inode_addr(i);
		
		/* read inode information from inode table entry */
		read_bytes(inode_addr, &inode, sizeof(struct ext2_inode));
		
		if (my_inode_map[i] == 0 && inode.i_links_count > 0)
			uref_inodes[cnt++] = i;
	}
	
	/* begin fixings */
	for (i = 1; i<= num; i++)
	{
		int uref_i = uref_inodes[i];
		/* get inode addr (in byte) from inode number */
		inode_addr = get_inode_addr(uref_i);
		
		/* read inode information from inode table entry */
		read_bytes(inode_addr, &inode, sizeof(struct ext2_inode));

		/* get its file type, if it's not dir, put it into lost+found */
		if (!EXT2_S_ISDIR(inode.i_mode))
		{
			printf("putting %d into lost+found\n", uref_i);
			put_into_lostfound(uref_i);
		}
		else
		{
			parent = get_parent_id(&inode);
			parent_missing = 0;
			for (j = 1; j <= num && j != i; j++)
			{
				if (parent == uref_inodes[j])
				{	parent_missing = 1;
					break;
				}
			}
			if (!parent_missing)
			{
				printf("putting %d into lost+found\n", uref_i);
				put_into_lostfound(uref_i);
			}
		}
	}
}


/** @brief fix wrong link counts 
 *  
 *  @return void
 */
void fix_link_counts()
{
	int inode_addr;;
	struct ext2_inode inode;
	int i = 0;

	/* fix wrong link counts */
	for (i = 1; i<= sb.num_inodes; i++)
	{
		/* get inode addr (in byte) from inode number */
		inode_addr = get_inode_addr(i);
		
		/* read inode information from inode table entry */
		read_bytes(inode_addr, &inode, sizeof(struct ext2_inode));
		
		if (my_inode_map[i] != inode.i_links_count)
		{
			printf("inode %d link count error ", i);
			printf("actual: %d  stored: %d\n",
				    my_inode_map[i], inode.i_links_count);
			inode.i_links_count = my_inode_map[i];
			write_bytes((long)inode_addr, &inode, sizeof(inode));
		}
	}
}


/** @brief fix block allocation map */
void fix_block_map()
{
	my_block_map = (int*)malloc(sb.num_groups*sb.blocks_per_group*4);
	/* first data block start from 0 or 1 ? */
	int s_fst_db = 1024 / sb.block_size;

	/* initialize local inode map */
	int i = 0, j = 0;
	for (i = 0; i< sb.num_groups*sb.blocks_per_group; i++)
	{
		my_block_map[i] = 0;
	}

	int blk_size = sb.block_size;
	/* meta block and superblock covers 2048 bytes */
	int size = 2048;
	/* block group table covers some block */
	int bg_desc_size = sizeof(struct ext2_group_desc);
	size += bg_desc_size * sb.num_groups;
	//int k = (size - 1) / blk_size *blk_size + blk_size;
	int k = (size - 1) / blk_size + 1;
	//printf("k = %d\n",k);
	for (i = 0; i< k; i++)
	{
		my_block_map[i] = 1;
	}

	/* inode table, block bitmap and inode bitmap in each
	 * group also covers some block */
	for (i = 0; i<sb.num_groups; i++)
	{
		/* mark superblock and bg descriptor table backup blocks */
		if (i==1 || ispowerof(i, 3) || ispowerof(i, 5) || ispowerof(i, 7))
		{
			my_block_map[s_fst_db + i * sb.blocks_per_group] = 1;
			my_block_map[s_fst_db + i * sb.blocks_per_group + 1] = 1;
		}

		/* block bitmap takes one block */
		int addr = bg_desc_table[i].bg_block_bitmap;
		my_block_map[addr] = 1;
		/* inode bitmap takes one block */
		addr = bg_desc_table[i].bg_inode_bitmap;
		my_block_map[addr] = 1;
		
		/* inode table takes several blocks */
		addr = bg_desc_table[i].bg_inode_table;
		size = sb.inodes_per_group * sb.inode_size;
		k = (size - 1) / blk_size + 1;
		//printf("addr = %d  k = %d\n", addr, k);
		for (j = addr; j < addr + k; j++)
			my_block_map[j] = 1;
	}
	
	for (i = 1; i<= sb.num_inodes; i++)
	{
		if (my_inode_map[i] <= 0)
			continue;
		mark_block(i);
	}

	/* compare block bitmap */
	int num = sb.num_blocks;
	bitmap = (unsigned char*)malloc(sb.block_size);
	int group_num = 0;
	while (num > 0)
	{
		int end = 0;
		if (num >= sb.blocks_per_group)
			end = sb.blocks_per_group;
		else
			end = num - s_fst_db;
		
		read_bytes(pt_info.base + bg_desc_table[group_num].bg_block_bitmap * sb.block_size, 
					bitmap, sb.block_size);
		
		for (i = 0; i< end; i++)
		{
			if ((((bitmap[i/8] & (1<<(i%8))) == 0) && (my_block_map[group_num*sb.blocks_per_group + i+s_fst_db] != 0))
			 || (((bitmap[i/8] & (1<<(i%8))) != 0) && (my_block_map[group_num*sb.blocks_per_group + i+s_fst_db] == 0)) )
			{
				printf("block bitmap %d in group %d wrong, I got %d\n",
					    i, group_num, my_block_map[group_num*sb.blocks_per_group + i]);
				bitmap[i/8] = (bitmap[i/8] & (~(1<<(i%8)))) | (my_block_map[group_num*sb.blocks_per_group + i+ s_fst_db] << (i%8));
			}
		}
		
		/* fix block map */
		write_bytes((long)(pt_info.base + bg_desc_table[group_num].bg_block_bitmap * sb.block_size), 
			               bitmap, sb.block_size);

		group_num++;
		num -= sb.blocks_per_group;
	}

	free(bitmap);
}



/** @brief fix unreferenced inode 
 *
 *  @param inode_num inode number
 *  @return int.
 */
int put_into_lostfound(int inode_num)
{
	struct ext2_inode inode;
	int inode_addr = 0;

	/* get inode addr (in byte) from inode number */
	inode_addr = get_inode_addr(inode_num);
	
	/* read inode information from inode table entry */
	read_bytes(inode_addr, &inode, sizeof(struct ext2_inode));

	struct ext2_dir_entry_2 dir_entry;
	/* inode number */
	dir_entry.inode = inode_num;
	/* name is inode_number */
	sprintf(dir_entry.name, "%d%c", inode_num, '\0');
	dir_entry.name_len = strlen(dir_entry.name) - 1;
	
	/* type and rec_length */
	dir_entry.file_type = imode_to_filetype(inode.i_mode);

	int lf_inodenum = get_inode_by_filepath("/lost+found");
	unsigned int base = get_dir_entry_end(lf_inodenum, 8 + dir_entry.name_len);
	
	dir_entry.rec_len = (base - pt_info.base - 1)/ sb.block_size * sb.block_size 
	                     + sb.block_size - (base - pt_info.base);
	
	if (base < 0)
		return -1;

	int entry_size = 8 + dir_entry.name_len;
	write_bytes(base, &dir_entry, entry_size);

	return 0;
}


/** @brief fix inode number of self and parent record
 *
 *  @param inode_num inode number of current directory
 *  @param parent inode number of parent directory
 *  @param offset disk file offset to write the inode
 *  @param fix_flag flag of fixing self inode or parent inode
 *  
 *  @return void.
 */
void set_inode_num(unsigned int inode_num, unsigned int parent, 
                   int offset, int fix_flag)
{
	if (fix_flag == FIX_SELF)
	{
		write_bytes((long)offset, &inode_num, sizeof(unsigned int));
	}
	else if (fix_flag == FIX_PARENT)
	{
		write_bytes((long)offset, &parent, sizeof(unsigned int));	
	}
}


/** @brief get the parent inode number of a given dir inode
 *
 *  @param inode inode struct
 *  @return parent inode number;
 */
int get_parent_id(struct ext2_inode* inode)
{
	/* read first block of inode */
	unsigned char buf[sb.block_size]; /* 1024 bytes */
	int disk_offset = pt_info.base + inode->i_block[0] * sb.block_size;
	read_bytes(disk_offset, buf, sb.block_size);

	struct ext2_dir_entry_2 dir_entry;
	int dir_entry_base = 0;

	/* skip first dir entry . */
	dir_entry.rec_len = *(__u16*)(buf + dir_entry_base + 4);
	dir_entry_base += dir_entry.rec_len;

	/* go to second entry whic is .. */
	dir_entry.inode = *(__u32*)(buf + dir_entry_base + 0);

	return dir_entry.inode;
}


/** @brief search filename and return its inode
 *
 *  @param path complete path of the file
 *  @return inode number or -1 if the file is not found
 */
int get_inode_by_filepath(const char* filepath)
{
	char path[strlen(filepath) + 1];
	strcpy(path, filepath);
	path[strlen(filepath)] = '\0';

	/* if path is root '/', return inode number 2 */
	if(strcmp("/", path) == 0)
		return EXT2_ROOT_INO;
	
	int inode_num = EXT2_ROOT_INO; /* root inode = 2 */
	struct ext2_inode inode;
	int inode_addr = 0;
	
	char* filename = strtok(path, "/");
	int ret = -1;
	while(filename != NULL)
	{
		/* get inode addr (in byte) from inode number */
		inode_addr = get_inode_addr(inode_num);

		/* read inode information from inode table entry */
		read_bytes(inode_addr, &inode, sizeof(struct ext2_inode));

		/* if it's not a directory, error */
		if(!EXT2_S_ISDIR(inode.i_mode))
			return -1;
		
		int found = 0;
		unsigned char buf[sb.block_size]; /* 1024 bytes */
		/* search in direct blocks */
		int i = 0;
		for(i = 0; i < EXT2_NDIR_BLOCKS; i++)
		{
			if (inode.i_block[i] <= 0)
				continue;
			
			read_bytes(pt_info.base + inode.i_block[i] * sb.block_size, 
			            buf, sb.block_size);

			/* direct blocks */
			ret = search_filename_in_dir_block(buf, filename);
			if (ret > 0)
			{
				found = 1;
				break;
			}
		}
		/* if not found, search in singly indirect block */
		if(!found && inode.i_block[i] > 0)
		{
			read_bytes(pt_info.base + inode.i_block[i] * sb.block_size, 
			            buf, sb.block_size);
			ret = search_filename_in_singly((unsigned int*)buf, filename);
			if (ret > 0)
			{
				found = 1;
				break;
			}
		}
		i++;
		/* if still not found, search in doubly indirect block */
		if(!found && inode.i_block[i] > 0)
		{
			read_bytes(pt_info.base + inode.i_block[i] * sb.block_size, 
			            buf, sb.block_size);
			ret = search_filename_in_doubly((unsigned int*)buf, filename);
			if (ret > 0)
			{
				found = 1;
				break;
			}
		}
		i++;
		/* if still not found, search in triply indirect block */
		if(!found && inode.i_block[i] > 0)
		{
			read_bytes(pt_info.base + inode.i_block[i] * sb.block_size, 
			            buf, sb.block_size);
			ret = search_filename_in_triply((unsigned int*)buf, filename);
			if (ret > 0)
			{
				found = 1;
				break;
			}
		}
		/* At last, filename is not found in this directory */
		if(!found)
		{
			printf("file %s not found\n", filename);
			return -1;
		}
		inode_num = ret;
		filename = strtok(NULL, "/");
	}

	return inode_num;
}


