# Fuze

Our filesystem implements directories with two layers of indirection.
opendir, closedir, mkdir, and rmdir are all implemented.

We organize every file with an inode structure, using a direct array and two levels of indirection to point to data blocks. 
Directories are just files with a mode bit set to indication directory status, and store directory entry structs in the data blocks.
Data blocks and inode numbers are each mapped to a char array, 0 for free, 1 for in use.
We map inode numbers to actual inode structs with a simple array, stored in consecutive data blocks in reserved data block space.


We organize our disc as follows: 
The first block is reserved for the superblock.
We then allocate one block each for the inode bitmap and block bitmap
Then, we store our actual inode structs on the disc, using a defined calculation of how many blocks the inodes would occupy
From there, our following blocks store files. We put root at the first of these blocks.
We define inodes and directory entry structs ourselves, and manage them within the code.
Whenever a file is created, a new inode is assigned from the inode bitmap, which defines which inode numbers are currently in use.
Whenever we request a new inode number or data block, we set the bitmap accordingly.

Directories are designed as normal files, with direntry structs stored in each block, instead of data.

An inode contains, among other things, an array of block numbers, 
a block number for single indirection, and a block number for double indirection.
The way we implement blocks is, we simply store the block number and block_read to access data. 
The single indirection block number points to a block of data that contains 128 blocks numbers, 
  each pointing to a block with  16 directory entries.
The double indirection block number points to a block of data that contains 128 block numbers, 
  each pointing to a block with 128 block numbers,
    each pointing to a block with 16 directory entries.
    
This way, we can store large files, and our directorys' max size is also very large.
However, each directory access becomes much more inefficient, 
  since measures against fragmentation and runtime optimization are not implemented
Searching through indirect blocks also takes up resources since it is a linear operation, 
and could potentially lead to geological run times in edge cases.

We have helper functions to manage the internal data of our file system
get_inode takes a path and returns the inode number of the path if it's found.
If it isn't found, it'll return a negative number.




Issues
-----------
If a file is created, on a remount of the directory, the file system cannot handle it.

No synchronization; changes to the inode table are not protected

Linear search times massively slow down performance.

Fuse does not call sfs_create after failing to find the inode in getattr. 
We do not want to manually call sfs_create, since we would be creating files whenever we getattr a nonexistant file
That is not intended. 
  
Init and readdir works.
