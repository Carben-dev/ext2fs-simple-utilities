/*
 This program takes two command line arguments. The first is the name of an ext2 formatted virtual disk, and the second is an absolute path to a file or link (not a directory!) on that disk. The program should be the exact opposite of rm, restoring the specified file that has been previous removed. If the file does not exist (it may have been overwritten), or if it is a directory, then your program should return the appropriate error.
 Hint: The file to be restored will not appear in the directory entries of the parent directory, unless you search the "gaps" left when files get removed. The directory entry structure is the key to finding out these gaps and searching for the removed file.
 Note: If the directory entry for the file has not been overwritten, you will still need to make sure that the inode has not been reused, and that none of its data blocks have been reallocated. You may assume that the bitmaps are reliable indicators of such fact. If the file cannot be fully restored, your program should terminate with ENOENT, indicating that the operation was unsuccessful.
 Note(2): For testing, you should focus primarily on restoring files that you've removed using your ext2_rm implementation, since ext2_restore should undo the exact changes made by ext2_rm. While there are some removed entries already present in some of the image files provided, the respective files have been removed on a non-ext2 file system, which is not doing the removal the same way that ext2 would. In ext2, when you do "rm", the inode's i_blocks do not get zeroed, and you can do full recovery, as stated in the assignment (which deals solely with ext2 images, hence why you only have to worry about this type of (simpler) recovery). In other FSs things work differently. In ext3, when you rm a file, the data block indexes from its inode do get zeroed, so recovery is not as trivial. For example, there are some removed files in deletedfile.img, which have their blocks zero-ed out (due to how these images were created). There are also some unrecoverable entries in images like twolevel.img, largefile.img, etc. In such cases, your code should still work, but simply recover a file as an empty file (with no data blocks), or discard the entry if it is unrecoverable. However, for the most part, try to recover files that you've ext2_rm-ed yourself, to make sure that you can restore data blocks as well. We will not be testing recovery of files removed with a non-ext2 tool.
 Note(3): We will not try to recover files that had hardlinks at the time of removal. This is because when trying to restore a file, if its inode is already in use, there are two options: the file we're trying to restore previously had other hardlinks (and hence its inode never really got invalidated), _or_ its inode has been re-allocated to a completely new file. Since there is no way to tell between these 2 possibilities, recovery in this case should not be attempted.
 BONUS: Implement an additional "-r" flag (after the disk image argument), which allows restoring directories as well. In this case, you will have to recursively restore all the contents of the directory specified in the last argument. If "-r" is used with a regular file or link, then it should be ignored (the restore operation should be carried out as if the flag had not been entered). If you decide to do the bonus, make sure first that your ext2_restore works, then create a new copy of it and rename it to ext2_restore_bonus.c, and implement the additional functionality in this separate source file.
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
    if (parent_inode_num < 0) {
        return ENOENT;
    }
    
    struct ext2_inode *parent_inode = inode_table + parent_inode_num - 1;
    
    if (restore_from_dir_entry(parent_inode, path_name)) {
        return ENOENT;
    }
    
    
    
    
}
