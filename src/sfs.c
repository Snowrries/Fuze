/*
  Simple File System

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.

*/

#include "params.h"
#include "block.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "log.h"


///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h
//


/*
Idea is to use statically allocated inodes for now. That means a limited number of 
things to work with.  With a large amount of files we might have problems providing
inodes for all our files.

The main assignment is a direct mapped system; 
An inode has 22 direct pointers to data, and can only store
22*512 bytes per file without entering indirection

Indirect Blocks:
Once the file grows beyond the direct mapped size the filesystem will grab a data block and
start using it to store additional block pointers.  The address of this block will be stored
in the inode.

We will have two levels of indirection, allowing for up to ~2gb files to be stored.

*/


 #define IFILE 0 //Inode is a file
 #define IDIR 1  //Inode is a directory

 #define BLOCK_SIZE 512
 #define MAX_SIZE 64 //64 is arbitrary
 #define MAX_BLOCKS 128
 #define MAX_NODES_PER_BLOCK ((BLOCK_SIZE)/sizeof(inode)) 
 #define MAX_NODES 64
 #define INODE_TLB_BLKS (MAX_NODES/MAX_NODES_PER_BLOCK)
 
//More of a bytemap.
char data_bitmap[MAX_BLOCKS]; 
char inode_bitmap[MAX_SIZE];
inode in_table[MAX_NODES];


direntry init_direntry(int n, char *name){
  direntry *new_dir = malloc(sizeof(direntry));
  strncpy(new_dir->name,name,sizeof(new_dir->name));
  new_dir->inode_number = n;
  return *new_dir;
}



//First thing in our memory/disk
//Superblock keeps track of our filesystem info
//Contains metadata on filesystems. 

//The most important item in the superblock object is s_op, which is the superblock operations table. 
//The superblock operations table is represented by struct super_operations and is defined in <linux/fs.h>. 

//Inode adventures.
//get_inode_fragment takes an array of 22 block pointers. These are guaranteed to be pointers to 16 direntry structs.
//If any link directly to the requested path fragment, it will return 0. 
//If it doesn't exist and will not be in the indirect nodes, it will return -1. 
//If it doesn't exist but might be in an indirect, it will return 1.
int get_inode_fragment(char* frag, int direct){
	direntry dirArray[16]; //512 bytes / block and 32 bytes/direntry. 16 direntries in a dirArray
	int result = 0;
	int j;
	char buffer[PATH_MAX];
	if(result = block_read(direct, dirArray) <= 0){
		return -1; //Couldn't read?
	}
	memset(buffer, 0, PATH_MAX);
	memcpy(buffer,frag,sizeof(frag));
	for(j = 0; j < 16; j++){
		if(!strncmp(dirArray[j].name, buffer, 27)){
			return dirArray[j].inode_number;
		}
	}

	return -2; //Doesn't exist in given node.
}
//get_inode takes a path and returns an inode_number, checking for errors along the way.
//Returns the inode_number of the inode on success.
//Returns -1 on file not existing. Prints error to log.
//Returns -2 if the pathname is invalid.
//Returns -4 if the directory is full, and path does not exist.

int get_inode(const char *path){//Returns the inode_number of an inode
//Assume path is a valid, null terminated path name.
	int num = 0;
	int cur_inode_number = 0; //Start at root!
	int running = sizeof(path)+1;
	inode curnode;
	char patho[running+1];
  	strncpy(patho, path, pathlen);
  	patho[pathlen] = '/';
	int found = 0 ;
	char buffer[PATH_MAX];
	int result;
	int offset = 0;
	int i;
	int j;
	int block_num;
	int buf[128];
	int buf2[128];
	
	while((num = num + parse_path(&patho[offset], buffer) + 1 ) <= running){
		//Fuse truncates all ending slashes for some reason. Thus, we pad the given path with a trailing slash.
		//Then, we now have a normalized representation of a path, where every segment has a trailing /. 
		
	//Path always ends in a /. At the last bit of the path, it'll return -1.
	//That is, we found 'something/', or just '/'. if we find 'something', that's considered nothing.
	//We end when we've parsed all the path.
		//Starting at root directory. Read dirents.
		//If path is valid, this is always a directory until the very end.
		
		curnode = in_table[cur_inode_number];
		if(curnode.inodetype == IFILE){
			if(num < running){
				//There is more path, but we hit a file... That's an error.
				log_msg("File is not a directory! Path: %s", path);
				return -2;
			}
			//Otherwise, we're just at the last part.
		}
		//Guaranteed to be a directory
		

	for(i = 0; i < DIRECT_SIZE; i++){
		if(result = get_inode_fragment(buffer, curnode.direct[i]) == -1){
			if(num < running){
				//There is more path, but we can't find the directory... That's an error.
				log_msg("Invalid path! Path: %s", path);
				return -2;
			}
			log_msg("File does not exist. Path: %s", path);
			return -1;
			//Critical error!
		}
		if(result > -1){
			break;
		}
	}
		//if result == -2, we need to check indirects.
		//Indirect checking
		if(result == -2){
			block_read(curnode.single_indirect, buf);
			for(i = 0; i<128; i++){
				//buf is an int array of length 128. 
				
				if(result = get_inode_fragment(buffer,buf[i]) == -1){
					if(num < running){
						//There is more path, but we can't find the directory... That's an error.
						log_msg("Invalid path! Path: %s", path);
						return -2;
					}
					log_msg("File does not exist. Path: %s", path);
					return -1;
					//Critical error!
				}
				if(result > -1){
					break;
				}
			}
		}
		if(result == -2){
			block_read(curnode.double_indirect, buf);
			for(i = 0; i<128; i++){
				block_read(buf[i], buf2);
				for (j = 0; j < 128; j++){
					if(result = get_inode_fragment(buffer, buf2[j]) == -1){
						if(num < running){
							//There is more path, but we can't find the directory... That's an error.
							log_msg("Invalid path! Path: %s", path);
							return -2;
						}
						log_msg("File does not exist. Path: %s", path);
						return -1;
						//Critical error!
					}
					if(result > -1){
						break;
					}
				}
				if(result > -1){
					break;
				}
			}
		}
		if(result == -2){
			log_msg("File does not exist. Path: %s", path);
			return -4;
			//Ridiculous. Doesn't exist in the direct, indirect, and double indirect... And they were all full!
		}
		offset = offset+num+1;
		cur_inode_number = result;
	}
	return cur_inode_number;
}
//Finds the leftmost segment of a valid pathname, and returns it terminated by a null byte, without the ending /
//Ex: segment/junk/morejunk/ 
//>> return: "segment"
//Ex: /segment/junk/morejunk/
//>> return: ""
//Note that root is an unnamed directory.
int parse_path(const char *path, char* buffer){
	char a;
	int pathlen = strnlen(path, PATH_MAX);
	char tpath[pathlen+1];
	int offset = 0;
	strncpy(tpath, path, pathlen);
	tpath[pathlen] = '\0';
	int i = 0 ;
	while( (a = tpath[offset]) != '/'){
		buffer[i] = a;
		offset++;
		i++;
		if(i == pathlen+1){ //pathlen is the length of the path minus the null byte, if there is one. 
		//If we read to the null byte, and there wasn't a / preceding it, this path is invalid.
			return -1;
		}
	}
	buffer[i] = '\0';
	//the return value is the number of characters up to the ending /.
	// input: forexample/ would return 10.
	// input: / would return 0.
	return i;
}
//Copies the path of the parent to buf.
//returns -1 on error.
//returns 0 on success.
int find_parent(const char *path, char* buf){
	
	char buffer[PATH_MAX];
	int pathlen = strnlen(path, PATH_MAX);
	char tpath[pathlen+1];
  	strncpy(tpath, path, pathlen);
	tpath[pathlen] = '\0';
	int offset = 0;
	int offparent = 0;
	int numread = 0;
	int retval = 0;
/*	if(strncmp(path, "/", 2)){
		strncpy(buf, "/", 2);
		return 0;
	}
	//Now it's ensured to have a parent; at least root.
	buffer[0] = '/';
*/
	
	while (offset < pathlen){
		if(numread = parse_path(&tpath[offset], buffer) < 0){
			retval = -1; //Error!
			break;
		}
		offset = offset + numread + 1;
		if(offset == pathlen){
			retval = 0;
			break;
		}
		offparent = offset;
	}
	if(offset > pathlen){
		return -1; 
		//?? Unspecified error. This shouldn't happen.
	}
	strncpy(buf,path,offset);
	buf[offset] = '\0';
	return 0;
}




void *sfs_init(struct fuse_conn_info *conn)
{
    int b;
    int k;
    fprintf(stderr, "in bb-init\n");
    log_msg("\nsfs_init()\n");
    
    log_conn(conn);
    log_fuse_context(fuse_get_context());
    /* 
    Tony: Assignment Description Says we must store and organize
    all our data into a flat file. 
    */
    disk_open((SFS_DATA)->diskfile);
    
    char* buf = malloc(BLOCK_SIZE);
    if(!(block_read(0,buf)>0)){
      spb superblk;
      superblk.total_inodes = MAX_NODES;
      superblk.total_blocks = MAX_BLOCKS;
      char temp[BLOCK_SIZE];
      memset(temp,0,BLOCK_SIZE);
      memcpy(temp,&superblk,sizeof(superblk));

      if(block_write(0,&temp) >0){
        log_msg("\n SUPERBLOCK initialized");
      }

      memset(data_bitmap,0,sizeof(data_bitmap));
      int n = INODE_TLB_BLKS +4;
      int i;
      for(i = 0; i < n;i++){
          data_bitmap[i] = 1;
      }
      if(block_write(1,&data_bitmap) >0){
        log_msg("\n DATABITMAP initialized");
      }

      inode_bitmap[0] = 1;
      memset(temp,0,BLOCK_SIZE);
      memcpy(temp,inode_bitmap,sizeof(inode_bitmap));
      if(block_write(2,&temp)>0){
        log_msg("\n INODEBITMAP initialized");
      }

      //Initialize root
      uid_t uid = getuid(); 
      gid_t gid = getgid();   
      inode *root = malloc(sizeof(inode));
      root->inode_number = 0;
      root->size = sizeof(direntry)*2;
      root->uid = uid;
      root->gid = gid;
      root->inodetype = IDIR;
      root->mode = S_IFDIR | 0755;
      root->single_indirect = -1;
      root->double_indirect = -1;
      root->access_time.tv_sec = 0;
      root->create_time.tv_sec = 0;
      root->modify_time.tv_sec = 0;

      //Initialize root dirent
      char ent1[] = ".";
      char ent2[] = "..";
      direntry *tmp_dirent = calloc(16,sizeof(direntry));
      direntry tmp_ent1 = init_direntry(0,ent1);
      direntry tmp_ent2 = init_direntry(0,ent2);
      tmp_dirent[0] = tmp_ent1;
      tmp_dirent[1] = tmp_ent2;
      int blk = get_free_block();
      root->direct[0] = blk;
      memset(temp,0,BLOCK_SIZE);
      memcpy(temp,tmp_dirent,sizeof(tmp_dirent));
      int info = block_write(blk, &temp);
      
      memset(in_table,0,sizeof(in_table));
      in_table[0] = *root;
      char* buf = malloc(BLOCK_SIZE);
      memset(buf, 0, BLOCK_SIZE);
      k=0;
      for(b =3; b<INODE_TLB_BLKS+3; b++){
         /*REALLY DANGEROUS AND COULD GET OUT OF BOUNDS PLEASE FIX*/
            //INODE_TLB_BLKS+3
          inode *tmp = &in_table[k];
          k = k+4;
          memcpy(buf, tmp, sizeof(buf));
          if(block_write(b,buf) == 0){
            break;
          }
       }

      //inode_numbers explained: This is just a number to uniquely identify our inodes. 2 is always root.
      // The inode_number is the index of the inode in the global static inode array.
     //Init superblock here
    /*  int total_inodes;
     int total_datablocks;
  struct inodes_table global_table; //EWWW someone help me change this
  struct super_operations s_op;  /* superblock methods */ 
  }
  else{
    k=0;
    spb superblock;
    log_msg("\n Reading");
    //SuperBlock is inited
    //Do a bunch of block reads
    //Buffer size should be the size of our inodebitmap/array
    //Read in inode_bitmap/array
    char* buffer = malloc(BLOCK_SIZE);
    memset(buffer,0,BLOCK_SIZE);
    if(block_read(0, buffer)>0){
     memcpy(&superblock,buffer,sizeof(spb));
     log_msg("\n Successful Superblock read");
    }

    //Init data bitmap
    memset(buffer,0,BLOCK_SIZE);
    if(block_read(1, buffer)>0){
       memcpy(data_bitmap,buffer,sizeof(data_bitmap));
       log_msg("\n Successful Data bitmap read");

    }

    //Init Inode Bitmap
    memset(buffer,0,BLOCK_SIZE);
    if(block_read(2, buffer)>0){
       memcpy(inode_bitmap,buffer,sizeof(inode_bitmap));
        log_msg("\n Successful Inode Bitmap read");

    }

    memset(buffer,0,BLOCK_SIZE);
    for (b =3;b<INODE_TLB_BLKS+3;++b){
      if(block_read(b, buffer)>0){
        inode *tmp = &in_table[k];
        k=k+4;
        memcpy(tmp,buffer,sizeof(BLOCK_SIZE));
        log_msg("\n Successful Inode Table read");

        }
      }

  }
    
  /*
  if(block_write(1,&inodes_bitmap)){

  }
  if(block_write(2,&data_bitmap)){
  
  }
  */
    /*
    Have to set block sizes, buffer sizes, max write/reads, inodes
    */
    return SFS_DATA;

}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void sfs_destroy(void *userdata)
{
    log_msg("\nsfs_destroy(userdata=0x%08x)\n", userdata);
    disk_close();
}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
 //Returns -1 if error.
int sfs_getattr(const char *path, struct stat *statbuf)
{
    int retstat = 0;
    int inode_num;
    inode cur_inode;
    log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
    path, statbuf);

    memset(statbuf, 0, sizeof(struct stat));
    inode_num = get_inode(path);
    if(inode_num >-1){
      cur_inode = in_table[inode_num];
      log_msg("\n Logging inode stat");
      statbuf->st_dev = 0; //How are we supposed to know this?
      statbuf->st_ino = inode_num;
      statbuf->st_mode = cur_inode.mode; 
      statbuf->st_nlink = 0; //Hardlinks not implemented
      statbuf->st_uid = cur_inode.uid;
      statbuf->st_gid = cur_inode.gid;
      //statbuf->st_rdev = 0; //What's a special file device id o_o
      statbuf->st_size = cur_inode.size; //Remember to define all these in inode struct later.
      statbuf->st_blocks = 0; //Remember to define me
      statbuf->st_atime = cur_inode.access_time.tv_sec;
      statbuf->st_ctime = cur_inode.create_time.tv_sec;
      statbuf->st_mtime = cur_inode.modify_time.tv_sec; 
    }
    else{
        log_msg("\n Inode not found");
        retstat = -1;
    }   
    log_stat(statbuf);
   
    return retstat;
    
}
 //Returns free block number
 //Returns -1 on error
 int get_free_block(){
 	int i;
 	for(i = 0; i<MAX_BLOCKS; i++ ){
 		if(!data_bitmap[i]){
 			return i;
 		}
 	}
 	return -1;//Disk full
 }
 //Returns free inode number
 //Returns -1 on error
 int get_free_inode(){
 	int i;
 	for(i = 0; i<MAX_NODES; i++ ){
 		if(!inode_bitmap[i]){
 			return i;
 		}
 	}
 	return -1;//No free inodes
 }
 
/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */

 
int sfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    log_msg("\nsfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n",path, mode, fi);
    int inode_num = get_inode(path);
    int block;
    inode *new_node;
    const char* buf = calloc(1,BLOCK_SIZE);
    if(inode_num > 0){
  
      return sfs_open(path, fi);
    }
    else if(inode_num == -1){
    	inode_num = get_free_inode();
    	block = get_free_block();/*Find the first unset block*/
      data_bitmap[block] = 1;
    	inode_bitmap[inode_num] = 1;
      new_node = malloc(sizeof(inode)); //128 bytes
    	new_node->inode_number = inode_num;
    	new_node->mode = mode;
    	new_node->size = 0;
    	new_node->inodetype = IFILE;
    	new_node->direct[0] = block;
    	in_table[inode_num] = *new_node;
      char* tmp = malloc(PATH_MAX);
    	find_parent(path,tmp);
        //Get parent directory
    	block_write(block, buf);
    	
      return inode_num;
      //Get new node location from bitmap/array
      //Populate inode info on the inode table
      //Write the inode to correct location on disk
      //Update bitmap and write to disk
    }
    else{
    	return inode_num; //Error code as defined in get_inode 
    } 

    return inode_num;
    
}

/** Remove a file */
int sfs_unlink(const char *path)
{
  int i = 0;
	int j = 0;
  int k = 0;
	log_msg("sfs_unlink(path=\"%s\")\n", path);
	int inode_number;
	int parent_inode_n;
	int check;
	char buffer[PATH_MAX];
	//char buffer2[PATH_MAX];
	int pardone = 0;
	direntry buf[16];
  int buf1[128];
	int buf2[128];
	inode_number = get_inode(path);
  inode rmnode = in_table[inode_number];
  inode par;
	int indir = rmnode.single_indirect;
	int indir2 = rmnode.double_indirect;
	if(find_parent(path, buffer)<1){
		log_msg("Could not find parent while unlinking");
		return -1; 
	} 
	parent_inode_n = get_inode(buffer);// buffer contains path of parent. 
	par = in_table[parent_inode_n];//par is the parent inode. This is a directory inode. 
	//Remove the dirent of the child from parent directory
	for(i = 0; i < DIRECT_SIZE; i++){
      block_read(par.direct[i],buf);
    for(j =0; j < 16; j++){
		  if(buf[j].inode_number == inode_number){
			 buf[j].inode_number = -1;
			 pardone = 1;
			 break;
		  }
    }
	}
	if(pardone == 0){
		if(block_read(par.single_indirect, buf1) < 0){ //Read in indirect
			return -1; //error
		}
		for(i = 0; i < 128; i++){
      if(block_read(buf1[i], buf) >= 0){ //Read in Direntry Block
       for(j = 0; j<16; j++){
			   if(buf[j].inode_number == inode_number){ //Get Direntry inode number
				  buf[j].inode_number  = -1;
          *(buf[j].name) = '\0';
				  pardone = 1;
				  break;
			   }
        }
		  }
      else{
        return -1;
      }
    }
	}
	if(pardone == 0){
		if(block_read(par.double_indirect,buf2)<0){ //Read in indirect pointers
			return -1; //error
		}
		for(i = 0; i < 128; i++){
			if(block_read(buf2[i],buf1)<0){ //Read in indirects
				return -1; //error
			}	
			for(j = 0; j < 128; j++){ 
        if(block_read(buf1[j],buf)<0){ //Read in Direntry Block
        return -1; //error
       }
        for(k = 0; k<16; k++){ //Get inode number
				if(buf[k].inode_number == inode_number){
					buf[k].inode_number  = -1;
         *(buf[k].name) = '\0';
					break;
				  }
        }
			}
			if(pardone == 1)break;
		}
	}
	if(pardone == 0){
		//...We went through all the indirectness and the file doesn't exist.
		return -1;
	}
	
	
	//Remove the data blocks allocated to the file from the global data bitmap, thereby 'freeing' them for use.
	inode_bitmap[inode_number] = 0;
	while(i < DIRECT_SIZE){
    check = rmnode.direct[i];
    if(check != -1){
		data_bitmap[check] = 0;
    }
    else
      break; 
		i++;
	}
	//Check indirects
  if(check != -1){
		if(block_read(indir, buf1) > 0){
		for(i = 0; i < 128; i++){
      check = buf1[i];
			if(check != -1){
				data_bitmap[check] = 0;
			}
			else{
				break;
			}
		data_bitmap[indir] = 0;
	 }
  }
  else{
    //Error Reading
    return -1;
  }
}
	if(check != -1){
		if(block_read(indir2, buf2) < 0){
      //Error reading
      return -1;
    }
		for(i = 0; i < 128; i++){
			block_read(buf2[i], buf1);
			for(j = 0; j < 128; j++ ){
			 check = buf1[i];
       if(check != -1){
          data_bitmap[check] = 0;
       }
				else{
					break;
				}
			}
			data_bitmap[buf2[i]] = 0;
		}
		data_bitmap[indir2] = 0;
	}

  return 0; //Sucess
}


/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
 //Returns -1 on error
 //Returns inode_number on success
int sfs_open(const char *path, struct fuse_file_info *fi)
{
    	int j = 0;
	log_msg("sfs_open(path=\"%s\")\n", path);
	int inode_number = get_inode(path);
	inode fd = in_table[inode_number];
	if(fd.inodetype == IDIR){
		return -1; //Cannot open a directory as a file
	}
	return inode_number;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int sfs_release(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_release(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    //Nothing to close, since we don't use file handlers. We just pass inode numbers.

    return retstat;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */

int sfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	//Assume *buf is big enough to hold size 
    	int retstat = 0;
    	log_msg("\nsfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);

  int bufferblocks;
  int doublebufferblx;
	int i;
	int j;
	int k;
	inode curnode;
	int running = 0;
	int done = 0;
	int start = 0;
	int inode_num;
	int temp;
	if(inode_num = sfs_open(path, fi) < 0){
		return -1;//Error
	}
	curnode = in_table[inode_num];
	bufferblocks = offset/BLOCK_SIZE;
	char buffer[sizeof(buf)];
	char indirbuf[128];
	char indirbuf2[128];
	//Here, we find the block that our offset is in; that is, our start block.
	//Then we find the offset from the beginning of the block that we have found,
	//And then we set i = -1, -2, or -3 based on where we started our block.
  if(size <= 0){
    //Nothing to read
    return -1;
  }
	if(bufferblocks < DIRECT_SIZE){
		start = offset-bufferblocks*BLOCK_SIZE;
		i = -1;
	}
	else if(bufferblocks < (DIRECT_SIZE + 128)){
		bufferblocks = bufferblocks - DIRECT_SIZE;
		start = offset - bufferblocks*BLOCK_SIZE - DIRECT_SIZE * BLOCK_SIZE;
		i = -2;
	}
	
	else if(bufferblocks < (DIRECT_SIZE + 128 + 16384)){
		doublebufferblx = bufferblocks/BLOCK_SIZE;
		bufferblocks = bufferblocks - DIRECT_SIZE - 128 - (doublebufferblx * 128);
		start = offset - (doublebufferblx*BLOCK_SIZE*BLOCK_SIZE) - (bufferblocks*BLOCK_SIZE) - (DIRECT_SIZE * BLOCK_SIZE) - (128 * BLOCK_SIZE);
		i = -3;
	}
	if(i > -2){ // Direct. 
		for (i = bufferblocks; i < DIRECT_SIZE; i++){
			//Starting at the bufferblocks block
			//Complicated logic here, in my opinion. Read carefully. 
			//Maybe draw it out.
			//Shake it off.
			if(block_read(curnode.direct[i], buffer) < 0){
				return -1;
			}
			if(temp = (size - running - start) < 512 ){
				strncpy(buf+running, buffer+start , temp);
				return size;
			}
			//Read a block normally.
			strncpy(buf+running, buffer+start, BLOCK_SIZE-start);
			running = running+(BLOCK_SIZE-start);
			if(running == size){
				return size;
			}
			start = 0;
		}
		bufferblocks = 0;
	}
	if(i > -3){//Guaranteed to need to read more, otherwise we'd return.
		if(block_read(curnode.single_indirect, indirbuf)<0){
			return -1; //couldn't read!
		}
		for (i = bufferblocks; i < 128 ;i++){
			if(block_read(indirbuf[i], buffer) < 0){
				return -1;
			}
			if(temp = (size - running - start) < 512 ){
				strncpy(buf+running, buffer+start , temp);
				return size;
			}
			//Read a block normally.
			strncpy(buf+running, buffer+start, BLOCK_SIZE-start);
			running = running+(BLOCK_SIZE-start);
			if(running == size){
				return size;
			}
			start = 0;
		}
		bufferblocks = 0;
	}
	if(i > -4){//Guaranteed to need to read more, otherwise we'd return.
		if(block_read(curnode.double_indirect, indirbuf2)<0){
			return -1; //couldn't read!
		}
		for (j = doublebufferblx; j < 128; j++){
			
			if(block_read(indirbuf2[j], indirbuf)<0){
				return -1; //couldn't read!
			}
		
			for (i = bufferblocks; i < 128 ;i++){
				if(block_read(indirbuf[i], buffer) < 0){
					return -1;
				}
				if(temp = (size - running - start) < 512 ){
					strncpy(buf+running, buffer+start , temp);
					return size;
				}
				//Read a block normally.
				strncpy(buf+running, buffer+start, BLOCK_SIZE-start);
				running = running+(BLOCK_SIZE-start);
				if(running == size){
					return size;
			}
				start = 0;
			}
		}	
	}
	return -1; //How'd we get here?
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
int sfs_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
	//Assume *buf is big enough to hold size 
    	int retstat = 0;
    	log_msg("\nsfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);

  	int bufferblocks;
  	int doublebufferblx;
	int i;
	int j;
	int k;
	inode curnode;
	int running = 0;
	int done = 0;
	int start = 0;
	int inode_num;
	int temp;
	if(inode_num = sfs_open(path, fi) < 0){
		return -1;//Error
	}
	curnode = in_table[inode_num];
	bufferblocks = offset/BLOCK_SIZE;
	char buffer[sizeof(buf)];
	char indirbuf[128];
	char indirbuf2[128];
	//Here, we find the block that our offset is in; that is, our start block.
	//Then we find the offset from the beginning of the block that we have found,
	//And then we set i = -1, -2, or -3 based on where we started our block.
	if(bufferblocks < DIRECT_SIZE){
		start = offset-bufferblocks*BLOCK_SIZE;
		i = -1;
	}
	else if(bufferblocks < (DIRECT_SIZE + 128)){
		bufferblocks = bufferblocks - DIRECT_SIZE;
		start = offset - bufferblocks*BLOCK_SIZE - DIRECT_SIZE * BLOCK_SIZE;
		i = -2;
	}
	
	else if(bufferblocks < (DIRECT_SIZE + 128 + 16384)){
		doublebufferblx = bufferblocks/BLOCK_SIZE;
		bufferblocks = bufferblocks - DIRECT_SIZE - 128 - (doublebufferblx * 128);
		start = offset - (doublebufferblx*BLOCK_SIZE*BLOCK_SIZE) - (bufferblocks*BLOCK_SIZE) - (DIRECT_SIZE * BLOCK_SIZE) - (128 * BLOCK_SIZE);
		i = -3;
	}
	if(i > -2){ // Direct. 
		for (i = bufferblocks; i < DIRECT_SIZE; i++){
			//Starting at the bufferblocks block
			//Complicated logic here, in my opinion. Read carefully. 
			//Maybe draw it out.
			//Shake it off.
			if(block_read(curnode.direct[i], buffer) < 0){
				return -1;
			}
			if(temp = (size - running - start) < 512 ){
				strncpy(buffer+start, buf+running, temp);
				if(block_write(curnode.direct[i], buffer) < 0){
					return -1;
				}
				return size;
			}
			else
			{//Write a block normally.
				strncpy(buffer+start, buf+running, BLOCK_SIZE-start);
				if(block_write(curnode.direct[i], buffer) < 0){
					return -1;
				}
				running = running+(BLOCK_SIZE-start);
				if(running == size){
					return size;
				}
			}
			start = 0;
		}
		bufferblocks = 0;
	}
	if(i > -3){//Guaranteed to need to read more, otherwise we'd return.
		if(block_read(curnode.single_indirect, indirbuf)<0){
			return -1; //couldn't read!
		}
		for (i = bufferblocks; i < 128 ;i++){
			if(block_read(indirbuf[i], buffer) < 0){
				return -1;
			}
			if(temp = (size - running - start) < 512 ){
				strncpy(buffer+start, buf+running, temp);
				if(block_write(curnode.direct[i], buffer) < 0){
					return -1;
				}
				return size;
			}
			else
			{//Write a block normally.
				strncpy(buffer+start, buf+running, BLOCK_SIZE-start);
				if(block_write(curnode.direct[i], buffer) < 0){
					return -1;
				}
				running = running+(BLOCK_SIZE-start);
				if(running == size){
					return size;
				}
			}
			start = 0;
		}
		bufferblocks = 0;
	}
	if(i > -4){//Guaranteed to need to read more, otherwise we'd return.
		if(block_read(curnode.double_indirect, indirbuf2)<0){
			return -1; //couldn't read!
		}
		for (j = doublebufferblx; j < 128; j++){
			
			if(block_read(indirbuf2[j], indirbuf)<0){
				return -1; //couldn't read!
			}
		
			for (i = bufferblocks; i < 128 ;i++){
				if(block_read(indirbuf[i], buffer) < 0){
					return -1;
				}
				if(temp = (size - running - start) < 512 ){
					strncpy(buffer+start, buf+running, temp);
					if(block_write(curnode.direct[i], buffer) < 0){
						return -1;
					}
					return size;
				}
				else
				{//Write a block normally.
					strncpy(buffer+start, buf+running, BLOCK_SIZE-start);
					if(block_write(curnode.direct[i], buffer) < 0){
						return -1;
					}
					running = running+(BLOCK_SIZE-start);
					if(running == size){
						return size;
					}
				}
				start = 0;
			}
		}	
	}
	return -1; //How'd we get here?
}
//Returns length of file name
//Places the file name into the buffer;

int get_file_name(const char* path, char* buffer){
	int a = 0;
	int b = strnlen(path, PATH_MAX);
	int c = 0;
	const char* patho = path;
	while( a < b ){
		a = a + c + 1;
		c = parse_path(&patho[a], buffer);
	}
	//strncpy(buffer,path[a],c+1);
	return c;
	
}

int get_parent_path(const char* path, char* name, char* buffer){
	int a;
	int b;
	a = strlen(name);
	b = strlen(path);
	buffer = strncpy(buffer, name, (b-a));
	buffer[b-a] = '\0';
	return 0;
	
}
//Given a directory,
//Find the next free block to write to.
//Returns the blocknum that it writes to.
//Set the block num in the directory pointed to by the inode.
//-1 if there is no free block.

int writedirent(direntry entry, int inode_number){
	log_msg("writedirent");
	int block;
	int i; 
	int j;
	inode dir;
	char buffer[BLOCK_SIZE];
	char buffer2[BLOCK_SIZE];
	block = get_free_block();
	block_write(block, &entry);
	dir = in_table[inode_number];
	if(dir.inodetype == IFILE){
		return -1; //Is file?!
	}
	for(i = 0; i < DIRECT_SIZE; i++){
		if(dir.direct[i] < 0){
			dir.direct[i] = block;
			return block;
		}
	}
	if(dir.single_indirect < 0){
		dir.single_indirect = get_free_block();
		dir.single_indirect = block;
    indirect *tmp = malloc((sizeof(indirect))* 16);
		for(i = 1; i<128; i++){
			tmp->blocks[i] = -1;
		}
    block_write(block, tmp);
	}

	for(i = 0; i < 128; i++){
		if(block_read(dir.single_indirect, buffer) < 0){
			return -1; //Errorsz
		}
		if(buffer[i] < 0){
			buffer[i] = block;
			return block;
		}
	}
	for(i = 0; i < 128; i++){
		if(block_read(dir.double_indirect, buffer2) <0){

	}
	
  }
}


/** Create a directory */
int sfs_mkdir(const char *path, mode_t mode)
{
	log_msg("\nsfs_mkdir(path=\"%s\", mode=0%3o)\n",path, mode);
	int a;
	int parent_in;
	direntry entry;
	char buffer[PATH_MAX];
	a = get_inode(path);
	if( a > 0 && in_table[a].inodetype == IDIR){
		return -1; //Error! It already exists!
	}
	get_file_name(path,entry.name);
	if(entry.inode_number = get_free_inode() <0){
		return -1; //No free inodes.
	}
	find_parent(path,buffer);
	parent_in = get_inode(buffer);
	
	
}


/** Remove a directory */
int sfs_rmdir(const char *path)
{
    int retstat = 0;
    log_msg("sfs_rmdir(path=\"%s\")\n",
	    path);
    
    
    return retstat;
}


/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int sfs_opendir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_opendir(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    
    
    return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */ //Returns -1 if there is more to load into buf.
 //Returns 0 if everything is loaded
int sfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{ 
	//Given a path, read the directory entries.
	//Path is a directory. 
	//Ignore filler, ignore offset, ignore fi. 
	//Put the entirety of the directory entries into buf.
  log_msg("\nsfs_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n",
      path, buf, filler, offset, fi);
	int dirinbuf;
	int i;
	int j;
	int k;
	inode curnode;
	int running = 0 ;
	int done = 0;
	int inode_num = get_inode(path);
	curnode = in_table[inode_num];
	dirinbuf = sizeof(buf)/sizeof(direntry);
	direntry buffer[16];
	 direntry* buf2 = buf;
	char indirbuf[512];
	char indirbuf2[512];
	
	for (i = 0; i < DIRECT_SIZE;i++){
		if(block_read(curnode.direct[i], buffer) < 0){
			return 0; //loaded the entirety of dirent structs into buffer yay
		}
		for (j = 0 ; j < 16 ; j++){
			*(buf2 + running*32) = buffer[j];
			running++;
			if (running > dirinbuf){
				done = 1;
				return -1; // Still have more to load.
			}
		}
	}
	block_read(curnode.single_indirect, indirbuf);
	for (i = 0; i < 128 ;i++){
		if(block_read(indirbuf[i], buffer)){
			return 0; //loaded the entirety of dirent structs into buffer yay
		}
		for (j = 0 ; j < 16 ; j++){
			*(buf2 + running*32) = buffer[j];
			running++;
			if (running > dirinbuf){
				done = 1;
				return -1;// Still have more to load.
			}
		}
	}
	if(block_read(curnode.double_indirect, indirbuf2)){
		return 0; //loaded the entirety of dirent structs into buffer yay
	}
	for (i = 0; i < 128 ;i++){
		if(block_read(indirbuf2[i], indirbuf)){
			return 0; //loaded the entirety of dirent structs into buffer yay
		}
		for(k = 0; k < 128; k++){
			if(block_read(indirbuf[k], buffer)){
				return 0; //loaded the entirety of dirent structs into buffer yay
			}
			for (j = 0 ; j < 16 ; j++){
				*(buf2 + running*32) = buffer[j];
				running++;
				if (running > dirinbuf){
					done = 1;
					return -1;// Still have more to load.
				}
			}
		}
	}
	
    /*
    DIR *somedir;
    struct dirent *entry;
//    struct stat statarg;
    if( (somedir=opendir(path) < 0) {
    	return errno; //Does this have to be negative? PositivE?
    }
    while((entry = readdir(somedir))!= NULL){
//    	memset(&statarg, 0, sizeof(statarg));
//    	statarg.sg_ino= 
	if (filler(buf, entry->d_name, NULL, 0)){
		closedir(somedir);
		return 0;
	}
	//If readdir is null, that's all the stuff in the directory
	//If filler is 0, something's wrong.
	//Just quit in either case. They'll never know! 
    }
    closedir(somedir);
    return 0;
    */
}


  /**
   * Change the access and modification times of a file with
   * nanosecond resolution
   *
   * This supersedes the old utime() interface.  New applications
   * should use this.
   *
   * See the utimensat(2) man page for details.
   *
   * Introduced in version 2.6
   */
  int sfs_utimens (const char * path, const struct timespec tv[2]){
    inode * node_modify;
    int inode_num;
    inode_num = get_inode(path);
    if(inode_num > -1){
      node_modify = &in_table[inode_num];
      node_modify->access_time = tv[0];
      node_modify->modify_time = tv[1];
    }
    else{
      return -1;
    }
    return 0;
  }


/** Release directory
 *
 * Introduced in version 2.3
 */
int sfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;

    
    return retstat;
}

struct fuse_operations sfs_oper = {
  .init = sfs_init,
  .destroy = sfs_destroy,
  .utimens = sfs_utimens,
  .getattr = sfs_getattr,
  .create = sfs_create,
  .unlink = sfs_unlink,
  .open = sfs_open,
  .release = sfs_release,
  .read = sfs_read,
  .write = sfs_write,

  .rmdir = sfs_rmdir,
  .mkdir = sfs_mkdir,

  .opendir = sfs_opendir,
  .readdir = sfs_readdir,
  .releasedir = sfs_releasedir,
};

void sfs_usage()
{
    fprintf(stderr, "usage:  sfs [FUSE and mount options] diskFile mountPoint\n");
    abort();
}

int main(int argc, char *argv[])
{
    int fuse_stat;
    struct sfs_state *sfs_data;
    
    // sanity checking on the command line
    if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
	sfs_usage();

    sfs_data = malloc(sizeof(struct sfs_state));
    if (sfs_data == NULL) {
	perror("main calloc");
	abort();
    }

    // Pull the diskfile and save it in internal data
    sfs_data->diskfile = argv[argc-2];
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;
    
    sfs_data->logfile = log_open();
    
    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main, %s \n", sfs_data->diskfile);
    fuse_stat = fuse_main(argc, argv, &sfs_oper, sfs_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
    return fuse_stat;
}
