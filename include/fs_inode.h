#include "fs_block.h"

/*
This structure represent the 12 disk pointers
0-9 pointers directly point to the disk blocks
10 is an 1 level indirection with the 1st level indirection pointing to 256 disk pointer
11 is an 2 level indirection
*/

#define DIRECT_BLOCK_COMPLETE 0x001
#define SINGLE_INDIRECT_BLOCK_COMPLETE 0x010
#define DOUBLE_INDIRECT_BLOCK_COMPLETE 0x100

typedef struct fs_disk_map
{
	struct fs_block *blocks[12];
	uint8_t disk_map_flag;

	uint8_t direct_pointer_ind;
	uint16_t single_indirect_pointer_ind;
	uint16_t double_indirect_pointer_ind;
}fs_disk_map_t;


/*typedef struct fs_inode_ops
{
	int (*fs_read)(struct fs_inode *inode, off_t offset, void *buf, size_t size);
	
	int (*fs_write)(struct fs_inode *inode, off_t offset, const void *buf, size_t size);
	
	int(*fs_mkdir)(struct fs_inode *inode, const char *name);
	
	int(*fs_rmdir)(struct fs_inode *inode, const char *name);
	
}fs_inode_ops_t;
*/

typedef struct fs_inode
{
	int device;
	int inode_num; //inode number
	int mode; //Mode in which file is opened
	int ref_count; //File refcount
	int file_size; //File size
	int file_offset;
		
	struct list_head fs_vfs_inode_list;
	
	struct fs_disk_map * disk_map;
	
	struct mutex inode_mutex;
}fs_inode_t;


int alloc_inode(struct fs_vfs * fs_vfs);
int allocate_inodes(struct fs_vfs * fs_vfs);

int alloc_disk_to_inode(struct fs_vfs * fs_vfs, struct fs_inode * inode);



