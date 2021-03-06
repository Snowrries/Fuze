/*
  Copyright (C) 2015 CS416/CS516

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.
*/

#include <sys/stat.h>
#include <time.h>

#ifndef _BLOCK_H_
#define _BLOCK_H_

#define BLOCK_SIZE 512
#define DIRECT_SIZE 10

typedef struct indirect_t {
	int blocks[128]; //stores block numbers of other blocks which contain dirent blocks or another indirect_t
}indirect;

typedef struct superblock{ //Volume control block
	int total_inodes;
	int total_blocks;
	int itable_block_num; //block location of our inode table
}spb; //This thing must be exactly 512 bytes.(write to blocknumber 0)

typedef struct direntry_t {
	char name[28];
	int inode_number; 
}direntry; //size 32 

typedef struct inode_t {
  //Universal to all Inodes
  int inode_number; //root starts with inode #2
  mode_t mode; //can this file be read/written/executed
  uid_t uid; //Do we need this
  gid_t gid;
  size_t size;
  struct timespec access_time;
  struct timespec create_time;
  struct timespec modify_time;
   /*define IFILE 0 //Inode is a file
   define IDIR 1  //Inode is a directory*/
  int inodetype; 
  int direct[DIRECT_SIZE];
  int single_indirect;
  int double_indirect;
} inode;

extern inode in_table[];
extern char data_bitmap[];
extern char inode_bitmap[];
void disk_open(const char* diskfile_path);
void disk_close();
int block_read(const int block_num, void *buf);
int block_write(const int block_num, const void *buf);

#endif
