#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include "ext2_utils.h"

extern unsigned char *disk;

extern struct ext2_super_block *sb;
extern struct ext2_group_desc *gdt;
extern unsigned char *block_bitmap;
extern unsigned char *inode_bitmap;
extern struct ext2_inode *inode_table;


void ext2_utils_init() {
    // Init global varible.
    // setup sb, gdt, block_bitmap, inode_bitmap, inode_table
    sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    gdt = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE + sizeof(struct ext2_super_block));
    block_bitmap = (unsigned char *)(disk + (gdt->bg_block_bitmap * EXT2_BLOCK_SIZE));
    inode_bitmap = (unsigned char *)(disk + (gdt->bg_inode_bitmap * EXT2_BLOCK_SIZE));
    inode_table = (struct ext2_inode *)(disk + (gdt->bg_inode_table * EXT2_BLOCK_SIZE));
    return;
}


int get_inode_number_by_name(int in_which_dir_num,
                             char *name)
{
    // Inode index = inode number - 1
    // Get inode by inode index.
    struct ext2_inode *in_which_inode = inode_table + in_which_dir_num - 1;
    
    int curr_i_block = 0;
    int *in_which_inode_i_block_array = read_i_block_into_array(in_which_inode);
    // walk through each datablock of parent dir
    while (in_which_inode_i_block_array[curr_i_block] != -1) {
        unsigned char *in_which_inode_data = disk + EXT2_BLOCK_SIZE * in_which_inode_i_block_array[curr_i_block];
        
        // Dir entry struct
        struct ext2_dir_entry *entry;
        // Offset in data block
        int curr_entry_off = 0;
        // walk through datablock
        while (curr_entry_off < EXT2_BLOCK_SIZE) {
            entry = (struct ext2_dir_entry *)(in_which_inode_data + curr_entry_off);
            if (strncmp(entry->name, name, entry->name_len) == 0) {
                return entry->inode;
            }
            curr_entry_off += entry->rec_len;
        }
        curr_i_block++;
    }
    // If reach here no match found
    return -1;
}


int get_inode_number_by_path_helper(char *path,
                                    int curr_inode_num)
{
    // Inode index = inode number - 1
    // Get inode by inode index.
    struct ext2_inode *curr_inode = inode_table + curr_inode_num - 1;
    
    // Prepare path
    // For example.
    // "d1/d2/d3" -> path_next_call: "d2/d3", dir_name: "d1"
    // "d3"       -> path_next_call: NULL   , dir_name: "d3"
    char *path_next_call = strchr(path, '/');
    char dir_name[EXT2_NAME_LEN];
    
    
    /*
     *  Base case:
     *      inode ref an reg file and not reach end return -1
     *          or
     *      reach end of path
     */
    
    if ((curr_inode->i_mode & EXT2_S_IFREG) &&
        path_next_call != NULL) {
        return -1;
    }
    
    // reach end of path, then search the inode number current dir.
    if (path_next_call == NULL) {
        memcpy(dir_name, path, strlen(path));
        return get_inode_number_by_name(curr_inode_num, dir_name);
    }
    
    
    /*
     *  Recursive part
     */
    
    // Prepare dir_name
    int i;
    int first_slash_index = 0;
    for (i = 0; i < strlen(path); i++) {
        if (path[i] == '/') {
            first_slash_index = i;
            break;
        }
    }
    memcpy(dir_name, path, first_slash_index);
    dir_name[first_slash_index] = '\0';
    
    // Enter next level dir
    // Before enter next level, check if entry exist.
    int next_level_inode = get_inode_number_by_name(curr_inode_num, dir_name);
    if (next_level_inode == -1) {
        return -1;
    }
    
    return get_inode_number_by_path_helper(path_next_call + 1, next_level_inode);
    
}


int get_inode_number_by_path(char *path)
{
    // Invaild path not start with '/'
    if (path[0] != '/') {
        return -1;
    }
    
    // Case: path = "/"
    if (strlen(path) == 1) {
        return 2;
    }
    
    return get_inode_number_by_path_helper(path + 1, 2);
}

int inode_mkdir(int parent,
                char *new_dir_name)
{
    struct ext2_inode *parent_inode = inode_table + parent - 1;
    
    int new_dir_inode_num = ialloc();
    int new_dir_datablock_idx = dalloc();
    if (new_dir_inode_num < 0 || new_dir_datablock_idx < 0) {
        return ENOSPC;
    }
    
    // Build inode and datablock for new dir
    struct ext2_inode *new_dir_inode = inode_table + new_dir_inode_num - 1;
    unsigned char *new_dir_datablock = disk + (EXT2_BLOCK_SIZE * new_dir_datablock_idx);
    
    new_dir_inode->i_mode           = 0x0000 | EXT2_S_IFDIR;
    new_dir_inode->i_uid            = 0;
    new_dir_inode->i_size           = EXT2_BLOCK_SIZE;
    new_dir_inode->i_ctime          = 0;
    new_dir_inode->i_mtime          = 0;
    new_dir_inode->i_dtime          = 0;
    new_dir_inode->i_gid            = 0;
    new_dir_inode->i_links_count    = 0;
    new_dir_inode->i_blocks         = new_dir_inode->i_size/512;
    new_dir_inode->i_flags          = 0;
    new_dir_inode->osd1             = 0;
    new_dir_inode->i_block[0]       = new_dir_datablock_idx;
    new_dir_inode->i_generation     = 0;
    new_dir_inode->i_file_acl       = 0;
    new_dir_inode->i_dir_acl        = 0;
    // padding
    memset(new_dir_inode->extra, 0, 3);
    
    // Init dir_entry datablock for new dir
    init_dir_entry(new_dir_datablock, new_dir_inode_num, parent);
    
    // Add inode back to parent dir entry
    add_to_dir_entry(parent_inode, new_dir_inode_num, new_dir_name, EXT2_FT_DIR);
    
    // Update gdt
    gdt->bg_used_dirs_count++;
    
    return 0;
}

void add_to_dir_entry(struct ext2_inode *parent_inode,
                      unsigned int inode_num,
                      char *name,
                      unsigned char type)
{
    int parent_inode_last_block_index = (parent_inode->i_blocks / 2) - 1;
    unsigned char *parent_inode_data = disk + EXT2_BLOCK_SIZE * parent_inode->i_block[parent_inode_last_block_index];
    
    // Find out the last entry
    int curr_entry_off = 0;
    struct ext2_dir_entry *entry;
    while (1) {
        entry = (struct ext2_dir_entry *)(parent_inode_data + curr_entry_off);
        if (entry->rec_len + curr_entry_off == EXT2_BLOCK_SIZE) {
            
            //handle case: no space for last entry
            if (entry->rec_len - sizeof(struct ext2_dir_entry) - entry->name_len < sizeof(struct ext2_dir_entry) + strlen(name)) {
                // allocate new block on datablock
                int new_block_number = dalloc();
                if (new_block_number < 0) {
                    exit(ENOSPC);
                }
                
                // add this block back to parent inode
                parent_inode->i_block[parent_inode_last_block_index + 1] = new_block_number;
                parent_inode->i_blocks += 2;
                
                // add entry in this block
                entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * new_block_number);
                // Build entry in parent for new dir
                entry->file_type    = type;
                entry->inode        = inode_num;
                entry->name_len     = strlen(name);
                entry->rec_len      = EXT2_BLOCK_SIZE;
                memcpy(entry->name, name, entry->name_len);
                // align name, padding null char to the end of entry->name
                int padding_len = 4 - (entry->name_len % 4);
                memset(entry->name + entry->name_len, '\0', padding_len);
                
                // Update link count for self
                struct ext2_inode *self_inode = inode_table + (inode_num - 1);
                self_inode->i_links_count++;
                return;
            }
            
            
            break;
        }
        curr_entry_off += entry->rec_len;
    }
    
    // Update the last entry's rec_len
    int last_entry_len = sizeof(struct ext2_dir_entry) + entry->name_len;
    if (last_entry_len % 4) {   // align
        last_entry_len += 4 - (last_entry_len % 4);
    }
    entry->rec_len = last_entry_len;
    
    // Move to next
    curr_entry_off += entry->rec_len;
    entry = (struct ext2_dir_entry *)(parent_inode_data + curr_entry_off);
    
    // Build entry in parent for new dir
    entry->file_type    = type;
    entry->inode        = inode_num;
    entry->name_len     = strlen(name);
    entry->rec_len      = EXT2_BLOCK_SIZE - curr_entry_off;
    memcpy(entry->name, name, entry->name_len);
    // align name, padding null char to the end of entry->name
    int padding_len = 4 - (entry->name_len % 4);
    memset(entry->name + entry->name_len, '\0', padding_len);
    
    // Update link count for self
    struct ext2_inode *self_inode = inode_table + (inode_num - 1);
    self_inode->i_links_count++;
    
}

int remove_from_dir_entry(struct ext2_inode * parent_inode,
                          char *name)
{
    int *parent_inode_iblock_array = read_i_block_into_array(parent_inode);
    struct ext2_dir_entry *prev_entry;
    struct ext2_dir_entry *curr_entry;
    
    int i = -1;
    int curr_offset = 1024;
    while (parent_inode_iblock_array[i] != -1) {
        if (curr_offset == EXT2_BLOCK_SIZE) {
            i++;
            curr_offset = 0;
            prev_entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * parent_inode_iblock_array[i] + curr_offset);
            curr_entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * parent_inode_iblock_array[i] + curr_offset);
        }
        
        if (strncmp(name, curr_entry->name, curr_entry->name_len) == 0) {
            // do entry del
            prev_entry->rec_len += curr_entry->rec_len;
            // update inode link count
            struct ext2_inode *curr_inode = inode_table + curr_entry->inode - 1;
            curr_inode->i_links_count--;
            
            // inode link count drop to 0
            if (curr_inode->i_links_count == 0) {
                free_inode(curr_entry->inode);
            }
            
            free(parent_inode_iblock_array);
            return 0;
        }
        
        prev_entry = curr_entry;
        curr_offset += curr_entry->rec_len;
        curr_entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * parent_inode_iblock_array[i] + curr_offset);
    }
    
    free(parent_inode_iblock_array);
    return -1;
}

int restore_from_dir_entry(struct ext2_inode *parent_inode,
                           char *name)
{
    int *parent_inode_iblock_array = read_i_block_into_array(parent_inode);
    int i = 0;
    int curr_off = 0;
    struct ext2_dir_entry *entry;
    
    while (parent_inode_iblock_array[i] != -1) {
        
        entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * parent_inode_iblock_array[i] + curr_off);
        int gap_size = entry->rec_len - sizeof(struct ext2_dir_entry) - entry->name_len;
        int gap_off = 0;
        if (entry->name_len%4) {
            gap_off = 4 - (entry->name_len%4);
        }
        
        while (gap_off < gap_size) {
            if (gap_size < sizeof(struct ext2_dir_entry) + 4) {
                break;
            }
            struct ext2_dir_entry *gap_entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * parent_inode_iblock_array[i] + curr_off + sizeof(struct ext2_dir_entry) + entry->name_len + gap_off);
            if (strncmp(name, gap_entry->name, gap_entry->name_len) == 0) {
                struct ext2_inode *curr_gap_inode = inode_table + gap_entry->inode - 1;
                
                // if inode for deleted entry reused, can't recover
                if (curr_gap_inode->i_dtime == 0) {
                    return -1;
                }
                
                // restore inode
                curr_gap_inode->i_links_count++;
                curr_gap_inode->i_dtime = 0;
                
                // restore dir entry
                gap_entry->rec_len = entry->rec_len - gap_off - sizeof(struct ext2_dir_entry) - entry->name_len;
                entry->rec_len = sizeof(struct ext2_dir_entry) + entry->name_len + gap_off;
                
                
                // restore inode bitmap
                if (restore_inode_bitmap(gap_entry->inode)) {
                    return -1;
                }
                
                
                // restore block bitmap
                if (curr_gap_inode->i_blocks == 0) {
                    return 0; // if blocks zeroed out
                }
                
                int *curr_gap_inode_iblock_array = read_i_block_into_array(curr_gap_inode);
                int j = 0;
                while (curr_gap_inode_iblock_array[j] != -1) {
                    if (restore_block_bitmap(curr_gap_inode_iblock_array[j])) {
                        return -1;
                    }
                    j++;
                }
                if (j >= 12) {// restore indirect block
                    if (restore_block_bitmap(curr_gap_inode->i_block[12])) {
                        return -1;
                    };
                }
                
                return 0;
            }
            
            gap_off += gap_entry->name_len + sizeof(struct ext2_dir_entry);
            if (gap_entry->name_len%4) {
                gap_off += 4 - (gap_entry->name_len%4);
            }
        }
        
        curr_off += entry->rec_len;
        
        if (curr_off == EXT2_BLOCK_SIZE) {
            i++;
        }
    }
    
    // If reach here, not found
    return -1;
}

void init_dir_entry(unsigned char *entry_datablock,
                    int self_inode_num,
                    int parent_inode_num)
{
    struct ext2_dir_entry *entry;
    // Build first entry for self "."
    entry = (struct ext2_dir_entry *)entry_datablock;
    entry->file_type    = EXT2_FT_DIR;
    entry->inode        = self_inode_num;
    entry->name[0]      = '.';
    entry->name_len     = 1;
    entry->rec_len      = 12;
    // Update self link count
    struct ext2_inode *self_inode = inode_table + (self_inode_num - 1);
    self_inode->i_links_count++;
    
    // Build second entry for parent ".."
    entry = (struct ext2_dir_entry *)(entry_datablock + 12);
    entry->file_type    = EXT2_FT_DIR;
    entry->inode        = parent_inode_num;
    entry->name[0]      = '.';
    entry->name[1]      = '.';
    entry->name_len     = 2;
    entry->rec_len      = EXT2_BLOCK_SIZE - 12;
    // Update parent link count
    struct ext2_inode *parent_inode = inode_table + (parent_inode_num - 1);
    parent_inode->i_links_count++;
}

int *read_i_block_into_array(struct ext2_inode *inode)
{
    int total_blocks = inode->i_blocks/2;
    int *result = malloc(sizeof(int) * (total_blocks + 1));
    int num_block_read = 0;
    int i;
    
    for (i = 0; i < 12; i++) {
        result[i] = inode->i_block[i];
        num_block_read++;
        if (num_block_read == total_blocks) {
            result[i + 1] = -1;
            return result;
        }
    }
    
    // If reach here, go into indirect_block
    num_block_read++;
    
    // Since disk size is 128kb, only consider the cases that
    //      there is only one indirect block.
    unsigned int *indirect_block = (unsigned int *)(disk + EXT2_BLOCK_SIZE * inode->i_block[12]);
    for (i = 0; i < EXT2_BLOCK_SIZE/4; i++) {
        result[12 + i] = indirect_block[i];
        num_block_read++;
        if (num_block_read == total_blocks) {
            result[12 + i + 1] = -1;
            return result;
        }
    }
    
    // If reach here, error
    free(result);
    return NULL;
}

void write_array_into_i_block(struct ext2_inode *inode,
                              int *array,
                              int array_size)
{
    int num_block_written = 0;
    int i;
    
    for (i = 0; i < 12; i++) {
        inode->i_block[i] = array[i];
        num_block_written++;
        if (num_block_written == array_size) {
            return;
        }
    }
    
    // If reach here, write into indirect_block
    // allocate a space for indirect_block
    int indirect_block_num = dalloc();
    if (indirect_block_num < 0) {
        exit(ENOSPC);
    }
    inode->i_block[12] = indirect_block_num;
    inode->i_blocks += 2;
    
    // Since disk size is 128kb, only consider the cases that
    //      there is only one indirect block.
    unsigned int *indirect_block = (unsigned int *)(disk + EXT2_BLOCK_SIZE * inode->i_block[12]);
    for (i = 0; i < EXT2_BLOCK_SIZE/4; i++) {
        indirect_block[i] = array[12 + i];
        num_block_written++;
        if (num_block_written == array_size) {
            return;
        }
    }
}

int ialloc(void) {
    if (gdt->bg_free_inodes_count == 0) {
        return -1;
    }
    
    unsigned char *inode_bitmap_ptr;
    int curr_byte = 0;
    int vaild_byte_count = sb->s_inodes_count/8;
    int curr_inode_num = 0;
    int i;
    
    while (curr_byte < vaild_byte_count) {
        inode_bitmap_ptr = inode_bitmap + curr_byte;
        for (i = 0; i < 8; i++) {
            curr_inode_num++;
            if (!((*inode_bitmap_ptr)>>i & 1)){
                // Set inode bitmap
                *inode_bitmap_ptr = *inode_bitmap_ptr | (1 << i);
                // Set gdt
                gdt->bg_free_inodes_count--;
                sb->s_free_inodes_count--;
                return curr_inode_num;
            }
            
        }
        curr_byte++;
    }
    
    return -1;
}

int dalloc(void) {
    if (gdt->bg_free_blocks_count == 0) {
        return -1;
    }
    
    unsigned char *block_bitmap_ptr;
    int curr_byte = 0;
    int vaild_byte_count = sb->s_blocks_count/8;
    int curr_inode_num = 0;
    int i;
    
    while (curr_byte < vaild_byte_count) {
        block_bitmap_ptr = block_bitmap + curr_byte;
        for (i = 0; i < 8; i++) {
            if (!((*block_bitmap_ptr)>>i & 1)){
                // Update block bitmap
                *block_bitmap_ptr = *block_bitmap_ptr | (1 << i);
                // Update gdt
                gdt->bg_free_blocks_count--;
                sb->s_free_blocks_count--;
                return curr_inode_num;
            }
            curr_inode_num++;
        }
        curr_byte++;
    }
    
    return -1;
}

void ifree(int inode_num) {
    unsigned char *ptr;
    int in_which_byte;
    int off;
    
    in_which_byte = inode_num/8;
    if (inode_num%8) {
        in_which_byte++;
        off = inode_num%8;
    } else {
        off = 8;
    }
    
    
    ptr = inode_bitmap + (in_which_byte - 1);
    *ptr = *ptr & ~(1<<(off - 1));
    
    //update superblock and gdt
    gdt->bg_free_inodes_count++;
    sb->s_free_inodes_count++;
}


void dfree(int block_num) {
    block_num++;
    
    unsigned char *ptr;
    int in_which_byte;
    int off;
    
    in_which_byte = block_num/8;
    if (block_num%8) {
        in_which_byte++;
        off = block_num%8;
    } else {
        off = 8;
    }
    
    
    ptr = block_bitmap + in_which_byte - 1;
    *ptr = *ptr & ~(1<<(off - 1));
    
    //update superblock and gdt
    gdt->bg_free_blocks_count++;
    sb->s_free_blocks_count++;
}

int restore_inode_bitmap(int inode_num) {
    unsigned char *ptr;
    int in_which_byte;
    int off;
    
    in_which_byte = inode_num/8;
    if (inode_num%8) {
        in_which_byte++;
        off = inode_num%8;
    } else {
        off = 8;
    }
    
    
    ptr = inode_bitmap + (in_which_byte - 1);
    
    // check if in use
    if ((*ptr)>>(off - 1) & 1) {
        return -1;
    }
    
    // mark it back to used
    
    *ptr = *ptr | (1<<(off - 1));
    
    //update superblock and gdt
    gdt->bg_free_inodes_count--;
    sb->s_free_inodes_count--;
    
    return 0;
}


int restore_block_bitmap(int block_num) {
    block_num++;
    
    unsigned char *ptr;
    int in_which_byte;
    int off;
    
    in_which_byte = block_num/8;
    if (block_num%8) {
        in_which_byte++;
        off = block_num%8;
    } else {
        off = 8;
    }
    
    
    ptr = block_bitmap + (in_which_byte - 1);
    
    // check if in use
    if ((*ptr)>>(off - 1) & 1) {
        return -1;
    }
    
    // mark it back to used
    
    *ptr = *ptr | (1<<(off - 1));
    
    //update superblock and gdt
    gdt->bg_free_blocks_count--;
    sb->s_free_blocks_count--;
    
    return 0;
    
}

int count_inode_bitmap() {
    unsigned char *inode_bitmap_ptr;
    int curr_byte = 0;
    int vaild_byte_count = sb->s_inodes_count/8;
    int result = 0;
    int i;
    
    while (curr_byte < vaild_byte_count) {
        inode_bitmap_ptr = inode_bitmap + curr_byte;
        for (i = 0; i < 8; i++) {
            if (!(*inode_bitmap_ptr>>i & 1)) {
                result++;
            }
            
        }
        curr_byte++;
    }
    
    return result;
}

int count_block_bitmap() {
    unsigned char *block_bitmap_ptr;
    int curr_byte = 0;
    int vaild_byte_count = sb->s_blocks_count/8;
    int result = 0;
    int i;
    
    while (curr_byte < vaild_byte_count) {
        block_bitmap_ptr = block_bitmap + curr_byte;
        for (i = 0; i < 8; i++) {
            if (!(*block_bitmap_ptr>>i & 1)){
                result++;
            }
            
        }
        curr_byte++;
    }
    
    return result;
}

void clear_end_slash(char *path) {
    if (path[strlen(path) - 1] == '/') {
        path[strlen(path) - 1] = '\0';
    }
}

void cut_path(char *path,
              char *parent,
              char *name)
{
    int i;
    // Find out the index of the last '/' in path;
    int last_slash_index = 0;
    for (i = 0; i < strlen(path); i++) {
        if (path[i] == '/') {
            last_slash_index = i;
        }
    }
    // Set parent path
    if (last_slash_index == 0) {
        parent[0] = '/';
        parent[1] = '\0';
    } else {
        memcpy(parent, path, last_slash_index);
        parent[last_slash_index] = '\0';
    }
    // Set name
    strncpy(name, path + last_slash_index + 1, EXT2_NAME_LEN);
}




int copy_to_inode_datablock(struct ext2_inode *dst_file_inode,
                            unsigned char *src_file,
                            int src_size)
{
    // allocate datablock space for cpy_file
    int dst_file_i_block_array_size = src_size/EXT2_BLOCK_SIZE;
    if (src_size%EXT2_BLOCK_SIZE) {
        dst_file_i_block_array_size++;
    }
    dst_file_inode->i_blocks = dst_file_i_block_array_size * 2; // set inode->blocks
    int *dst_file_i_block_array = malloc(sizeof(int) * dst_file_i_block_array_size);
    int i;
    for (i = 0; i < dst_file_i_block_array_size; i++) {
        dst_file_i_block_array[i] = dalloc();
        if (dst_file_i_block_array[i] < 0) {
            return ENOSPC;
        }
    }
    write_array_into_i_block(dst_file_inode, dst_file_i_block_array, dst_file_i_block_array_size);
    // do copy
    i = 0;
    while (i < dst_file_i_block_array_size) {
        unsigned char *src = src_file + EXT2_BLOCK_SIZE * i;
        unsigned char *dst = disk + EXT2_BLOCK_SIZE * dst_file_i_block_array[i];
        memcpy(dst, src, EXT2_BLOCK_SIZE);
        i++;
    }
    
    free(dst_file_i_block_array);
    return 0;
}

int new_inode(unsigned short type,
              unsigned int size){
    // allocate space in inode table
    int new_inode_num = ialloc();
    if (new_inode_num < 0) {
        return ENOSPC;
    }
    struct ext2_inode *new_inode = inode_table + (new_inode_num - 1);
    
    // build dst file inode
    new_inode->i_mode           = 0x0000 | type;
    new_inode->i_uid            = 0;
    new_inode->i_size           = size;
    new_inode->i_ctime          = 0;
    new_inode->i_mtime          = 0;
    new_inode->i_dtime          = 0;
    new_inode->i_gid            = 0;
    new_inode->i_links_count    = 0;
    new_inode->i_blocks         = 0;
    new_inode->i_flags          = 0;
    // block[15]
    new_inode->osd1             = 0;
    new_inode->i_generation     = 0;
    new_inode->i_file_acl       = 0;
    new_inode->i_dir_acl        = 0;
    // padding
    memset(new_inode->extra, 0, 3);
    
    return new_inode_num;
}

void free_inode(int inode_num){
    struct ext2_inode *inode = inode_table +  inode_num - 1;
    int *inode_datablock_array = read_i_block_into_array(inode);
    
    // set del time
    inode->i_dtime = (unsigned int)time(NULL);
    
    // Mark bitmap as free
    
    int i = 0;
    while (inode_datablock_array[i] != -1) {
        dfree(inode_datablock_array[i]);
        i++;
    }
    if (i >= 12) { // free indirect block
        dfree(inode->i_block[12]);
    }
    
    // free inode
    ifree(inode_num);
    
    free(inode_datablock_array);
}

int each_checker_rec(int curr_inode_num,
                     int parent_inode_num,
                     unsigned char *ft_type_ptr) {
    struct ext2_inode *curr_inode = inode_table + curr_inode_num - 1;
    int num_fixed = 0;
    char inode_type;
    char entry_type;
    
    // check if file type in dir_entry and inode consistent
    if ((curr_inode->i_mode & EXT2_S_IFLNK) == EXT2_S_IFLNK) {
        inode_type = 's';
    } else if (curr_inode->i_mode & EXT2_S_IFREG) {
        inode_type = 'f';
    } else if (curr_inode->i_mode & EXT2_S_IFDIR) {
        inode_type = 'd';
    } else {
        exit(-1);
    }
    
    switch (*ft_type_ptr) {
        case EXT2_FT_REG_FILE:
            entry_type = 'f';
            break;
        case EXT2_FT_DIR:
            entry_type = 'd';
            break;
        case EXT2_FT_SYMLINK:
            entry_type = 's';
            break;
        default:
            exit(-1);
            break;
    }
    
    if (entry_type != inode_type) {
        if (inode_type == 'f') {
            *ft_type_ptr = EXT2_FT_REG_FILE;
        } else if (inode_type == 'd') {
            *ft_type_ptr = EXT2_FT_DIR;
        } else if (inode_type == 's') {
            *ft_type_ptr = EXT2_FT_SYMLINK;
        }
        num_fixed++;
        printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", curr_inode_num);
    }
    
    // check if inode bitmap consistent
    // reuse function from restore
    if (restore_inode_bitmap(curr_inode_num) != -1) {
        num_fixed++;
        printf("Fixed: inode [%d] not marked as in-use\n", curr_inode_num);
    }
    
    // check if block bitmap consistent
    int *curr_inode_iblock_array = read_i_block_into_array(curr_inode);
    int i = 0;
    while (curr_inode_iblock_array[i] != -1) {
        if (restore_block_bitmap(curr_inode_iblock_array[i]) != -1) {
            num_fixed++;
            printf("Fixed: %d in-use data blocks not marked in data bitmap for inode: [%d]\n", curr_inode_iblock_array[i], curr_inode_num);
        }
        i++;
    }
    
    // check if dtime is 0
    if (curr_inode->i_dtime != 0) {
        curr_inode->i_dtime = 0;
        num_fixed++;
        printf("Fixed: valid inode marked for deletion: [%d]\n", curr_inode_num);
    }
    
    /*
     *  Base case curr_inode is regfile or symlinks
     */
    if (inode_type != 'd') { // not dir then is f or s
        return num_fixed;
    }
    
    /*
     *  Recursive part
     *  curr_inode is dir
     */
    i = 0;
    while (curr_inode_iblock_array[i] != -1) {
        unsigned char *curr_entry_data = disk + EXT2_BLOCK_SIZE * curr_inode_iblock_array[i];
        int curr_off = 0;
        struct ext2_dir_entry *entry;
        
        while (curr_off < EXT2_BLOCK_SIZE) {
            entry = (struct ext2_dir_entry *)(curr_entry_data + curr_off);
            if (entry->rec_len == EXT2_BLOCK_SIZE) {
                break;
            }
            if ((entry->inode != parent_inode_num) &&
                (entry->inode != curr_inode_num)) { // skip "." and ".." and "lost+found"
                num_fixed += each_checker_rec(entry->inode, curr_inode_num, &entry->file_type);
            }
            
            curr_off += entry->rec_len;
        }
        
        i++;
    }
    
    
    return num_fixed;
}

