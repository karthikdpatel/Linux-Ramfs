
//While changing system size add an extra super block while adding 100 diskblocks i.e, want to add 200 disk block then add 2 super blocks which makes the total diskblocks as 202 and total size as (202*4*1024)
#define FILE_SYSTEM_SIZE ((300)*4*1024)

#define FS_BLOCK_SIZE (4*1024)
#define FS_BYTES_PER_INODE (16*1024)

#define FS_NUM_INODES (FILE_SYSTEM_SIZE)/(FS_BYTES_PER_INODE)

