/** @file readwrite.c
 *  @brief This module contains functions for io read/write
 *
 *  @author: Hang Yuan (hangyuan)
 *  @bug: No bugs found yet
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <assert.h>
#include "genhd.h"
#include "ext2_fs.h"
#include "readwrite.h"

extern int disk;


/** @brief read bytes from disk
 *  
 *  @param base base address 
 *  @param buf buffer to store read bytes
 *  @param buf_len length of bytes to read
 */
void read_bytes(long base, void* buf, int buf_len)
{
	int ret;
	long lret;
	
	if((lret = lseek64(disk, base, SEEK_SET)) != base)
	{
		printf("Seek to position %ld failed:\n", base);
		exit(-1);
	}
  
	if((ret = read(disk, buf, buf_len)) != buf_len)
	{
		printf("Read disk failed in read_bytes\n");
		exit(-1);
	}
}


/** @brief write bytes to disk
 *  
 *  @param base base address 
 *  @param buf source of bytes
 *  @param buf_len length of bytes to write
 */
void write_bytes(long base, void* buf, int buf_len)
{
	int ret;
	long lret;

	if(base != (lret = lseek(disk, base, SEEK_SET)))
	{
		printf("Seek to position %ld failed:\n", base);
		exit(-1);
	}
	if(buf_len != (ret = write(disk, buf, buf_len)))
	{
		printf("Write disk failed in write_bytes\n");
		exit(-1);
	}
}


/** @brief read sectors
 *  
 *  @param block sector numbre from which to read
 *  @param buf buffer to store read sectors
 *  @param buf_len length of bytes to write
 */
void read_sector(long sector, void *into, int buf_len)
{
  	int ret;
  	long lret;
  	
  	if ((lret = lseek64(disk, sector * SECTOR_SIZE, SEEK_SET)) 
      	!= sector * SECTOR_SIZE) 
  	{
    	printf("Seek to position %ld failed\n", sector * SECTOR_SIZE);
    	exit(-1);
  	}
  	if ((ret = read(disk, into, buf_len)) != buf_len)
  	{
    	printf("read disk failed in read_sector\n");
    	exit(-1);
  	}
}


