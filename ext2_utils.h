
#ifndef ext2_utils_h
#define ext2_utils_h

#include <stdio.h>
#include "ext2.h"

#define DISK_SIZE 128

/*
 *  Init function, it *MUSE* be called before any of the rest utils function get called.
 */
void ext2_utils_init(void);

/*
 *  Return inode number if name match found in in_which_dir inode.
 *  Parameters:
 *      int in_which_dir    :  inode number for dir that search happen
 *      char *name          :  name of a file or a dir
 *  Return: int
 *      inode number of matched file or dir
 *      -1 for not found
 */
int get_inode_number_by_name(int in_which_dir, char *name);

/*
 *  Return inode number that ref by path.
 *      Vaild path: "/d1/d2/d3" and "/d1/d2/f1"
 *      Invaild path: "d1/d2/d3" or "/d1/d2/d3/"
 *  Return: int
 *      -1 if path not exist
 */
int get_inode_number_by_path(char *path);

/*
 *  Make a new dir in where dir inode.
 *  Parameters:
 *      int where           :   inode number for dir where new dir should be created.
 *      char *new_dir_name  :   name of the new dir
 *  Return: int
 *      If success return inode number of the new dir,
 *      if dir already exist in in_which_dir return -1;
 */
int inode_mkdir(int where, char *new_dir_name);

/*
 *  Append new entry to given dir entry.
 *  Parameters:
 *      unsigned char *entry_datablock  :   where new entry should be added
 *      unsigned int inode_num          :   inode number for new entry
 *      char *name                      :   name of the new entry
 *      unsigned char type              :   type of the new entry
 */
void add_to_dir_entry(struct ext2_inode *parent_inode,
                      unsigned int inode_num,
                      char *name,
                      unsigned char type);

/*
 *  Remove existed entry from a given dir entry.
 *  Parameters:
 *      int   parent_inode_num  :   where entry should be remove from
 *      char *name              :   name of the entry to remove
 *  Return : int
 *      -1 if no entry name in parent_inode were found
 *       0 if success
 */
int remove_from_dir_entry(struct ext2_inode *parent_inode,
                          char *name);

/*
 *  Restore entry bring removed by remove_from_dir_entry(),
 *      this function will search the "gaps" between each
 *      entry.
 *  - If inode being reused(del time == 0), return -1.
 *  - If inode's i_block[] were zeroed out, then recover a file as
 *      an empty file (no data blocks)
 *  Parameters:
 *      int   parent_inode_num  :   where entry should be remove from
 *      char *name              :   name of the entry to remove
 *  Return : int
 *      -1 if restore unsuccess
 *       0 if success
 */
int restore_from_dir_entry(struct ext2_inode *parent_inode,
                           char *name);

/*
 *  Init a datablock as dir_entry datablock
 *  Parameters:
 *      unsigned char *entry_datablock  :   pointer to the datablock to init
 *      int            self_inode_num   :   inode number of self dir inode
 *      int            parent_inode_num :   inode number of parent inode
 */
void init_dir_entry(unsigned char *entry_datablock,
                    int self_inode_num,
                    int parent_inode_num);

/*
 *  Allocate space on Inode table, return the allocated block number.
 *  Return: int
 *      inode number for new inode
 */
int ialloc(void);

/*
 *  Allocate space on data Blacks.
 *  Parameters:
 *      int size    :   numbers of block need to be allocated.
 *  Return: int
 *      block number that allocated.
 */
int dalloc(void);

/*
 *  Free inode bitmap for certain inode.
 */
void ifree(int inode_num);

/*
 *  Free block bitmap for certian block.
 */
void dfree(int block_num);

/*
 *  Restore inode bitmap
 *  If inode already reused return -1
 */
int restore_inode_bitmap(int inode_num);

/*
 *  Restore block bitmap
 *  If block already reused return -1
 */
int restore_block_bitmap(int block_num);

/*
 *  Count number of inode in bitmap mark as used
 */
int count_inode_bitmap(void);

/*
 *  Count number of block in bitmap mark as used
 */
int count_block_bitmap(void);

/*
 *  Read each block number in struct ext2_inode -> block[] into an array.
 *  Note. Indirect block number will not be include in the output array.
 *  Parameters:
 *      int inode_num   :   inode number
 *  Return: int *
 *      Array of block number in block[], -1 will be add to the end.
 */
int *read_i_block_into_array(struct ext2_inode *inode);

/*
 *  Write each block number in array to struct ext2_inode -> block[] .
 *  Parameters:
 *      int inode_num   :   inode number
 *      int *array      :   pointer to array
 *      int size        :   array size
 */
void write_array_into_i_block(struct ext2_inode *inode,
                              int *array,
                              int array_size);

/*
 *  Clear the slash in the end of path.
 *  Example.
 *      "/d1/d2/" -> "/d1/d2"
 */
void clear_end_slash(char *path);

/*
 *  cut path into two part.
 *  Example.
 *      "/d1/d2" -> parent: "/d1" , name: "d2"
 *      "/d1"    -> parent: "/"   , name: "d1"
 */
void cut_path(char *path,
              char *parent,
              char *name);

/*
 *  Copy data to data block of an inode.
 *  Parameters:
 *      struct ext2_inode * :   Copy to which inode
 *      unsigned char *     :   src file
 *      int                 :   size of src file
 *  Return : int
 *      ENOSPC if no enought space
 *      0      if success
 */

int copy_to_inode_datablock(struct ext2_inode *dst_file_inode,
                            unsigned char *src_file,
                            int src_size);

/*
 *  Create an inode.
 *  Parameters:
 *      unsigned short : inode type
 *      unsigned int   : file size
 *  Return : int
 *      ENOSPC if no enought space
 *      0      if success
 */

int new_inode(unsigned short type,
              unsigned int size);

/*
 *  Free inode. Mark inode as deleted.
 *  Parameters:
 *      int : which inode to free
 */
void free_inode(int inode_num);

/*
 *  Recursively check if any consistent occur in curr_inode_num.
 *  Parameters:
 *      int : number of inode of current file or dir
 *      int : number of inode of parent dir
 *      unsigned char * : pointer to current file or dir's entry's type
 *  Return:
 *      int : number of inconsistents fixed
 */

int each_checker_rec(int curr_inode_num,
                     int parent_inode_num,
                     unsigned char *ft_type_ptr);


#endif /* ext2_utils_h */
