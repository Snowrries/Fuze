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

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */


/*
Idea is to use statically allocated inodes for now. That means a limited number of 
things to work with.  With a large amount of files we might have problems providing
inodes for all our files.

The main assignment is a direct mapped system; ie: an inode with 12 pointers to data can only store
12*512 bytes per file.

Indirect Blocks:
Once the file grows beyond the direct mapped size the filesystem will grab a data block and
start using it to store additional block pointers.  The address of this block will be stored
in the inode.




*/

//This should go in another file

 #define IFILE 0 //Inode is a file
 #define IDIR 1  //Inode is a directory

 #define BLOCK_SIZE 512
 #define MAX_SIZE 64 //64 is arbitrary
 #define InodeStartAddr 1 //Need to Set where the inodes start in our data blocks
 #define MAX_NODES ((BLOCK_SIZE*MAX_SIZE)/sizeof(struct inode)) 
 
struct indir1{
	struct* inode indirect[BLOCK_SIZE/sizeof(struct *inode)]
	//an array of inode pointers.
}
struct indir2{
	struct* indir1 indirect2[BLOCK_SIZE/sizeof(struct *indir1)]
	//an array of indir 1 pointers. 
}
 
 
 
 
struct inode {
  //Universal to all Inodes
  int inode_number; //root starts with inode #2
  int mode; //can this file be read/written/executed
  struct icommon *on-disk;
  int uid;
  int size;
   /*define IFILE 0 //Inode is a file
   define IDIR 1  //Inode is a directory*/
  int inodetype; 
  int db_addr[13]; //datablock addresses; 13 is used for indirect pointers
  char name[64]; // Max name size is 64 chars. PATH_MAX is a thing.
  struct indir1 indirect1;
  struct indir2 indirect2;
  
  //To read inode number 32 we do 32*sizeof(inode) + indoestartaddr
  //blk = (inumber * sizeof(inode)) / blockSize;
  //sector = ((blk * blockSize) + inodeStartAddr) / sectorSize;
  //Things Specific to directory Inodes
  int parent;
  int child;
  //Things Specfic to file Inodes
  int next;
  //Parent child and next are all uids. Look up in the global table to find the inode corresponding.
};
/*
//Allen: I don't know how to bitmap, so can we just be inefficient and use an array? :^)
struct inodes_bitmap{
	int bitmap[size];
};

struct data_bitmap{
	int bitmap[size];
};*/

//First thing in our memory/disk
//Superblock keeps track of our filesystem info
//Contains metadata on filesystems. 

//Tony:
//Not sure what we need or don't need here
//Unix's superblock is a huge struct full irrelevant stuff 
//when we are only using a single filesystem
//Allen: superblock never initted

//Keeps track of inodes.
struct inodes_table {
	struct inode table[MAX_NODES];
}; 

struct superblock{
	int total_inodes;
	int total_datablocks;
	struct inodes_table global_table; //EWWW someone help me change this
	struct super_operations s_op;  /* superblock methods */
};

//Superblock Operations

//The most important item in the superblock object is s_op, which is the superblock operations table. The superblock operations table is represented by struct super_operations and is defined in <linux/fs.h>. It looks like this:

//TOny: We may not need any or some of these.
struct super_operations {
        struct inode *(*alloc_inode) (struct super_block *sb);
        void (*destroy_inode) (struct inode *);
        void (*read_inode) (struct inode *);
        void (*dirty_inode) (struct inode *);
        void (*write_inode) (struct inode *, int);
        void (*put_inode) (struct inode *);
        void (*drop_inode) (struct inode *);
        void (*delete_inode) (struct inode *);
        void (*put_super) (struct super_block *);
        void (*write_super) (struct super_block *);
        int (*sync_fs) (struct super_block *, int);
        void (*write_super_lockfs) (struct super_block *);
        void (*unlockfs) (struct super_block *);
        int (*statfs) (struct super_block *, struct statfs *);
        int (*remount_fs) (struct super_block *, int *, char *);
        void (*clear_inode) (struct inode *);
        void (*umount_begin) (struct super_block *);
        int (*show_options) (struct seq_file *, struct vfsmount *);
};




//Find Inode
struct inode *get_inode(char *path){
	int i;
	log_msg("\n I am converting my path to an inode");
	for(i = 0; i<MAX_NODES; i++){
		if(strcmp((char*)&global_table[i].path,path) = 0){
			return &inds_table.table[i];
		}
	}
	return NULL;
}
//Find Inode part 2

int get_inode_kai(struct superblock superblock, char *path){//Returns the uid of an inode
	char buffer[PATH_MAX];
	int num;
	int cur_uid = 0; //Start at root!
	char *patho = path;
	
	while((num = parse_path(patho, buffer)) > -1){//Path ends in a /. 
	//That is, we found 'something/', or just '/'. if we find 'something', that's considered nothing and we quit.
	
		while(!strncmp((superblock.global_table[cur_uid].path), buffer, PATH_MAX)){//return 0 means match find
			if(cur_uid = (superblock.global_table[cur_uid].next) < 0){
				log_msg("File does not exist. Path %s", path);
				return NULL;
				//Uhoh. Got to the directory, but file no found. 
				//We hit the end of the LL for the Directory without matching a file name.
			}
		}
		//Current parsed path part found. 
		//Is there more to parse? 
		if(parse_path(patho+num+1, buffer) > -1){
			if(superblock.global_table[cur_uid].child == NULL){
				//There is more to parse, but no child exists? Problem.
				log_msg("File does not exist. Path: %s", path);
				return NULL;
			}
		}
		//In the else case, the while loop will just exit and we'll return the right value.
		patho = patho+num+1;
		//Increment by the number we read, plus the / at the end of each file or path. 
	}
	return cur_uid
	
	
}

int parse_path(char *path, char* buffer){
	char a;
	char* tpath = path;
	int i = 0 ;
	while( (a = tpath*) != '/'){
		buffer[i] = a;
		tpath++;
		i++
		if(i == PATH_MAX-1){
			return -1;
		}
	}
	buffer[i] = '\0';
	return i;
}


void *sfs_init(struct fuse_conn_info *conn)
{
    fprintf(stderr, "in bb-init\n");
    log_msg("\nsfs_init()\n");
    
    log_conn(conn);
    log_fuse_context(fuse_get_context());
    /* 
    Tony: Assignment Description Says we must store and organize
    all our data into a flat file. 
    */
    disk_open((SFS_DATA)->diskfile);
    
    struct inode root;
    root.uid = 0; 
    root.parent = -1;
    root.child = -1;
    root.next = -1;
    root.path = "";
    //UIDs explained: Not your traditional uid. This is just a number to uniquely identify our inodes. 0 is always root.
    // The uid is the index of the inode in the global static inode array.
    struct superblock spb;
    //Init superblock here
    /*	int total_inodes;
	int total_datablocks;
	struct inodes_table global_table; //EWWW someone help me change this
	struct super_operations s_op;  /* superblock methods */
    spb.total_inodes = MAX_NODES;
    spb.total_datablocks = 9001;
    spb.global_table[0] = root;
    
    
    //Init the inode table
    int i;
    for(i =0; i<MAX_NODES;i++){
    	inodes_table[i].inode_number = i;
    }
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
    //disk_close()
}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int sfs_getattr(const char *path, struct stat *statbuf)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
	  path, statbuf);

    //Skeleton Code
    strcpy(fpath, SFS_DATA->rootdir);
    strncat(fpath, path, PATH_MAX);

    retstat = stat(fpath,statbuf);
    if (retstat != 0){
       log_msg("ERROR %s",strerror(errno));
    }
    //log_stat

    return retstat;
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
    int retstat = 0;
    int fd;
    log_msg("\nsfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n",
	    path, mode, fi);

    //Skeleton Code
    fd = open(path,O_WRONLY|O_CREAT|O_TRUNC, mode);
    if (fd == -1){
      //We should log these errors
      return -errno;
    }
    fi->fh = fd;

    /*
    Tony: Use path to lookup where the file is located in our virtual disk.
    If it cannot be found, we have write a new block to the virtual disk.
    Then we open by returning the data block to a pointer given by the user; in this case
    fi->fh (file handler)
    */
    
    return retstat;
}

/** Remove a file */
int sfs_unlink(const char *path)
{
    int retstat = 0;
    log_msg("sfs_unlink(path=\"%s\")\n", path);
    retstat = unlink(path);
    if (retstat == -1){
      //Log this error 
      return errno;
    }
    
    return retstat;
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
    log_msg("\nsfs_open(path\"%s\", fi=0x%08x)\n",
  	    path, fi);

    //Skeleton Code
    fd = open(path,fi->flags);
    if(fd == -1){
      return -errno;
    }
    fi->fh = fd;

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
    Tony: Similar to read.
    */
    
    return retstat;
}


/** Create a directory */
int sfs_mkdir(const char *path, mode_t mode)
{
    int retstat = 0;
    log_msg("\nsfs_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);
   
    
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
    int retstat = 0;
    
    // Tony: Fuck This Shit.
    
    return retstat;
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
