CONFIG_MODULE_SIG=n
obj-m += ramfsko.o

ramfsko-objs := ramfs.o fs/fs_vfs.o fs/fs_block.o fs/fs_inode.o
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules

	
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
	
