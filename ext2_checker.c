/*
 This program takes only one command line argument: the name of an ext2 formatted virtual disk. The program should implement a lightweight file system checker, which detects a small subset of possible file system inconsistencies and takes appropriate actions to fix them (as well as counts the number of fixes), as follows:
 a,the superblock and block group counters for free blocks and free inodes must match the number of free inodes and data blocks as indicated in the respective bitmaps. If an inconsistency is detected, the checker will trust the bitmaps and update the counters. Once such an inconsistency is fixed, your program should output the following message: "Fixed: X's Y counter was off by Z compared to the bitmap", where X stands for either "superblock" or "block group", Y is either "free blocks" or "free inodes", and Z is the difference (in absolute value). The Z values should be added to the total number of fixes.
 b,for each file, directory, or symlink, you must check if its inode's i_mode matches the directory entry file_type. If it does not, then you shall trust the inode's i_mode and fix the file_type to match. Once such an inconsistency is repaired, your program should output the following message: "Fixed: Entry type vs inode mismatch: inode [I]", where I is the inode number for the respective file system object. Each inconsistency counts towards to total number of fixes.
 c,for each file, directory or symlink, you must check that its inode is marked as allocated in the inode bitmap. If it isn't, then the inode bitmap must be updated to indicate that the inode is in use. You should also update the corresponding counters in the block group and superblock (they should be consistent with the bitmap at this point). Once such an inconsistency is repaired, your program should output the following message: "Fixed: inode [I] not marked as in-use", where I is the inode number. Each inconsistency counts towards to total number of fixes.
 d,for each file, directory, or symlink, you must check that its inode's i_dtime is set to 0. If it isn't, you must reset (to 0), to indicate that the file should not be marked for removal. Once such an inconsistency is repaired, your program should output the following message: "Fixed: valid inode marked for deletion: [I]", where I is the inode number. Each inconsistency counts towards to total number of fixes.
 e,for each file, directory, or symlink, you must check that all its data blocks are allocated in the data bitmap. If any of its blocks is not allocated, you must fix this by updating the data bitmap. You should also update the corresponding counters in the block group and superblock, (they should be consistent with the bitmap at this point). Once such an inconsistency is fixed, your program should output the following message: "Fixed: D in-use data blocks not marked in data bitmap for inode: [I]", where D is the number of data blocks fixed, and I is the inode number. Each inconsistency counts towards to total number of fixes.
 Your program must count all the fixed inconsistencies, and produce one last message: either "N file system inconsistencies repaired!", where N is the number of fixes made, or "No file system inconsistencies detected!".
 You may limit your consistency checks to only regular files, directories and symlinks.
 Hint: You might want to fix the counters based on the bitmaps, as a one-time step before attempting to fix any other type of inconsistency. Even if initially trusting the bitmaps may not be the way to go (since they could be corrupted), the counters should get readjusted in the later steps anyway, whenever the bitmaps get updated. The Z values from point a) should be added to the tally of fixes, but do not include any further superblock or block group counter adjustments from points c) and e) (since technically these may be just correcting the adjustments made in point a)).
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
    if(argc != 2) {
        fprintf(stderr, "Usage: <image file name>\n");
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
    
    // varible
    int num_fixed = 0;
    
    // check free inode and block count in sb and gdt
    // always trust bitmap
    // output certain message
    int free_inode_count = count_inode_bitmap();
    int free_block_count = count_block_bitmap();
    
    if (sb->s_free_blocks_count != free_block_count) {
        int s_blocks_off;
        if (sb->s_free_blocks_count > free_block_count) {
            s_blocks_off = sb->s_free_blocks_count - free_block_count;
        } else {
            s_blocks_off = free_block_count - sb->s_free_blocks_count;
        }
        sb->s_free_blocks_count = free_block_count;
        num_fixed++;
        printf("Fixed: superblock's free blocks counter was off by %d compared to the bitmap\n", s_blocks_off);
    }
    
    if (sb->s_free_inodes_count != free_inode_count) {
        int s_inodes_off;
        if (sb->s_free_inodes_count > free_inode_count) {
            s_inodes_off = sb->s_free_inodes_count - free_inode_count;
        } else {
            s_inodes_off = free_inode_count - sb->s_free_inodes_count;
        }
        num_fixed++;
        sb->s_free_inodes_count = free_inode_count;
        printf("Fixed: superblock's free inodes counter was off by %d compared to the bitmap\n", s_inodes_off);
    }
    
    if (gdt->bg_free_blocks_count != free_block_count) {
        int bg_blocks_off;
        if (gdt->bg_free_blocks_count > free_block_count) {
            bg_blocks_off = gdt->bg_free_blocks_count - free_block_count;
        } else {
            bg_blocks_off = free_block_count - gdt->bg_free_blocks_count;
        }
        gdt->bg_free_blocks_count = free_block_count;
        num_fixed++;
        printf("Fixed: block group's free blocks counter was off by %d compared to the bitmap\n", bg_blocks_off);
    }
    
    if (gdt->bg_free_inodes_count != free_inode_count) {
        int bg_inodes_off;
        if (gdt->bg_free_inodes_count > free_inode_count) {
            bg_inodes_off = gdt->bg_free_inodes_count - free_inode_count;
        } else {
            bg_inodes_off = free_inode_count - gdt->bg_free_inodes_count;
        }
        gdt->bg_free_inodes_count = free_inode_count;
        num_fixed++;
        printf("Fixed: block group's free inodes counter was off by %d compared to the bitmap\n", bg_inodes_off);
    }
    
    
    // check if type in dir entry match i_mode in inode
    // trust inode
    // output certain message
    
    
    // check if each inode in bitmap mark as used
    // trust inode
    // output certain message
    
    // check if each inode dtime is 0
    // reset to 0 if not
    // output certain message
    
    // check if each inode datablock mark as used in bitmap
    // trust inode
    // output certain message
    
    unsigned char root_ft_type = EXT2_FT_DIR;
    num_fixed += each_checker_rec(2, 2, &root_ft_type);
    
    // output summary
    if (num_fixed) {
        printf("%d file system inconsistencies repaired!\n", num_fixed);
    } else {
        printf("No file system inconsistencies detected!\n");
    }
    
    
    return 0;
    
}
