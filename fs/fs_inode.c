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
	inode->disk_map->disk_map_flag = 0x0;
	inode->disk_map->direct_pointer_ind = 0;
	inode->disk_map->single_indirect_pointer_ind = 0;
	inode->disk_map->double_indirect_pointer_ind = 0;
	
	return 0;
}

void destroy_inode(struct fs_inode * inode)
{
	list_del(&inode->fs_vfs_inode_list);
	kfree(inode->disk_map);
	kfree(inode);
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
	if(!(inode->disk_map->disk_map_flag & 0x01))
	{
		struct fs_block * block = get_free_block(fs_vfs);
		if(!block)
		{
			mutex_unlock(&inode->inode_mutex);
			return -FS_ENO_FREE_BLOCK;
		}
		inode->disk_map->blocks[inode->disk_map->direct_pointer_ind] = block;
		inode->disk_map->direct_pointer_ind += 1;
		if(inode->disk_map->direct_pointer_ind == 10)
		{
			inode->disk_map->disk_map_flag = inode->disk_map->disk_map_flag | 0x01;
			inode->disk_map->direct_pointer_ind = 0;
		}
	}
	else if(!(inode->disk_map->disk_map_flag & 0x02))
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
		if(!block)
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
			return ret;
		}
		
		inode->disk_map->single_indirect_pointer_ind += 1;
		if(inode->disk_map->single_indirect_pointer_ind == 256)
		{
			inode->disk_map->single_indirect_pointer_ind = 0;
			inode->disk_map->disk_map_flag = inode->disk_map->disk_map_flag | 0x02;
		}
	}
	else if(!(inode->disk_map->disk_map_flag & 0x04))
	{
		printk(KERN_DEBUG "FILE_SYSTEM_DEBUG : Second level\n");
		struct fs_block * first_indirection_block, * second_indirection_block;
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
			
			second_indirection_block = get_free_block(fs_vfs);
			if(!second_indirection_block)
			{
				mutex_unlock(&inode->inode_mutex);
				return -FS_ENO_FREE_BLOCK;
			}
			uintptr_t * addr = kmalloc(sizeof(uintptr_t), GFP_KERNEL);
			(*addr) = second_indirection_block->block_addr;
			int ret = write_to_block(first_indirection_block, 0, (void *)addr, sizeof(uintptr_t));
			kfree(addr);
			
			if(ret)
			{
				mutex_unlock(&inode->inode_mutex);
				return ret;
			}
		}
		else
		{
			first_indirection_block = inode->disk_map->blocks[11];
			void * second_indirection_block_addr = (void *)first_indirection_block->block_addr + sizeof(uintptr_t)*inode->disk_map->single_indirect_pointer_ind;
			printk(KERN_INFO "FILE_SYSTEM_INFO : %lx\n", (uintptr_t)second_indirection_block_addr);
			second_indirection_block = mem_to_disk_block(fs_vfs, second_indirection_block_addr);
		}
		
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
			return ret;
		}
		
		inode->disk_map->double_indirect_pointer_ind += 1;
		if(inode->disk_map->double_indirect_pointer_ind == 256)
		{
			inode->disk_map->double_indirect_pointer_ind = 0;
			inode->disk_map->single_indirect_pointer_ind += 1;
			if(inode->disk_map->single_indirect_pointer_ind == 256)
			{
				inode->disk_map->single_indirect_pointer_ind = 0;
				inode->disk_map->disk_map_flag = inode->disk_map->disk_map_flag | 0x04;
			}
			else
			{
				struct fs_block * new_second_indirection_block = get_free_block(fs_vfs);
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
					return ret;
				}
			}
		}
		
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



