#include <linux/list.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/mutex.h>

#include "../error.h"
#include "../config.h"

#define FS_DISK_BLOCK_SUPER_BLOCK 1
#define FS_DISK_BLOCK_FREE_LIST 2

struct fs_superblock;

typedef struct fs_vfs
{
	struct fs_superblock * super_block;
	
	struct list_head super_block_disk_list;
	
	struct list_head free_disk_block_list;
	int num_free_disk_blocks;
	
	struct list_head allocated_disk_block_list;
	
	struct list_head free_inode_list;
	int num_free_inodes;
	
	struct list_head allocated_inode_list;
	
	struct mutex vfs_lock;
}fs_vfs_t;

void intialise_file_system(struct fs_vfs * fs_vfs);

