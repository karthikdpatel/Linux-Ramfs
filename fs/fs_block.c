#include "../include/fs_block.h"

/*
Initialises superblock
Initialises all the superblock parameters see /include/fs_block.h to get to know about superblock parameters
*/
void initialise_super_block(struct fs_vfs * fs_vfs)
{
	fs_vfs->super_block = kmalloc(sizeof(struct fs_superblock), GFP_KERNEL);
	
	for(int i = 0; i < 100; i++)
	{
		fs_vfs->super_block->blocks[i] = NULL;
	}
	
	fs_vfs->super_block->fs_block_ind = 0;
	
	mutex_init(&fs_vfs->super_block->superblock_mutex);
}

inline void deallocate_super_block(struct fs_vfs * fs_vfs)
{
	kfree(fs_vfs->super_block);
}

/*
Copies the data from src memory to the given disk block
Parameters:-
struct fs_block * block :- destination disk block to which the data will be written
int offset :- Position from the start of the disk block from which the write starts
void * src :- src memory
int size :- size of the data that will be written
*/
int write_to_block(struct fs_block * block, int offset, void * src, int size)
{
	if(offset < 0 || offset > FS_BLOCK_SIZE || block == NULL || src == NULL)
	{
		if(offset < 0)
			printk(KERN_ERR "FILE_SYSTEM_ERROR : offset < 0\n");
			
		if(offset > FS_BLOCK_SIZE)
			printk(KERN_ERR "FILE_SYSTEM_ERROR : offset > FS_BLOCK_SIZE\n");
			
		if(block == NULL)
			printk(KERN_ERR "FILE_SYSTEM_ERROR : block == NULL\n");
			
		if(src == NULL)
			printk(KERN_ERR "FILE_SYSTEM_ERROR : src == NULL\n");
			
		printk(KERN_ERR "FILE_SYSTEM_ERROR : Wrong input parameter in write_to_block()\n");
		return -FS_EINPUT_PARAMETER;
	}
	if(size > FS_BLOCK_SIZE || size > (FS_BLOCK_SIZE - offset))
	{
		printk(KERN_DEBUG "FILE_SYSTEM_ERROR_DEBUG : size:%d, offset:%d\n", size, offset);
		printk(KERN_ERR "FILE_SYSTEM_ERROR : Size of data requested to write to block is greater than block size\n");
		return -FS_EBLOCK_SIZE;
	}
	
	void * dest_addr = (void *)block->block_addr + offset;
	
	mutex_lock(&block->diskblock_mutex);
	
	memcpy(dest_addr, src, size);
	
	mutex_unlock(&block->diskblock_mutex);
	
	return 0;
}

/*
Allocates disk block and initialises its parameters
struct fs_vfs * fs_vfs :- filesystem to which the disk block belongs
void * memory :- starting address of the disk block
int flag :- flag indicates whether the disk block belongs to free list or super block list
*/
struct fs_block * initialise_block(struct fs_vfs * fs_vfs, void * start_memory, int flag)
{
	struct fs_block * block = kmalloc(sizeof(struct fs_block), GFP_KERNEL);
	block->block_addr = (uintptr_t)start_memory;
	mutex_init(&block->diskblock_mutex);
	switch(flag)
	{
		case FS_DISK_BLOCK_SUPER_BLOCK: list_add_tail(&block->fs_vfs_list, &fs_vfs->super_block_disk_list);
						break;
		
		case FS_DISK_BLOCK_FREE_LIST:   list_add_tail(&block->fs_vfs_list, &fs_vfs->free_disk_block_list);
						break;
		default:			printk(KERN_ERR "FILE_SYSTEM : Wrong switch case in initialise_block()\n");
	}
	return block;
}

inline void destroy_block(struct fs_vfs * fs_vfs, struct fs_block * block)
{
	list_del(&block->fs_vfs_list);
	kfree(block);
}

/*
Initialises disk blocks
*/
void initialise_disk_blocks(struct fs_vfs * fs_vfs, void * start_memory)
{
	initialise_super_block(fs_vfs);
	
	int num_disk_block = FILE_SYSTEM_SIZE/FS_BLOCK_SIZE;
	int block_count = 0, super_block_count = 0;
	
	int num_super_block = (num_disk_block / 100) - 1;
	
	num_disk_block -= num_super_block;
	
	struct fs_block * block, * prev_block = NULL;
	
	bool first_super_block_flag = true;
	
	printk("FILE_SYSTEM : Initialising disk blocks\n");
	
	while(super_block_count < num_super_block)
	{
		uintptr_t *block_address = kmalloc(100*sizeof(uintptr_t), GFP_KERNEL);
		if(!block_address)
		{
			printk(KERN_ERR "Error allocating block address memory\n");
			return;
		}
		
		int num_blocks = 99;
		if(first_super_block_flag)
		{
			num_blocks = 100;
			first_super_block_flag = false;
		}
		
		for(int i = 0; i < num_blocks; i++)
		{
			block = initialise_block(fs_vfs, start_memory + block_count*FS_BLOCK_SIZE, FS_DISK_BLOCK_FREE_LIST);
			block_address[i] = block->block_addr;
			if(block_address[i] != block->block_addr)
				printk("FILE_SYSTEM : Block addr:%lx, %lx\n", block_address[i], block->block_addr);
			block_count += 1;
		}
		
		block = initialise_block(fs_vfs, start_memory + block_count*FS_BLOCK_SIZE, FS_DISK_BLOCK_SUPER_BLOCK);
		block_count += 1;
		if(num_blocks == 99)
		{
			block_address[99] = prev_block->block_addr;
		}
		write_to_block(block, 0, block_address, 100*sizeof(uintptr_t));
		kfree(block_address);
		super_block_count += 1;
		prev_block = block;
	}
	
	for(int i = 0; i < 99; i++)
	{
		block = initialise_block(fs_vfs, start_memory + block_count*FS_BLOCK_SIZE, FS_DISK_BLOCK_FREE_LIST);
		block_count += 1;
		mutex_lock(&fs_vfs->super_block->superblock_mutex);
		fs_vfs->super_block->blocks[i] = block;
		mutex_unlock(&fs_vfs->super_block->superblock_mutex);
	}
	mutex_lock(&fs_vfs->super_block->superblock_mutex);
	fs_vfs->super_block->blocks[99] = prev_block;
	mutex_unlock(&fs_vfs->super_block->superblock_mutex);
	
	printk("FILE_SYSTEM : Initialised %d disk blocks\n", FILE_SYSTEM_SIZE/FS_BLOCK_SIZE);
}

/*
Given a memory address this function returns the disk block corresponding to the memory address only if the block is free else returns NULL

Note :- This function has to be called while holding the super block mutex struct (i.e., fs_superblock.superblock_mutex)
*/
struct fs_block * mem_to_disk_block(struct fs_vfs * fs_vfs, void * mem_addr)
{
	uintptr_t mem = (uintptr_t)mem_addr;
	
	struct fs_block * block;
	
	list_for_each_entry(block, &fs_vfs->super_block_disk_list, fs_vfs_list)
	{
		if((block->block_addr <= mem) && (mem < (block->block_addr + FS_BLOCK_SIZE)))
		{
			return block;
		}
	}
	
	list_for_each_entry(block, &fs_vfs->free_disk_block_list, fs_vfs_list)
	{
		if((block->block_addr <= mem) && (mem < (block->block_addr + FS_BLOCK_SIZE)))
		{
			return block;
		}
	}
	
	list_for_each_entry(block, &fs_vfs->allocated_disk_block_list, fs_vfs_list)
	{
		if((block->block_addr <= mem) && (mem < (block->block_addr + FS_BLOCK_SIZE)))
		{
			return block;
		}
	}
	
	return NULL;
}

/*
This function returns a free block if its available else returns NULL
Note : This function uses superblock and struct fs_vfs mutexes
*/
struct fs_block * get_free_block(struct fs_vfs * fs_vfs)
{
	if(fs_vfs->num_free_disk_blocks == 0)
	{
		printk(KERN_ERR "FILE_SYSTEM_ERROR : No free blocks available\n");
		return NULL;
	}
	else if(fs_vfs->num_free_disk_blocks == 1)
	{
		struct fs_superblock * superblock = fs_vfs->super_block;
		struct fs_block * block;
		mutex_lock(&superblock->superblock_mutex);
		block = superblock->blocks[superblock->fs_block_ind];
		superblock->blocks[superblock->fs_block_ind] = NULL;
		superblock->fs_block_ind += 1;
		mutex_unlock(&superblock->superblock_mutex);
		
		mutex_lock(&fs_vfs->vfs_lock);
		list_del(&block->fs_vfs_list);
		list_add(&block->fs_vfs_list, &fs_vfs->allocated_disk_block_list);
		fs_vfs->num_free_disk_blocks -= 1;
		mutex_unlock(&fs_vfs->vfs_lock);
		
		return block;
	}
	else
	{
		struct fs_superblock * superblock = fs_vfs->super_block;
		struct fs_block * block;
		
		mutex_lock(&superblock->superblock_mutex);
		block = superblock->blocks[superblock->fs_block_ind];
		
		if(superblock->fs_block_ind == 99)
		{
			uintptr_t * addr = (uintptr_t *)block->block_addr;
			
			//printk("FILE_SYSTEM : Copying diskblocks into super block\n");
			for(int i = 0; i < 100; i++)
			{
				//printk("FILE_SYSTEM : Copying diskblocks into super block Addr:%lx, %d\n", addr[i], i);
				superblock->blocks[i] = mem_to_disk_block(fs_vfs, (void *)addr[i]);
				if(!superblock->blocks[i])
				{
					mutex_unlock(&superblock->superblock_mutex);
					printk(KERN_ERR "FILE_SYSTEM_ERROR : Copying diskblocks into super block:%lx\n", addr[i]);
					return NULL;
				}
			}
			superblock->fs_block_ind = 0;
		}
		else
		{
			superblock->blocks[superblock->fs_block_ind] = NULL;
			superblock->fs_block_ind += 1;
		}
		
		mutex_unlock(&superblock->superblock_mutex);
		
		mutex_lock(&fs_vfs->vfs_lock);
		list_del(&block->fs_vfs_list);
		list_add(&block->fs_vfs_list, &fs_vfs->allocated_disk_block_list);
		fs_vfs->num_free_disk_blocks -= 1;
		mutex_unlock(&fs_vfs->vfs_lock);
		
		return block;
	}
	
}


void put_free_block(struct fs_vfs * fs_vfs, struct fs_block * block)
{
	struct fs_superblock * superblock = fs_vfs->super_block;
	
	mutex_lock(&superblock->superblock_mutex);
	
	if(superblock->fs_block_ind == 0)
	{
		uintptr_t *block_address = kmalloc(100*sizeof(uintptr_t), GFP_KERNEL);
		for(int i = 0; i < 100; i++)
		{
			block_address[i] = superblock->blocks[i]->block_addr;
			superblock->blocks[i] = NULL;
		}
		
		write_to_block(block, 0, (void *)block_address, 100*sizeof(uintptr_t));
		kfree(block_address);
		superblock->blocks[99] = block;
		superblock->fs_block_ind = 99;
		
		mutex_lock(&fs_vfs->vfs_lock);
		list_del(&block->fs_vfs_list);
		list_add(&block->fs_vfs_list, &fs_vfs->super_block_disk_list);
		fs_vfs->num_free_disk_blocks += 1;
		mutex_unlock(&fs_vfs->vfs_lock);
	}
	else
	{
		superblock->fs_block_ind -= 1;
		superblock->blocks[superblock->fs_block_ind] = block;
		
		mutex_lock(&fs_vfs->vfs_lock);
		list_del(&block->fs_vfs_list);
		list_add(&block->fs_vfs_list, &fs_vfs->free_disk_block_list);
		fs_vfs->num_free_disk_blocks += 1;
		mutex_unlock(&fs_vfs->vfs_lock);
	}
	
	mutex_unlock(&superblock->superblock_mutex);
	
}


