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
 #define InodeStartAddr 1 //Need to Set where the inodes start in our data blocks
 #define MAX_NODES_PER_BLOCK ((BLOCK_SIZE)/sizeof(inode)) 
 #define MAX_NODES 64
 #define INODE_TLB_BLKS (MAX_NODES/MAX_NODES_PER_BLOCK)
 #define MAX_PATH 32
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
	int num;
	int cur_inode_number = 2; //Start at root!
	int running = 0;
	inode curnode;
	int pathlen = strnlen(path, PATH_MAX);
	char patho[pathlen];
  	strncpy(patho, path, pathlen);
	int found = 0 ;
	char buffer[PATH_MAX];
	int result;
	int offset = 0;
	int i;
	int j;
	int block_num;
	char buf[BLOCK_SIZE];
	char buf2[BLOCK_SIZE];
	
	while((num = num + parse_path(&patho[offset], buffer) + 1 ) <= running){
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
int parse_path(char *path, char* buffer){
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
	int numread;
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
      int uid = getuid();    
      inode *root = malloc(sizeof(inode));
      root->inode_number = 0;
      root->size = sizeof(direntry)*2;
      root->uid = uid;
      root->inodetype = IDIR;
      root->mode = 0700;
      root->single_indirect = -1;
      root->double_indirect = -1;
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
      log_msg("\n %d", info);
      
      memset(in_table,0,sizeof(in_table));
      in_table[0] = *root;
      log_msg("\n %d", sizeof(in_table));
      log_msg("\n %d", sizeof(in_table[0]));
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
    //disk_close();
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
    log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
    path, statbuf);
    char fpath[PATH_MAX];
   /* int inode_num;
    struct inode cur_inode;
    if((inode_num = get_inode(path) < 0){
    	log_msg("Could not find path!");
    	return -1;
    }
    */
    inode cur_inode;
    int n = get_inode(path);
    cur_inode = in_table[n];
    
    //statbuf->st_dev = 9001; //How are we supposed to know this?
   // statbuf->st_ino = (ino_t)inode_num;
    statbuf->st_mode = cur_inode.mode; 
    //statbuf->st_nlink = cur_inode->links; //Hardlinks not implemented
    statbuf->st_uid = cur_inode.uid;
    //statbuf->st_gid = cur_inode->gid;
    //statbuf->st_rdev = 0; //What's a special file device id o_o
    statbuf->st_size = cur_inode.size; //Remember to define all these in inode struct later.
    statbuf->st_blocks = cur_inode.num_blocks; //Remember to define me
    //statbuf->st_atime = 0;
    //statbuf->st_mtime = 0;
    //statbuf->st_ctime = 0; 
    
//    struct stat {
//    dev_t     st_dev;     /* ID of device containing file */
//    ino_t     st_ino;     /* inode number */
//    mode_t    st_mode;    /* protection */
//    nlink_t   st_nlink;   /* number of hard links */
//    uid_t     st_uid;     /* user ID of owner */
//    gid_t     st_gid;     /* group ID of owner */
//    dev_t     st_rdev;    /* device ID (if special file) */
//    off_t     st_size;    /* total size, in bytes */
//    blksize_t st_blksize; /* blocksize for file system I/O */
//    blkcnt_t  st_blocks;  /* number of 512B blocks allocated */
//    time_t    st_atime;   /* time of last access */
//    time_t    st_mtime;   /* time of last modification */
//    time_t    st_ctime;   /* time of last status change */
//};
    

    //Skeleton Code
    /*
    strcpy(fpath, SFS_DATA->rootdir);
    strncat(fpath, path, PATH_MAX);

    retstat = stat(fpath,statbuf);
    if (retstat != 0){
       log_msg("ERROR %s",strerror(errno));
    }
    */
    //log_sta

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
    int inode_num;
    int block;
    inode *new_node;
    const char* buf = calloc(1,BLOCK_SIZE);

    if(inode_num > 0){
      return sfs_open(path, fi);
    }
    else if(inode_num == -1){
    	inode_num = get_free_inode();
    	block = get_free_block();/*Find the first unset block*/
    	new_node = malloc(sizeof(inode));
      data_bitmap[block] = 1;
    	inode_bitmap[inode_num] = 1;
    	new_node->inode_number = inode_num;
    	new_node->mode = mode;
    	new_node->size = 0;
    	new_node->num_blocks = 0;
    	new_node->inodetype = IFILE;
    	new_node->direct[0] = block;
    	in_table[inode_num] = *new_node;
    	
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
	int j = 0;
	log_msg("sfs_unlink(path=\"%s\")\n", path);
	int inode_number;
	int check;
	char buffer[PATH_MAX];
	int i = 0;
	char buf[512];
	char buf2[512];
	inode_number = get_inode(path);
  inode dir = in_table[inode_number];
	int indir = dir.single_indirect;
	int indir2 = dir.double_indirect;
	
	if(find_parent(path, buffer)<1){
		log_msg("Could not find parent while unlinking");
		return -1; 
	} 
	inode_bitmap[inode_number] = 0;
	while(check = dir.direct[i] != -1){
		data_bitmap[check] = 0;
		i++;
		if(i > 21){
			break;
		}
	}
	//Check indirects
	if(check != -1){
		block_read(indir, buf);
		for(i = 0; i < 128; i++){
			if(check = buf[i] != -1){
				data_bitmap[check] = 0;
			}
			else{
				break;
			}
		}
		data_bitmap[indir] = 0;
	}
	if(check != -1){
		block_read(indir2, buf);
		for(i = 0; i < 128; i++){
			block_read(buf[i], buf2);
			for(j = 0; j < 128; j++ ){
				if(check = buf2[j] != -1){
					data_bitmap[check] = 0;
				}
				else{
					break;
				}
			}
			data_bitmap[buf[i]] = 0;
		}
		data_bitmap[indir] = 0;
	}
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
int sfs_open(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    int fd;
    int inode_num;
    log_msg("\nsfs_open(path\"%s\", fi=0x%08x)\n",
  	    path, fi);

    //Skeleton Code
    fd = open(path,fi->flags);
    if(fd == -1){
      return -errno;
    }
    fi->fh = fd;

    //inode_num = get_inode(spb, path);
    
    /*
    Tony: Use path to located where the file is located in our virtual drive index.
    */
    return retstat;
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
    
    close(fi->fh);

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
    int retstat = 0;
    log_msg("\nsfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);

    //Skeleton Code
    retstat = pread(fi->fh, buf, size, offset);
    if (retstat == -1){
      retstat = -errno;
    }
    /*Tony: We have to convert file handler to an index and determine location in our
    virtual disk file.
    Flesh code:
    */
    /*
    struct inode cur;
    char* buf = buffer;
    int inode_num = get_inode(path);
    cur = spb.global_inode_table[inode_num];
    curset = offset;
    off = (int)(offset / BLOCK_SIZE);
    int indir1;
    int indir2;
    int temp;
    size_t leftover = size;
    char* blockpointer;
    
    
    
    while(leftover > 0){
	    //WORK IN PROGRESS
	    if(off < 13){
	    	blockpointer = cur.db_addr[off];
	    }
	    else if(off < 157){//Indir1
	    	off = off-12;
	    	indir1 = off/12;
	    	off = off-(indir1*12);
	    	blockpointer = cur.indirect1[indir1][off];
	    	
	    }
	    else if(off < 1885) {//This is 12^3 +157
	    //Indir2
	    	off = off - 156; //These blocks are taken care of by indir 1 and the direct map
	    	indir2 = off/144;
	    	temp = off-(indir2*144);
	    	indir1 = temp/12;
	    	off = off-(indir1*12);
	    	blockpointer = cur.indirect2[indir2][indir1][off];
	    	
	    }
	    memcpy(buffer, blockpointer, BLOCK_SIZE);
	    buffer = buffer+BLOCK_SIZE;
	    leftover = curset - BLOCK_SIZE;
	    off++;
    }
  */
    return retstat;
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
    int retstat = 0;
    log_msg("\nsfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);
    retstat = pwrite(fi->fh, buf, size, offset);
    if (retstat == -1){
      retstat = -errno;
    }
    /*
    if(size <= BLOCK_SIZE){
      //get inode 
      struct inode *node = //get_inode(path);
      node->size = size;
    }
*/
    /*
    Tony: Similar to read.
    */
    
    return retstat;
}
//Returns length of file name
//Places the file name into the buffer;
/*
int get_file_name(char* path, char* buffer){
	int a = 0;
	int b = strnlen(path, PATH_MAX);
	int c = 0;
	char* patho = path;
	while( a < b ){
		a = a + c + 1;
		c = parse_path(&patho[a], buffer);
	}
	//strncpy(buffer,path[a],c+1);
	return c;
	
}
int get_parent_path( char* path, char* name; char* buffer){
	int a;
	int b;
	a = strlen(name);
	b = strlen(path);
	buffer = strncpy(buffer, name, (b-a));
	buffer[b-a] = '\0';
	return 0;
	
}
int add_to_dirtree(const char* path, struct dirent entry){
	char* pathy = get_parent_path(path);
	get_inode(path);
	
	return 0;
}

/** Create a directory */
int sfs_mkdir(const char *path, mode_t mode)
{ /*
	int a;
	a = get_inode(spb,path); //spb = superblock
	if( a>0 && spb.global_table[a].inodetype == IDIR){
		return -1; //Error! 
	}
	struct dirent entry;
	get_file_name(path,entry.d_name);
	entry.d_type = DT_UNKNOWN;
	if(add_to_dirtree(path, entry)<0){
		return -1;
	}
  */
	int retstat = 0;
	log_msg("\nsfs_mkdir(path=\"%s\", mode=0%3o)\n",path, mode);
  
	return retstat;
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
 */
int sfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{ 
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
  .releasedir = sfs_releasedir
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
