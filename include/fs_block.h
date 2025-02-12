#include <linux/list.h>
#include "fs_vfs.h"

#define SUPER_BLOCK_FLAG 0x01
#define LAST_SUPER_BLOCK_FLAG 0x02

typedef struct fs_block
{
	uintptr_t block_addr;
	struct list_head fs_vfs_list;
	
	struct mutex diskblock_mutex;
}fs_block_t;

typedef struct fs_superblock
{
	struct fs_block *blocks[100];
	int fs_block_ind;

	struct mutex superblock_mutex;
}fs_superblock_t;

void initialise_super_block(struct fs_vfs *);
struct fs_block * initialise_block(struct fs_vfs * fs_vfs, void * start_memory, int flag);
int write_to_block(struct fs_block * block, int offset, void * src, int size);
void initialise_disk_blocks(struct fs_vfs * fs_vfs, void * start_memory);

struct fs_block * mem_to_disk_block(struct fs_vfs * fs_vfs, void * mem_addr);
struct fs_block * get_free_block(struct fs_vfs * fs_vfs);
void put_free_block(struct fs_vfs * fs_vfs, struct fs_block * block);

