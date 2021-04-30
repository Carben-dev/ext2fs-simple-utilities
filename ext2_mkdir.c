/*
 ext2_mkdir: This program takes two command line arguments.The first is the name of an ext2 formatted virtual disk.The second is an absolute path on your ext2 formatted disk.The program should work like mkdir, creating the final directory on the specified path on the disk.If any component on the path to the location where the final directory is to be created does not exist or if the specified directory already exists, then your program should return the appropriate error (ENOENT or EEXIST).
 Note:
 Please read the specifications to make sure you're implementing everything correctly (e.g., directory entries should be aligned to 4B, entry names are not null-terminated, etc.).
 When you allocate a new inode or data block, you *must use the next one available* from the corresponding bitmap (excluding reserved inodes, of course). Failure to do so will result in deductions, so please be careful about this requirement.
 Be careful to consider trailing slashes in paths. These will show up during testing so it's your responsibility to make your code as robust as possible by capturing corner cases.
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
        fprintf(stderr, "Usage: <image file name> <absolute path of new dir>\n");
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
    
    char path[PATH_MAX];
    char new_dir_name[EXT2_NAME_LEN];
    char parent_path[PATH_MAX];
    
    // Copy argv[2] to user stack
    memcpy(path, argv[2], strlen(argv[2]));
    
    // Case: handle user input path end with '/'
    // For example.
    //      "/d1/" -> "/d1"
    clear_end_slash(path);
    
    
    
    // Prepare parent path and new dir name
    // For example.
    //      "/d1/d2" -> parent path: "/d1" , new dir name: "d2"
    //      "/d1"    -> parent path: "/"   , new dir name: "d1"
    cut_path(path, parent_path, new_dir_name);
    
    
    
    // Find out in which inode this dir should be created
    int in_which_inode = get_inode_number_by_path(parent_path);
    if (in_which_inode == -1) {
        return ENOENT;
    }
    
    // Check if dir already exists
    if (get_inode_number_by_name(in_which_inode, new_dir_name) > 0) {
        return EEXIST;
    }
    
    // create dir
    int rs;
    if ((rs = inode_mkdir(in_which_inode, new_dir_name)) != 0) {
        return rs;
    }
    
}
