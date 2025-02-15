#include "../include/fs_vfs.h"

void intialise_file_system(struct fs_vfs * fs_vfs)
{
	printk("FILE_SYSTEM : Initializing file system\n");
	
	INIT_LIST_HEAD(&fs_vfs->super_block_disk_list);
	INIT_LIST_HEAD(&fs_vfs->free_disk_block_list);
	INIT_LIST_HEAD(&fs_vfs->allocated_disk_block_list);
	
	INIT_LIST_HEAD(&fs_vfs->free_inode_list);
	INIT_LIST_HEAD(&fs_vfs->allocated_inode_list);
	
	mutex_init(&fs_vfs->vfs_lock);
	
	fs_vfs->total_num_disk_blocks = FILE_SYSTEM_SIZE/FS_BLOCK_SIZE;
	fs_vfs->num_free_disk_blocks = FILE_SYSTEM_SIZE/FS_BLOCK_SIZE;
	fs_vfs->num_free_inodes = 0;
}

