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
	
	for(int i = 0; i < 12; i++)
	{
		inode->disk_map->blocks[i] = NULL;
	}
	
	list_add_tail(&inode->fs_vfs_inode_list, &fs_vfs->free_inode_list);
	inode->disk_map->direct_pointer_ind = 0;
	inode->disk_map->single_indirect_pointer_ind = 0;
	inode->disk_map->double_indirect_pointer_ind = 0;
	
	
	return 0;
}

int allocate_inodes(struct fs_vfs * fs_vfs)
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


int alloc_disk_to_inode(struct fs_vfs * fs_vfs, struct fs_inode * inode)
{
	mutex_lock(&inode->inode_mutex);
	if(!(inode->disk_map->disk_map_flag && 0x001))
	{
		struct fs_block * block = get_free_block(fs_vfs);
		if(!block)
		{
			mutex_unlock(&inode->inode_mutex);
			return -FS_ENO_FREE_BLOCK;
		}
		inode->disk_map->blocks[inode->disk_map->direct_pointer_ind] = block;
		inode->disk_map->direct_pointer_ind += 1;
		if(inode->disk_map->direct_pointer_ind == 9)
		{
			inode->disk_map->disk_map_flag = inode->disk_map->disk_map_flag && 0xff1;
			inode->disk_map->direct_pointer_ind = 0;
		}
	}
	else if(!(inode->disk_map->disk_map_flag && 0x010))
	{
		struct fs_block * first_indirection_block;
		if(inode->disk_map->blocks[10] == NULL)
		{
			first_indirection_block = get_free_block(fs_vfs);
			if(!first_indirection_block)
			{
				mutex_unlock(&inode->inode_mutex);
				return -FS_ENO_FREE_BLOCK;
			}
			inode->disk_map->blocks[10] = first_indirection_block;
			inode->disk_map->single_indirect_pointer_ind = 0;
		}
		else
		{
			first_indirection_block = inode->disk_map->blocks[10];
		}
		
		struct fs_block * block = get_free_block(fs_vfs);
		if(!disk_block)
		{
			mutex_unlock(&inode->inode_mutex);
			return -FS_ENO_FREE_BLOCK;
		}
		
		uintptr_t * addr = kmalloc(sizeof(uintptr_t), GFP_KERNEL);
		(*addr) = block->block_addr;
		int ret = write_to_block(first_indirection_block, (sizeof(uintptr_t)*inode->disk_map->single_indirect_pointer_ind), (void *)addr, sizeof(uintptr_t));
		kfree(addr);
		
		if(ret)
		{
			mutex_unlock(&inode->inode_mutex);
			ret;
		}
		
		inode->disk_map->single_indirect_pointer_ind += 1;
		if(inode->disk_map->single_indirect_pointer_ind == 1024)
		{
			inode->disk_map->single_indirect_pointer_ind = 0;
			inode->disk_map->disk_map_flag = inode->disk_map->disk_map_flag && 0xf1f;
		}
	}
	else if(!(inode->disk_map->disk_map_flag && 0x100))
	{
		struct fs_block * first_indirection_block;
		if(inode->disk_map->blocks[11] == NULL)
		{
			first_indirection_block = get_free_block(fs_vfs);
			if(!first_indirection_block)
			{
				mutex_unlock(&inode->inode_mutex);
				return -FS_ENO_FREE_BLOCK;
			}
			inode->disk_map->blocks[11] = first_indirection_block;
			inode->disk_map->single_indirect_pointer_ind = 0;
			inode->disk_map->double_indirect_pointer_ind = 0;
		}
		else
		{
			first_indirection_block = inode->disk_map->blocks[10];
		}
		
		uintptr_t * second_indirection_block_addr = (uintptr_t)((void *)first_indirection_block->block_addr + sizeof(uintptr_t)*inode->disk_map->single_indirect_pointer_ind);
		
		struct fs_block * second_indirection_block = mem_to_disk_block(fs_vfs, (*second_indirection_block_addr));
		
		struct fs_block * block = get_free_block(fs_vfs);
		if(!block)
		{
			mutex_unlock(&inode->inode_mutex);
			return -FS_ENO_FREE_BLOCK;
		}
		
		uintptr_t * addr = kmalloc(sizeof(uintptr_t), GFP_KERNEL);
		(*addr) = block->block_addr;
		int ret = write_to_block(second_indirection_block, (sizeof(uintptr_t)*inode->disk_map->double_indirect_pointer_ind), (void *)addr, sizeof(uintptr_t));
		
		kfree(addr);
		
		if(ret)
		{
			mutex_unlock(&inode->inode_mutex);
			ret;
		}
		
		inode->disk_map->double_indirect_pointer_ind += 1;
		if(inode->disk_map->double_indirect_pointer_ind == 1024)
		{
			inode->disk_map->single_indirect_pointer_ind += 1;
			if(inode->disk_map->single_indirect_pointer_ind == 1024)
			{
				inode->disk_map->single_indirect_pointer_ind = 0;
				inode->disk_map->double_indirect_pointer_ind = 0;
				inode->disk_map->disk_map_flag = inode->disk_map->disk_map_flag && 0x1ff;
			}
			else
			{
				struct fs_block new_second_indirection_block = get_free_block(fs_vfs);
				if(!new_second_indirection_block)
				{
					mutex_unlock(&inode->inode_mutex);
					return -FS_ENO_FREE_BLOCK;
				}
				uintptr_t * addr = kmalloc(sizeof(uintptr_t), GFP_KERNEL);
				(*addr) = new_second_indirection_block->block_addr;
				int ret = write_to_block(first_indirection_block, (sizeof(uintptr_t)*inode->disk_map->single_indirect_pointer_ind), (void *)addr, sizeof(uintptr_t));
				
				kfree(addr);
				
				if(ret)
				{
					mutex_unlock(&inode->inode_mutex);
					inode->disk_map->double_indirect_pointer_ind = 0;
					ret;
				}
			}
		}
		
	}
	else
	{
		mutex_unlock(&inode->inode_mutex);
		return -
	}
	
	mutex_unlock(&inode->inode_mutex);
	return 0;
}



