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
 #define MAX_BLOCKS 128
 #define InodeStartAddr 1 //Need to Set where the inodes start in our data blocks
 #define MAX_NODES_PER_BLOCK ((BLOCK_SIZE)/sizeof(struct inode)) 
 #define MAX_NODES MAX_SIZE/MAX_NODES_PER_BLOCK

char data_bitmap[MAX_BLOCKS]; 
char inode_bitmap[MAX_SIZE];
inode in_table[MAX_NODES];


direntry *init_direntry(inode *n, char *name){
  direntry *new_dir = malloc(sizeof(direntry));
  strncpy(new_dir->name,name,sizeof(new_dir->name));
  new_dir->node = n;
  return new_dir
}
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


//Superblock Operations

//The most important item in the superblock object is s_op, which is the superblock operations table. The superblock operations table is represented by struct super_operations and is defined in <linux/fs.h>. It looks like this:

//TOny: We may not need any or some of these.

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

int get_inode(struct superblock superblock, char *path){//Returns the inode_number of an inode
	char buffer[PATH_MAX];
	int num;
	int cur_inode_number = 2; //Start at root!
	char *patho = path;
	
	while((num = parse_path(patho, buffer)) > -1){//Path ends in a /. 
	//That is, we found 'something/', or just '/'. if we find 'something', that's considered nothing and we quit.
	
		while(!strncmp((superblock.global_table[cur_inode_number].path), buffer, PATH_MAX)){//return 0 means match find
			if(cur_inode_number = (superblock.global_table[cur_inode_number].next) < 0){
				log_msg("File does not exist. Path %s", path);
				return -1;
				//Uhoh. Got to the directory, but file no found. 
				//We hit the end of the LL for the Directory without matching a file name.
			}
		}
		//Current parsed path part found. 
		//Is there more to parse? 
		if(parse_path(patho+num+1, buffer) > -1){
			if(cur_inode_number = superblock.global_table[cur_inode_number].child == NULL){
				//There is more to parse, but no child exists? Problem.
				log_msg("File does not exist. Path: %s", path);
				return -1;
			}
			
		}
		//In the else case, the while loop will just exit and we'll return the right value.
		patho = patho+num+1;
		//Increment by the number we read, plus the / at the end of each file or path. 
	}
	return cur_inode_number;
	
	
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
    
    char* buf = BLOCK_SIZE;
    if(!(block_read(0,buf)>0){
      spb superblk;
      superblk.total_inodes = MAX_NODES;
      superblk.total_datablocks = MAX_BLOCKS;
      char temp[BLOCK_SIZE];
      memset(temp,0,BLOCK_SIZE);
      memcpy(temp,&superblk,sizeof(superblk));

      if(block_write(0,&temp) >0){
        log_msg("\n SUPERBLOCK initialized");
      }

      memset(data_bitmap,0,sizeof(data_bitmap));
      int n = MAX_NODES + 4;
      for(int i = 0; i < n;i++){
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
      int gid = getegid();       
      inode *root = malloc(sizeof(inode));
      root->inode_number = 0;
      root->size = 0;
      root->uid = uid;
      root->gid = gid;
      root->inodetype = IDIR;
      root->singleindirect =0;
      root->doubleindirect =0;
      //Initialize root dirent
      char ent1[] = ".";
      char ent2[] = "..";
      dirent *tmp_dirent = malloc(sizeof(dirent));
      direntry *tmp_ent1 = init_direntry(root,ent1);
      direntry *tmp_ent2 = init_direntry(root,ent2);
      tmp_dirent->entries[0] = tmp_ent1;
      tmp_dirent->entries[1] = tmp_ent2;
      n = n - 1;
      if(block_write(n, tmp_dirent)>0){
        log_msg("\n Root Directory Initialized");
      }
      memset(in_table,0,sizeof(in_table));
      in_table[0] = root;
      int b;
      char* buf = malloc(BLOCK_SIZE);
      memset(buf, 0, BLOCK_SIZE);
      for(b = 3; b<MAX_NODES+3; b++){
        /*REALLY DANGEROUS AND COULD GET OUT OF BOUNDS PLEASE FIX*/
          inode *tmp = in_table+(sizeof(BLOCK_SIZE)*(b-2)); //This can get out of bounds.
          memcpy(buf, tmp, sizeof(buf));
          //Nothin left to write
          if(block_write(b,&buf) == 0){
            break;
      }

      //inode_numbers explained: This is just a number to uniquely identify our inodes. 2 is always root.
      // The inode_number is the index of the inode in the global static inode array.
     //Init superblock here
    /*	int total_inodes;
	   int total_datablocks;
	struct inodes_table global_table; //EWWW someone help me change this
	struct super_operations s_op;  /* superblock methods */ 
  }

  else{
    spb superblock;
    //SuperBlock is inited
    //Do a bunch of block reads
    //Buffer size should be the size of our inodebitmap/array
    //Read in inode_bitmap/array
    char* buffer = malloc(BLOCK_SIZE);
    memset(buffer,0,BLOCK_SIZE);
    if(block_read(0, buffer)>0){
     memcpy(&superblock,buffer,sizeof(spb));
    }

    //Init data bitmap
    memset(buffer,0,BLOCK_SIZE);
    if(block_read(1, buffer)>0){
       memcpy(data_table,buffer,sizeof(data_bitmap));
    }

    //Init Inode Bitmap
    memset(buffer,0,BLOCK_SIZE);
    if(block_read(2, buffer)>0){
       memcpy(inode_bitmap,buffer,sizeof(inode_bitmap));
    }

    memset(buffer,0,BLOCK_SIZE);
    for (b =3;b<MAX_NODES+3;++b){
      if(block_read(b, buffer)>0){
        inode *tmp = in_table+(sizeof(BLOCK_SIZE) *(b-2));
        memcpy(temp,buffer,sizeof(BLOCK_SIZE));
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
    //disk_close()
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
    cur_inode = get_inode(path);
    
    //statbuf->st_dev = 9001; //How are we supposed to know this?
   // statbuf->st_ino = (ino_t)inode_num;
    statbuf->mode_t = cur_inode.mode; 
    statbuf->st_nlink = cur_inode.links; //Hardlinks not implemented
    statbuf->uid_t = cur_inode.uid;
    statbuf->gid_t = cur_inode.gid;
    //statbuf->st_rdev = 0; //What's a special file device id o_o
    statbuf->st_size = size; //Remember to define all these in inode struct later.
    statbuf->st_blksize = BLOCK_SIZE;
    statbuf->st_blocks = num_blocks; //Remember to define me
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
    strcpy(fpath, SFS_DATA->rootdir);
    strncat(fpath, path, PATH_MAX);

    retstat = stat(fpath,statbuf);
    if (retstat != 0){
       log_msg("ERROR %s",strerror(errno));
    }
    //log_sta

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

    int block = get_free_block();/*Find the first unset block*/ 
    struct inode *new_node = get_inode((char*) path);

    if(new_node != NULL){
      //it is already taken
    }
    else {
      //Get new node location from bitmap/array
      //Populate inode info on the inode table
      //Write the inode to correct location on disk
      //Update bitmap and write to disk
    }


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
    if(size <= BLOCK_SIZE){
      //get inode 
      struct inode *node = //get_inode(path);
      node->size = size;
    }

    /*
    Tony: Similar to read.
    */
    
    return retstat;
}
int get_file_name(char* path, char* buffer){
	int a = 0;
	int b = strnlen(path, PATH_MAX);
	char* patho = path;
	while((a = a + (c=parse_path(patho, buffer)) ) < b ){
		patho = patho + c;
	}
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
{
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
	int retstat = 0;
	//log_msg("\nsfs_mkdir(path=\"%s\", mode=0%3o)\n",path, mode);
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
