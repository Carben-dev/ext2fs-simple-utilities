/*
 This program takes three command line arguments. The first is the name of an ext2 formatted virtual disk. The other two are absolute paths on your ext2 formatted disk. The program should work like ln, creating a link from the first specified file to the second specified path. This program should handle any exceptional circumstances, for example: if the source file does not exist (ENOENT), if the link name already exists (EEXIST), if a hardlink refers to a directory (EISDIR), etc. then your program should return the appropriate error code. Additionally, this command may take a "-s" flag, after the disk image argument. When this flag is used, your program must create a symlink instead (other arguments remain the same).
 Note:
 For symbolic links, you will see that the specs mention that if the path is short enough, it can be stored in the inode in the space that would otherwise be occupied by block pointers - these are called fast symlinks. Do *not* implement fast symlinks, just store the path in a data block regardless of length, to keep things uniform in your implementation, and to facilitate testing. If in doubt about correct operation of links, use the ext2 specs and ask on the discussion board.
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
#include <getopt.h>
#include "ext2_utils.h"

unsigned char *disk;

struct ext2_super_block *sb;
struct ext2_group_desc *gdt;
unsigned char *block_bitmap;
unsigned char *inode_bitmap;
struct ext2_inode *inode_table;

int main(int argc, const char * argv[]) {
    // if -s flag set, this will be set to 1
    int create_symlink = 0;
    
    if(argc < 4) {
        fprintf(stderr, "Usage: <image file name> <-s flag> <src> <dst>\n");
        exit(1);
    }
    
    if (argc == 5 && strncmp("-s", argv[2], strlen("-s")) == 0) {
        create_symlink = 1;
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
    
    // find out max path len
    unsigned long src_path_len;
    unsigned long lnk_path_len;
    if (create_symlink) {
        src_path_len = strlen(argv[3]) + 1;
        lnk_path_len = strlen(argv[4]) + 1;
    } else {
        src_path_len = strlen(argv[2]) + 1;
        lnk_path_len = strlen(argv[3]) + 1;
    }
    
    //
    char src_path[src_path_len];
    char lnk_path[lnk_path_len];
    char src_parent_path[src_path_len];
    char src_file_name[EXT2_NAME_LEN];
    char lnk_parent_path[lnk_path_len];
    char lnk_file_name[EXT2_NAME_LEN];
    
    if (create_symlink) {
        strcpy(src_path, argv[3]);
        strcpy(lnk_path, argv[4]);
    } else {
        strcpy(src_path, argv[2]);
        strcpy(lnk_path, argv[3]);
    }
    
    clear_end_slash(src_path);
    clear_end_slash(lnk_path);
    
    cut_path(src_path, src_parent_path, src_file_name);
    cut_path(lnk_path, lnk_parent_path, lnk_file_name);
    
    int src_file_inode_num = get_inode_number_by_path(src_path);
    struct ext2_inode *src_file_inode = inode_table + (src_file_inode_num - 1);
    int lnk_parent_inode_num = get_inode_number_by_path(lnk_parent_path);
    struct ext2_inode *lnk_parent_inode = inode_table + (lnk_parent_inode_num - 1);
    
    // check if src path exist
    if (src_file_inode_num < 0) {
        return ENOENT;
    }
    
    // check if lnk parent exist
    if (lnk_parent_inode_num < 0) {
        return ENOENT;
    }
    
    
    // check if lnk already exist
    if (get_inode_number_by_name(lnk_parent_inode_num, lnk_parent_path) > 0) {
        return EEXIST;
    }
    
    if (create_symlink) { // create soft link
        // allocate space in inode_table for soft link
        int soft_link_inode_num = new_inode(EXT2_S_IFLNK, strlen(src_path)+1);
        struct ext2_inode *soft_link_inode = inode_table + (soft_link_inode_num - 1);
        
        // copy src_path to datablock
        copy_to_inode_datablock(soft_link_inode, (unsigned char *)src_path, strlen(src_path)+1);
        
        // add soft link inode to lnk_parent
        add_to_dir_entry(lnk_parent_inode, soft_link_inode_num, lnk_file_name, EXT2_FT_SYMLINK);
        return 0;
        
    } else { // create hard link
        // check if src is a dir
        if (src_file_inode->i_mode & EXT2_S_IFDIR) {
            return EISDIR;
        }
        
        unsigned char type;
        
        // check file type
        if ((src_file_inode->i_mode & EXT2_S_IFLNK) == EXT2_S_IFLNK) {
            type = EXT2_FT_SYMLINK;
        } else if (src_file_inode->i_mode & EXT2_S_IFREG) {
            type = EXT2_FT_REG_FILE;
        } else {
            exit(-1);
        }
        
        add_to_dir_entry(lnk_parent_inode, src_file_inode_num, lnk_file_name, type);
        return 0;
    }
    
    
}
