#include "../include/fs_inode.h"

int alloc_inode(struct fs_vfs * fs_vfs)
{
	struct fs_inode * inode = kmalloc(sizeof(struct fs_inode), GFP_KERNEL);
	
	if(!inode)
	{
		printk(KERN_ERR "FILE_SYSTEM_ERROR : Error allocating inode %d\n", fs_vfs->num_free_inodes);
		return -FS_EMALLOC;
	}
	
	inode->inode_num = fs_vfs->num_free_inodes;
	inode->ref_count = 0;
	inode->file_size = 0;
	inode->file_offset = 0;
	
	mutex_init(&inode->inode_mutex);
	
	fs_vfs->num_free_inodes += 1;
	
	inode->disk_map = kmalloc(sizeof(struct fs_disk_map), GFP_KERNEL);
	
	inode->disk_map->disk_map_flag = 0x0;
	
	for(int i = 0; i < 10; i++)
	{
		inode->disk_map->blocks[i] = NULL;
	}
	
	inode->disk_map->direct_pointer_ind = 0;
	inode->disk_map->single_indirect = NULL;
	inode->disk_map->double_indirect = NULL;
	
	list_add_tail(&inode->fs_vfs_inode_list, &fs_vfs->free_inode_list);
	
	return 0;
}

void destroy_inode(struct fs_inode * inode)
{
	list_del(&inode->fs_vfs_inode_list);
	kfree(inode->disk_map);
	kfree(inode);
}

int intialise_inodes(struct fs_vfs * fs_vfs)
{
	int ret;
	printk("FILE_SYSTEM : Initialising inodes\n");
	
	for(int i = 0; i < FS_NUM_INODES; i++)
	{
		ret = alloc_inode(fs_vfs);
		if(ret)
			return ret;
	}
	printk("FILE_SYSTEM : Initialised %d inodes\n", fs_vfs->num_free_inodes);
	return 0;
}


struct fs_inode * get_inode(struct fs_vfs * fs_vfs)
{
	mutex_lock(&fs_vfs->vfs_lock);
	if(fs_vfs->num_free_inodes == 0)
	{
		mutex_unlock(&fs_vfs->vfs_lock);
		printk(KERN_ERR "FILE_SYSTEM_ERROR : Zero inodes in the system\n");
		return NULL;
	}
	
	struct fs_inode * inode = list_first_entry(&fs_vfs->free_inode_list, struct fs_inode, fs_vfs_inode_list);
	
	list_del(&inode->fs_vfs_inode_list);
	list_add(&inode->fs_vfs_inode_list, &fs_vfs->allocated_inode_list);
	fs_vfs->num_free_inodes -= 1;
	
	mutex_unlock(&fs_vfs->vfs_lock);
	return inode;
}

void put_inode(struct fs_vfs * fs_vfs, struct fs_inode * inode)
{
	mutex_lock(&fs_vfs->vfs_lock);
	list_del(&inode->fs_vfs_inode_list);
	list_add(&inode->fs_vfs_inode_list, &fs_vfs->free_inode_list);
	fs_vfs->num_free_inodes += 1;
	mutex_unlock(&fs_vfs->vfs_lock);
}



struct fs_single_indirect_block * allocate_fs_single_indirect_block(void)
{
	struct fs_single_indirect_block * single_indirect = kmalloc(sizeof(struct fs_single_indirect_block), GFP_KERNEL);
	if(!single_indirect)
		return NULL;
	
	single_indirect->pointer_ind = 0;
	
	for(int i = 0; i < 256; i++)
	{
		single_indirect->blocks[i] = NULL;
	}
	
	return single_indirect;
}

inline void destroy_fs_single_indirect_block(struct fs_single_indirect_block * single_indirect)
{
	if(single_indirect)
		kfree(single_indirect);
	else
		printk(KERN_ERR"FILE_SYSTEM_ERROR : destroy_fs_single_indirect_block passed NULL");
}


struct fs_double_indirect_block * allocate_fs_double_indirect_block(void)
{
	struct fs_double_indirect_block * double_indirect = kmalloc(sizeof(struct fs_double_indirect_block), GFP_KERNEL);
	if(!double_indirect)
		return NULL;
	
	double_indirect->pointer_ind = 0;
	
	for(int i = 0; i < 256; i++)
	{
		double_indirect->blocks[i] = NULL;
	}
	
	return double_indirect;
}

inline void destroy_fs_double_indirect_block(struct fs_double_indirect_block * double_indirect)
{
	if(double_indirect)
		kfree(double_indirect);
	else
		printk(KERN_ERR"FILE_SYSTEM_ERROR : destroy_fs_single_indirect_block passed NULL");
}



int alloc_disk_to_inode(struct fs_vfs *fs_vfs, struct fs_inode *inode)
{
	mutex_lock(&inode->inode_mutex);
	
	struct fs_disk_map * disk_map = inode->disk_map;
	
	if( !(disk_map->disk_map_flag & 0x01) )
	{
		struct fs_block * block = get_free_block(fs_vfs);
		if(!block)
		{
			mutex_unlock(&inode->inode_mutex);
			return -FS_ENO_FREE_BLOCK;
		}
		
		disk_map->blocks[inode->disk_map->direct_pointer_ind] = block;
		disk_map->direct_pointer_ind += 1;
		if(disk_map->direct_pointer_ind == 10)
		{
			disk_map->disk_map_flag = disk_map->disk_map_flag | 0x01;
		}
	}
	else if( !(disk_map->disk_map_flag & 0x02) )
	{
		if(!disk_map->single_indirect)
		{
			disk_map->single_indirect = allocate_fs_single_indirect_block();
			if(!disk_map->single_indirect)
			{
				mutex_unlock(&inode->inode_mutex);
				return -FS_EMALLOC;
			}
		}
		
		struct fs_block * block = get_free_block(fs_vfs);
		if(!block)
		{
			mutex_unlock(&inode->inode_mutex);
			return -FS_ENO_FREE_BLOCK;
		}
		
		disk_map->single_indirect->blocks[disk_map->single_indirect->pointer_ind] = block;
		disk_map->single_indirect->pointer_ind += 1;
		if(disk_map->single_indirect->pointer_ind == 256)
		{
			disk_map->disk_map_flag = disk_map->disk_map_flag | 0x02;
		}
	}
	else if( !(disk_map->disk_map_flag & 0x04) )
	{
		if(!disk_map->double_indirect)
		{
			disk_map->double_indirect = allocate_fs_double_indirect_block();
			if(!disk_map->double_indirect)
			{
				mutex_unlock(&inode->inode_mutex);
				return -FS_EMALLOC;
			}
		}
		
		struct fs_double_indirect_block * double_indirect = disk_map->double_indirect;
		
		if(!double_indirect->blocks[double_indirect->pointer_ind])
		{
			double_indirect->blocks[double_indirect->pointer_ind] = allocate_fs_single_indirect_block();
			
			if(!double_indirect->blocks[double_indirect->pointer_ind])
			{
				mutex_unlock(&inode->inode_mutex);
				return -FS_EMALLOC;
			}
		}
		
		if(double_indirect->blocks[double_indirect->pointer_ind]->pointer_ind == 256)
		{
			double_indirect->pointer_ind += 1;
			
			if(double_indirect->pointer_ind == 256)
			{
				disk_map->disk_map_flag = disk_map->disk_map_flag | 0x04;
				mutex_unlock(&inode->inode_mutex);
				printk(KERN_ERR "FILE_SYSTEM_ERROR : Cannot allocate more memory to the inode. MAX LIMIT reached\n");
				return -FS_E_MAX_LIMIT;
			}
			
			if(!double_indirect->blocks[double_indirect->pointer_ind])
			{
				double_indirect->blocks[double_indirect->pointer_ind] = allocate_fs_single_indirect_block();
				
				if(!double_indirect->blocks[double_indirect->pointer_ind])
				{
					mutex_unlock(&inode->inode_mutex);
					return -FS_EMALLOC;
				}
			}
		}
		
		struct fs_single_indirect_block * single_indirect = double_indirect->blocks[double_indirect->pointer_ind];
		
		struct fs_block * block = get_free_block(fs_vfs);
		if(!block)
		{
			mutex_unlock(&inode->inode_mutex);
			return -FS_ENO_FREE_BLOCK;
		}
		
		single_indirect->blocks[single_indirect->pointer_ind] = block;
		single_indirect->pointer_ind += 1;
	}
	else
	{
		mutex_unlock(&inode->inode_mutex);
		printk(KERN_ERR "FILE_SYSTEM_ERROR : Cannot allocate more memory to the inode. MAX LIMIT reached\n");
		return -FS_E_MAX_LIMIT;
	}
	
	mutex_unlock(&inode->inode_mutex);
	
	return 0;
}

void destroy_inode_disk_map_single_indirect(struct fs_vfs * fs_vfs, struct fs_single_indirect_block * single_indirect)
{
	while(single_indirect->pointer_ind >= 0)
	{
		put_free_block(fs_vfs, single_indirect->blocks[single_indirect->pointer_ind]);
		single_indirect->blocks[single_indirect->pointer_ind] = NULL;
		single_indirect->pointer_ind -= 1;
		
	}
	destroy_fs_single_indirect_block(single_indirect);
}

void trim_inode_disk_map(struct fs_vfs * fs_vfs, struct fs_inode * inode)
{
	mutex_lock(&inode->inode_mutex);
	
	struct fs_disk_map * disk_map = inode->disk_map;
	
	if(disk_map->double_indirect)
	{
		struct fs_double_indirect_block * double_indirect = disk_map->double_indirect;
		if(double_indirect->pointer_ind == 256)
		{
			double_indirect->pointer_ind -= 1;
		}
		
		struct fs_single_indirect_block * single_indirect = double_indirect->blocks[double_indirect->pointer_ind];
		single_indirect->pointer_ind -= 1;
		if(single_indirect->pointer_ind < 0)
		{
			destroy_fs_single_indirect_block(single_indirect);
			double_indirect->blocks[double_indirect->pointer_ind] = NULL;
			double_indirect->pointer_ind -= 1;
			if(double_indirect->pointer_ind < 0)
			{
				destroy_fs_double_indirect_block(double_indirect);
				disk_map->double_indirect = NULL;
			}
			single_indirect = double_indirect->blocks[double_indirect->pointer_ind];
			single_indirect->pointer_ind = 255;
		}
		
		while(disk_map->double_indirect)
		{
			struct fs_block * block = single_indirect->blocks[single_indirect->pointer_ind];
			put_free_block(fs_vfs, block);
			single_indirect->blocks[single_indirect->pointer_ind] = NULL;
			single_indirect->pointer_ind -= 1;
			
			if(single_indirect->pointer_ind < 0)
			{
				destroy_fs_single_indirect_block(single_indirect);
				double_indirect->blocks[double_indirect->pointer_ind] = NULL;
				double_indirect->pointer_ind -= 1;
				if(double_indirect->pointer_ind < 0)
				{
					destroy_fs_double_indirect_block(double_indirect);
					disk_map->double_indirect = NULL;
					break;
				}
				single_indirect = double_indirect->blocks[double_indirect->pointer_ind];
				single_indirect->pointer_ind = 255;
			}
		}
	}
	
	if(disk_map->single_indirect)
	{
		struct fs_single_indirect_block * single_indirect = disk_map->single_indirect;
		
		single_indirect->pointer_ind -= 1;
		//printk("FILE_SYSTEM : single_indirect->pointer_ind:%d\n", single_indirect->pointer_ind);
		destroy_inode_disk_map_single_indirect(fs_vfs, single_indirect);
		disk_map->single_indirect = NULL;
	}
	
	
	disk_map->direct_pointer_ind -= 1;
	
	//printk("FILE_SYSTEM : inode->disk_map->direct_pointer_ind:%d\n", inode->disk_map->direct_pointer_ind);
	while(disk_map->direct_pointer_ind >= 0)
	{
		put_free_block(fs_vfs, disk_map->blocks[disk_map->direct_pointer_ind]);
		disk_map->blocks[disk_map->direct_pointer_ind] = NULL;
		disk_map->direct_pointer_ind -= 1;
	}
	
	inode->disk_map->disk_map_flag = 0x0;
	disk_map->direct_pointer_ind = 0;
	
	mutex_unlock(&inode->inode_mutex);
}

/*
void remove_last_disk_block_from_inode(struct fs_vfs * fs_vfs, struct fs_inode * inode)
{
	if(inode->disk_map->disk_map_flag == 0x0)
		return;
	
	mutex_lock(&inode->inode_mutex);
	
	struct fs_disk_map * disk_map = inode->disk_map;
	
	if(disk_map->double_indirect)
	{
		struct fs_double_indirect_block * double_indirect = disk_map->double_indirect;
		if(double_indirect->pointer_ind == 256)
		{
			double_indirect->pointer_ind -= 1;
		}
		
		struct fs_single_indirect_block * single_indirect = double_indirect->blocks[double_indirect->pointer_ind];
		single_indirect->pointer_ind -= 1;
		
		put_free_block(fs_vfs, single_indirect->blocks[single_indirect->pointer_ind]);
		single_indirect->blocks[single_indirect->pointer_ind] = NULL;
		
		if(single_indirect->pointer_ind == 0)
		{
			destroy_fs_single_indirect_block(single_indirect);
			double_indirect->blocks[double_indirect->pointer_ind] = NULL;
			double_indirect->pointer_ind -= 1;
			if(double_indirect->pointer_ind < 0)
			{
				destroy_fs_double_indirect_block(double_indirect);
				disk_map->double_indirect = NULL;
				disk_map->disk_map_flag = disk_map->disk_map_flag & 0xf
			}
		}
	}
	
	mutex_unlock(&inode->inode_mutex);
}
*/

