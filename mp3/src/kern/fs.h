//           fs.h - File system interface
//          

#ifndef _FS_H_
#define _FS_H_

#include "io.h"
#include "thread.h"
#include "timer.h"
#include "plic.h"
#include "intr.h"

/* Macro definitions */
#define DATABLKSIZE 4096 // 4 kB is 4096 B
#define MAX_FILENAME_LEN 32 // the maximum length of filename in dentry is 32
#define MAX_OPENFILES 32 // Each task can have up to 32 open files
#define MAX_DENTRIES 63 // the file system can hold up to 63 files


/* File structures */

// directory entry
struct dentry {
    char filename[MAX_FILENAME_LEN]; // file name, up to 32 characters
    uint32_t inode_num; // refers to the index node specifying the file's size and location
    uint8_t reserved[28]; // 28 bytes reserved because of alignment
}__attribute((packed));
typedef struct dentry dentry_t;

// boot block (4kB)
struct bootblock {
    uint32_t num_dentries; // number of directory entries
    uint32_t num_inodes; // number of inodes
    uint32_t num_blks; // number of data blocks
    uint8_t reserved[52]; // 52 bytes reserved
    struct dentry dentries[MAX_DENTRIES]; // directory entries, has a maximum of 63 files
}__attribute((packed));
typedef struct bootblock bootblock_t;

// inodes (4kB per block)
struct inode{
    uint32_t len; // the size of the file in bytes
    uint32_t datablk_nums[1023]; // specify which data blocks this file is related to. There are (4096 - 4)/4 = 1023 blocks maximum.
}__attribute((packed));
typedef struct inode inode_t;

// data block (4kB per block) from mkfs.c
typedef struct data_block_t{
    uint8_t data[DATABLKSIZE];
}__attribute((packed)) data_block_t;

// file structure, contains the metadata that our filesystem interface needs so that it can read, write, etc. properly
struct file_struct {
    struct io_intf file_io_intf; // the io_intf associated with each file
    uint64_t file_pos; // keeps track of where the user is currently reading from / writing to in the file
    uint64_t file_size; // store the length of the file in bytes
    uint64_t inode_num; // inode number of this file
    uint64_t flags; // indicate this file is in use or not
}__attribute((packed));
typedef struct file_struct file_t;



extern char fs_initialized;

extern void fs_init(void);
extern int fs_mount(struct io_intf * blkio);
extern int fs_open(const char * name, struct io_intf ** ioptr);
extern void fs_close(struct io_intf* blkio);
extern long fs_write(struct io_intf* blkio, const void* buf, unsigned long n);
extern long fs_read(struct io_intf* blkio, void* buf, unsigned long n);
extern int fs_ioctl(struct io_intf* blkio, int cmd, void* arg);
extern int fs_getlen(file_t* fd, void* arg);
extern int fs_getpos(file_t* fd, void* arg);
extern int fs_setpos(file_t* fd, void* arg);
extern int fs_getblksz(file_t* fd, void* arg);

//           _FS_H_
#endif
