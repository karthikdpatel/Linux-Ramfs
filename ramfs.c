#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "config.h"
#include "include/fs_inode.h"

MODULE_LICENSE("GPL");

void * fs_memory;
struct fs_vfs * fs_vfs;

static int alloc_mem_fs(void)
{
	fs_vfs = kmalloc(sizeof(struct fs_vfs), GFP_KERNEL);
	if(!fs_vfs)
	{
		printk(KERN_ERR "FILE_SYSTEM : fs_vfs kmalloc error\n");
		return -FS_EMALLOC;
	}
	
	fs_memory = kvmalloc(FILE_SYSTEM_SIZE, GFP_KERNEL);
	if(!fs_memory)
	{
		printk(KERN_ERR "FILE_SYSTEM : mem kmalloc error\n");
		return -FS_EMALLOC;
	}
	return 0;
}

static void print_superblock(void)
{
	printk("FILE_SYSTEM : Super Block\n");
	for(int i = 0; i < 100; i++)
	{
		printk("FILE_SYSTEM : Super Block addr:%lx, %d\n", fs_vfs->super_block->blocks[i]->block_addr, i);
	}
}

static int fs_init(void)
{
	printk("FILE_SYSTEM : Mounting file system\n");
	alloc_mem_fs();
	printk("FILE_SYSTEM : Starting addr:%lx\n", (uintptr_t)fs_memory);
	
	intialise_file_system(fs_vfs);	
	initialise_disk_blocks(fs_vfs, fs_memory);
	allocate_inodes(fs_vfs);
	
	return 0;
}

static void fs_exit(void)
{
	printk("FILE_SYSTEM : Unmounting file system\n");
}

module_init(fs_init);
module_exit(fs_exit);

