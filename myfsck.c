/** @file myfsck.c
 *  @brief myfsck main module
 *  
 *   This file contains main function for myfsck program
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#include "genhd.h"
#include "ext2_fs.h"
#include "fsck.h"

int disk;  /* file descriptor of disk image*/

/** @breif main function  */
int main (int argc, char **argv)
{
	char* disk_name = NULL;
	int prt_partition_num = -1;
	int fix_partition_num = -1;
	int i = 0;

	if(argc == 1)
	{
		printf("invalid arguments\n");
		exit(-1);
	}

	int opt;
	while((opt = getopt(argc, argv, ":i:p:f:")) != -1)
	{
		switch(opt)
		{
			case 'i':
				disk_name = argv[optind-1];
				break;
			case 'p':
				prt_partition_num = atoi(optarg);
				break;
			case 'f':
				fix_partition_num = atoi(optarg);
				break;
			case ':':
				printf("\nmissing arguments after -%c\n", optopt);
				exit(-1);
			case '?':
				printf("\nargument error: -%c\n", optopt);
			default:
				printf("invalid argument list\n");
				exit(-1);
		}
	}

	/* open the disk file */
	if((disk = open(disk_name, O_RDWR, S_IRUSR|S_IWUSR)) == -1)
	{
		perror("Could not open disk file!");
		exit(-1);
	}
	
	partition_t pt_info;
	/* print partition information */
	if(prt_partition_num > 0)
	{
		if(read_partition_info(prt_partition_num, &pt_info) == -1)
		{
			printf("-1\n");
			close(disk);
			exit(-1);
		}
		printf("0x%02X %d %d\n", pt_info.type, pt_info.start_sec, 
		                         pt_info.length);
	}
	if(fix_partition_num >= 0)
	{
		if (fix_partition_num > 0)
		{
			fix_fs(fix_partition_num);
		}
		else if (fix_partition_num == 0)
		{
			i = 1;
			while (1)
			{
				if (read_partition_info(i, &pt_info) == -1)
					break;
				if (pt_info.type == 0x83)
					fix_fs(i);
				i++;
			}
		}
	}

	close(disk);
	return 0;
}

