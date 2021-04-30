/*
 This program takes three command line arguments. The first is the name of an ext2 formatted virtual disk. The second is the path to a file on your native operating system, and the third is an absolute path on your ext2 formatted disk. The program should work like cp, copying the file on your native file system onto the specified location on the disk. If the source file does not exist or the target is an invalid path, then your program should return the appropriate error (ENOENT). If the target is a file with the same name that already exists, you should not overwrite it (as cp would), just return EEXIST instead.
 Note:
 Please read the specifications of ext2 carefully, some things you will not need to worry about (like permissions, gid, uid, etc.), while setting other information in the inodes may be important (e.g., i_dtime).
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
#include <errno.h>
#include <string.h>
#include "ext2_utils.h"

unsigned char *disk;

struct ext2_super_block *sb;
struct ext2_group_desc *gdt;
unsigned char *block_bitmap;
unsigned char *inode_bitmap;
struct ext2_inode *inode_table;

int main(int argc, const char * argv[]) {
    if(argc != 4) {
        fprintf(stderr, "Usage: <image file name> <src> <dst>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);
    int src_fd = open(argv[2], O_RDWR);
    
    // map disk img into memory
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    
    // map src file into memory
    struct stat src_stat;
    if (lstat(argv[2], &src_stat) == -1) {
        return ENOENT;
    }
    int src_file_size = (int)src_stat.st_size;
    unsigned char *src_file = mmap(NULL, src_file_size, PROT_READ | PROT_WRITE, MAP_SHARED, src_fd, 0);
    if(src_file == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    // Init utils
    ext2_utils_init();
    
    
    char dst_path[PATH_MAX];
    char dst_file_name[EXT2_NAME_LEN];
    char dst_file_parent[PATH_MAX];
    strcpy(dst_path, argv[3]);
    clear_end_slash(dst_path);
    cut_path(dst_path, dst_file_parent, dst_file_name);
    
    
    // find out dst file parent dir inode
    int dst_file_parent_inode_num = get_inode_number_by_path(dst_file_parent);
    // check if dst file parent exist
    if (dst_file_parent_inode_num < 0) {
        return ENOENT;
    }
    // check if there is a file in dst dir has same name as src file
    if (get_inode_number_by_name(dst_file_parent_inode_num, dst_file_name) > 0) {
        return EEXIST;
    }
    struct ext2_inode *dst_file_parent_inode = inode_table + (dst_file_parent_inode_num - 1);
    
    // create inode for dst_file
    int dst_file_inode_num = new_inode(EXT2_S_IFREG, src_file_size);
    struct ext2_inode *dst_file_inode = inode_table + dst_file_inode_num - 1;
    
    
    /* -- copy datablock -- */
    copy_to_inode_datablock(dst_file_inode, src_file, src_file_size);
    
    // add new file entry to dst parent dir entry
    add_to_dir_entry(dst_file_parent_inode, dst_file_inode_num, dst_file_name, EXT2_FT_REG_FILE);

    
}
