/*
 This program takes two command line arguments. The first is the name of an ext2 formatted virtual disk, and the second is an absolute path to a file or link (not a directory) on that disk. The program should work like rm, removing the specified file from the disk. If the file does not exist or if it is a directory, then your program should return the appropriate error. Once again, please read the specifications of ext2 carefully, to figure out what needs to actually happen when a file or link is removed (e.g., no need to zero out data blocks, must set i_dtime in the inode, removing a directory entry need not shift the directory entries after the one being deleted, etc.).
 BONUS: Implement an additional "-r" flag (after the disk image argument), which allows removing directories as well. In this case, you will have to recursively remove all the contents of the directory specified in the last argument. If "-r" is used with a regular file or link, then it should be ignored (the ext2_rm operation should be carried out as if the flag had not been entered). If you decide to do the bonus, make sure first that your ext2_rm works, then create a new copy of it and rename it to ext2_rm_bonus.c, and implement the additional functionality in this separate source file.
 */
 

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

#include <limits.h>
#include <string.h>
#include <errno.h>
#include "ext2_utils.h"

unsigned char *disk;

struct ext2_super_block *sb;
struct ext2_group_desc *gdt;
unsigned char *block_bitmap;
unsigned char *inode_bitmap;
struct ext2_inode *inode_table;

int main(int argc, const char * argv[]) {
    if(argc != 3) {
        fprintf(stderr, "Usage: <image file name> <absolute path of rm file>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);
    
    // map disk img into memory
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    
    // Init utils
    ext2_utils_init();
    
    //
    unsigned long path_len = strlen(argv[2]) + 1;
    char path[path_len];
    char path_parent[path_len];
    char path_name[EXT2_NAME_LEN];
    
    strcpy(path, argv[2]);
    clear_end_slash(path);
    cut_path(path, path_parent, path_name);
    
    // check if exist
    int parent_inode_num = get_inode_number_by_path(path_parent);
    int file_inode_num = get_inode_number_by_path(path);
    if (file_inode_num < 0) {
        return ENOENT;
    }
    
    struct ext2_inode *parent_inode = inode_table + parent_inode_num - 1;
    struct ext2_inode *file_inode = inode_table + file_inode_num - 1;
    if (file_inode->i_mode & EXT2_S_IFDIR) {
        return EISDIR;
    }
    
    remove_from_dir_entry(parent_inode, path_name);
    
    return 0;
}
