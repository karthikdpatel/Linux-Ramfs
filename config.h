
//While changing system size bring it to next 100 diskblock i.e want to set 302 disk blocks set the size to 400*4*1024
#define FILE_SYSTEM_SIZE ((66100)*4*1024)

#define FS_BLOCK_SIZE (4*1024)
#define FS_BYTES_PER_INODE (16*1024)

#define FS_NUM_INODES (FILE_SYSTEM_SIZE)/(FS_BYTES_PER_INODE)

