// //#include <stdio.h>
// #include <stdint.h>
// #include <unistd.h>
// #include <stdlib.h>
// #include <string.h>
// #include <fcntl.h>
// #include <assert.h>

#include <stddef.h>
// #include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdlib.h>

#include "io.h"
#include "fs.h"
#include "error.h"
#include "console.h"
#include "string.h"
#include "heap.h"
#include "lock.h"

/* Global Variables */
// the array ptr to file_ts
static file_t openfiles[MAX_OPENFILES]; // an array of open files
static struct lock openfile_lock; // each open file has a lock related
static bootblock_t boot_block; // the bootblock of the filesystem
static uint32_t N_inodes;
static uint32_t N_datablks;
//data_block_t* abs_blks; // the array to all of the blocks, including boot block, inode, data blocks.
static inode_t inodes[MAX_DENTRIES]; // an array of inodes
//data_block_t* datablocks; // an array of data blocks
// the file operations
static const struct io_ops fileio_ops = {
    .close = fs_close,
    .write = fs_write,
    .read = fs_read,
    .ctl = fs_ioctl
};
static struct io_intf* fs_blkio = NULL;

char fs_initialized = 0;

/**fs_init
 * input: none
 * output: none
 * Initializes the fs.
 */
void fs_init(){
    fs_initialized = 1;
}


/** fs_mount
 * Takes an io_intf* to the filesystem provider and sets up
 * the filesystem for future fs_open operations.
 * 
 * Input: struct io_intf* io
 * Output: int, indicates whether mount success.
 *      0 - success
 *      negative value - fail
 */
int fs_mount(struct io_intf * blkio){
    // Do the general checks
    // if(fs_initialized != 0){
    //     return -EBUSY; // has been initialized
    // }
    if(blkio == NULL){
        return -EINVAL; // invalid argument was passed in
    }
    if(blkio->ops == NULL){
        return -ENOTSUP; // operation not supported
    }
    if(blkio->ops->read == NULL){
        return -ENOTSUP; // operation not supported
    }

    
    // Read the boot block to get data
    uint64_t iopos = 0; // boot is at position 0
    if(ioseek(blkio, iopos) < 0){
        return -ENOTSUP; // eror in operation
    }

    long bootbytes = ioread_full(blkio, &boot_block, sizeof(bootblock_t));
    if(bootbytes != DATABLKSIZE)
        return -EIO;
    N_inodes = boot_block.num_inodes;
    N_datablks = boot_block.num_blks;

    // // initialize the array of all blocks
    // abs_blks = kmalloc((N_inodes+N_datablks)*sizeof(data_block_t));
    // if(abs_blks == NULL){
    //     return -ENOTSUP;
    // }

    
    //inodes = (inode_t*) abs_blks + sizeof(data_block_t);
    //datablocks = (data_block_t*) abs_blks + (1+N_inodes) * sizeof(data_block_t);

    iopos = iopos + sizeof(bootblock_t);
    if(ioseek(blkio, iopos) < 0){
        // deallocate the ptrs!
        //kfree(abs_blks);
        //inodes = NULL;
        //datablocks = NULL;
        return -ENOTSUP; // eror in operation
    }

    // initialize inodes
    long inodebytes = ioread_full(blkio, &inodes, boot_block.num_inodes * sizeof(inode_t));
    if(inodebytes != boot_block.num_inodes * DATABLKSIZE){
        //kfree(abs_blks);
        //inodes = NULL;
        //datablocks = NULL;
        return -EIO;
    }
    // data blocks not initialized.
    // set up the fs blkio ptr
    fs_blkio = blkio;

    // set up the openfiles
    for(uint32_t i=0; i<MAX_OPENFILES; i++){
        openfiles[i].file_io_intf.ops = &fileio_ops; // specify available operations
        openfiles[i].file_io_intf.refcnt = 0; // mark the reference count as 0
        openfiles[i].flags = 0; // mark as not used
    }
    // set up the lock
    char lock_name[15] = "openfile_lock";
    lock_init(&openfile_lock, lock_name);

    // initialization complete, mark as initialized
    fs_initialized = 1;
    return 0;
}



/**fs_open
 * Takes the name of the file to be opened, and
 * modify the given pointer to contain the io_intf of the file.
 * This function should also associate a specific file struct with the
 * file and mark as in use.
 */
int fs_open(const char* name, struct io_intf** io){
    if(name == NULL || io == NULL){
        return -EINVAL; // invalid input argument
    }
    // if(fs_initialized == 0){
    //     return -ENOENT; // file system not implemented yet
    // }

    uint32_t dentry_idx = 0;
    uint32_t num_dentries = boot_block.num_dentries;
    for(; dentry_idx < num_dentries; dentry_idx++){
        if(0 == strncmp(boot_block.dentries[dentry_idx].filename, name, MAX_FILENAME_LEN)){
            // The file is found.
            break;
        }
    }
    if(dentry_idx == num_dentries)
        return -ENOENT; // no such file or directory
    
    // Read the inode to know which inode it corresponds to
    uint32_t inode_num = boot_block.dentries[dentry_idx].inode_num;
    
    // Find a vacant place in the open files
    uint32_t openfile_idx = 0;
    for(; openfile_idx < MAX_OPENFILES; openfile_idx++){
        if((openfiles[openfile_idx].flags & 1) == 0){
            break; // the vacant place is found
        }
    }
    if (openfile_idx == MAX_OPENFILES){
        return -EBUSY; // resource busy
    }

    if(openfiles[openfile_idx].file_io_intf.refcnt)
        return -EBUSY;

    // set up the file struct
    openfiles[openfile_idx].file_io_intf.ops = &fileio_ops;
    // Initialize reference count;
    openfiles[openfile_idx].file_io_intf.refcnt = 1;
    kprintf("fs_open: refcnt of file in_intf at openfiles[%d] set to 1.\n", openfile_idx);
    openfiles[openfile_idx].file_pos = 0; // initialize the position to 0
    openfiles[openfile_idx].file_size = (uint64_t)inodes[inode_num].len; // filesize from inode
    openfiles[openfile_idx].inode_num = inode_num; // inode num just found above
    openfiles[openfile_idx].flags = 1; // mark as in use

    // modify the given ptr to contain io_intf of the file
    struct io_intf* thisintf = (struct io_intf*)(&openfiles[openfile_idx]);
    *io = thisintf - offsetof(file_t, file_io_intf);

    return openfile_idx;
}



/**fs_close
 * Marks the file struct associated with io as unused
 * Input: struct io_intf* io -- 
 */
void fs_close(struct io_intf* io) {
    if(io == NULL){
        return;
    }
    
    // file_t* thisfile = (file_t*)io - offsetof(file_t, file_io_intf);

    // find the file struct with io_intf == *io
    uint32_t openfile_idx = 0;
    for(; openfile_idx < MAX_OPENFILES; openfile_idx++){
        if(openfiles[openfile_idx].flags == 1 && &(openfiles[openfile_idx].file_io_intf) == io){
            break; // the file is found
        }
    }
    //thisfile->flags = 0; // Mark as not in use

    if (openfile_idx == MAX_OPENFILES){
        kprintf("fs_close: file not closed\n");
        return; // no such file or directory
    } 
    
    kprintf("fs_close: closed file at openfiles[%d].\n", openfile_idx);
    openfiles[openfile_idx].flags = 0;  // Mark as not in use
}




/**fs_write
 * Write n bytes from buf into the file associated with io. The size of the file should not change.
 * Update the metadata in the file struct as appropriate.
 */
long fs_write(struct io_intf* blkio, const void* buf, unsigned long n){
    // Do the general checks
    // if(fs_initialized == 0){
    //     return -ENOENT; // has not been initialized
    // }
    if(blkio == NULL || n == 0){
        return -EINVAL; // invalid argument was passed in
    }
    if(blkio->ops == NULL){
        return -ENOTSUP; // operation not supported
    }
    
    //file_t* thisfile = (file_t*)blkio - offsetof(file_t, file_io_intf);

    // find the file struct with io_intf == *io
    uint32_t openfile_idx = 0;
    for(; openfile_idx < MAX_OPENFILES; openfile_idx++){
        if(openfiles[openfile_idx].flags == 1 && &(openfiles[openfile_idx].file_io_intf) == blkio){
            break; // the file is found
        }
    }
    if (openfile_idx == MAX_OPENFILES){
        return -ENOENT; // no such file or directory
    }


    // if(thisfile->flags == 0){
    //     return -ENOENT; // the file struct is not in use
    // }
    if(buf == NULL){
        return -EINVAL; // invalid argument
    }
    
    // check if it can write
    uint64_t thisfile_size = openfiles[openfile_idx].file_size;
    if (openfiles[openfile_idx].file_pos >= thisfile_size){ // is already full
        return 0; // wrote 0 bytes
    }
    // can write at most (size - pos) bytes
    uint64_t bytes_to_write = (n+openfiles[openfile_idx].file_pos > thisfile_size) ? thisfile_size-openfiles[openfile_idx].file_pos : n;
    // compute the position where the write starts
    uint64_t bytes_written = 0;
    const char* thisbuff = (const char*)buf;
    uint64_t inode_idx = openfiles[openfile_idx].inode_num;

    // lock the file
    lock_acquire(&openfile_lock);

    while (bytes_written < bytes_to_write) {
        // Calculate block details
        uint64_t block_index = openfiles[openfile_idx].file_pos / DATABLKSIZE;
        uint64_t block_offset = openfiles[openfile_idx].file_pos % DATABLKSIZE;

        // Determine bytes that can be written in this iteration
        uint64_t bytes_avail = DATABLKSIZE - block_offset;
        uint64_t bytes_left = bytes_to_write - bytes_written;
        uint64_t bytes_to_process;
        if(bytes_left < bytes_avail){
            bytes_to_process = bytes_left;
        }else{
            bytes_to_process = bytes_avail;
        }

        // Compute block position and target address for writing
        uint64_t block_position = 1 + boot_block.num_inodes + inodes[inode_idx].datablk_nums[block_index]; // num of boot_block + num of inodes + block_index
        uint64_t real_address = (block_position * DATABLKSIZE) + block_offset; // real address is the actual position in the blocks

        // set the write position
        if (ioseek(fs_blkio, real_address) < 0) {
            // unlock the file
            lock_release(&openfile_lock);
            return -ENOTSUP; // if position set failure, return operation not supported
        }

        // Perform the write operation and handle its outcome
        
        long bytes_written_this = iowrite(fs_blkio, thisbuff, bytes_to_process);
        if (bytes_written_this <= 0) {
            // unlock the file
            lock_release(&openfile_lock);
            return (bytes_written_this == 0) ? bytes_written : -EIO; // End loop on EOF or return error on failure
        }

        // Update counters and pointers after a successful write
        bytes_written += bytes_written_this;
        thisbuff += bytes_written_this;
        openfiles[openfile_idx].file_pos += bytes_written_this;
    }

    // unlock the file
    lock_release(&openfile_lock);

    return bytes_written;

}



/**fs_read
 * Read n bytes from the file associated with io into buf.
 * Update metadata in the file struct as appropriate.
 */
long fs_read(struct io_intf* blkio, void* buf, unsigned long n){
    // Do the general checks
    // if(fs_initialized == 0){
    //     return -ENOENT; // has not been initialized
    // }
    if(blkio == NULL || n == 0){
        return -EINVAL; // invalid argument was passed in
    }
    if(blkio->ops == NULL){
        return -ENOTSUP; // operation not supported
    }
    
    //file_t* thisfile = (file_t*)blkio - offsetof(file_t, file_io_intf);

    // find the file struct with io_intf == *io
    uint32_t openfile_idx = 0;
    for(; openfile_idx < MAX_OPENFILES; openfile_idx++){
        if(openfiles[openfile_idx].flags == 1 && &(openfiles[openfile_idx].file_io_intf) == blkio){
            break; // the file is found
        }
    }
    if (openfile_idx == MAX_OPENFILES){
        return -ENOENT; // no such file or directory
    }


    // if(thisfile->flags == 0){
    //     return -ENOENT; // the file struct is not in use
    // }
    if(buf == NULL){
        return -EINVAL; // invalid argument
    }
    
    // check if it can write
    uint64_t thisfile_size = openfiles[openfile_idx].file_size;
    if (openfiles[openfile_idx].file_pos >= thisfile_size){ // is already full
        return 0; // wrote 0 bytes
    }
    // can write at most (size - pos) bytes
    uint64_t bytes_to_read = (n+openfiles[openfile_idx].file_pos > thisfile_size) ? thisfile_size-openfiles[openfile_idx].file_pos : n;
    // compute the position where the write starts
    uint64_t bytes_read = 0;
    const char* thisbuff = (const char*)buf;
    uint64_t inode_idx = openfiles[openfile_idx].inode_num;

    // lock the file
    lock_acquire(&openfile_lock);

    // read until full read is reached
    while (bytes_read < bytes_to_read) {
        // Calculate block details
        uint64_t block_index = openfiles[openfile_idx].file_pos / DATABLKSIZE;
        uint64_t block_offset = openfiles[openfile_idx].file_pos % DATABLKSIZE;

        // Determine bytes that can be written in this iteration
        uint64_t bytes_avail = DATABLKSIZE - block_offset; // available bytes in current datablock
        uint64_t bytes_left = bytes_to_read - bytes_read; // total bytes that still need to be read
        uint64_t bytes_to_process; // real bytes to process in this datablock
        if(bytes_left < bytes_avail){ // total bytes that still need to be read can be all read in this block
            bytes_to_process = bytes_left;
        }else{                          // more bytes is needed than can be read, read all bytes available for now
            bytes_to_process = bytes_avail;
        }

        // Compute datablock position and real address of the device for writing
        uint64_t datablock_position = 1 + boot_block.num_inodes + inodes[inode_idx].datablk_nums[block_index]; // num of boot_block + num of inodes + block_index
        uint64_t real_address = (datablock_position * DATABLKSIZE) + block_offset; // real address is the actual position in the blocks

        // set the read position
        if (ioseek(fs_blkio, real_address) < 0) {
            // unlock the file
            lock_release(&openfile_lock);
            return -ENOTSUP; // if position set failure, return operation not supported
        }

        // Perform the write operation. If 0 byte is read, read is complete; if negative value, error.
        long bytes_read_this = ioread_full(fs_blkio, (void*) thisbuff, bytes_to_process);
        if (bytes_read_this <= 0) {
            // unlock the file
            lock_release(&openfile_lock);
            return (bytes_read_this == 0) ? bytes_read : -EIO; // End loop on EOF or return error on failure
        }

        // Update counters and pointers after a successful read
        bytes_read += bytes_read_this;
        thisbuff += bytes_read_this;
        openfiles[openfile_idx].file_pos += bytes_read_this;
    }

    // unlock the file
    lock_release(&openfile_lock);


    return bytes_read;
}


/**fs_ioctl
 * Performs a device-specific function based on cmd.
 * Return values are stored in arg.
 */
int fs_ioctl(struct io_intf* blkio, int cmd, void* arg){
    // check if the passed in file is not assigned / used
    if(blkio == NULL){
        return -EINVAL; // invalid argument: io invalid
    }

    file_t* thisfile = (file_t*)blkio;
    if((thisfile->flags & 1) == 0){ // if this file is in use
        return -EINVAL; // invalid argument: file invalid
    }

    switch (cmd){
    case IOCTL_GETLEN:
        int length = fs_getlen(thisfile, arg);
        return length;
        break;
    case IOCTL_GETPOS:
        int pos = fs_getpos(thisfile, arg);
        return pos;
        break;
    case IOCTL_SETPOS:
        return fs_setpos(thisfile, arg);
        break;
    case IOCTL_GETBLKSZ:
        int blksize = fs_getblksz(thisfile, arg);
        return blksize;
        break;
    
    default:
        return -ENOTSUP; // operation not supported
        break;
    }
}



/** fs_getlen
 * return the length of the file in arg
 */
int fs_getlen(file_t * fd, void * arg) {
    // Do the general checks
    if(fd == NULL || arg == NULL){
        return -EINVAL; // invalid argument was passed into a function
    }

    uint64_t size = fd->file_size;
    *(uint64_t*)arg = size;
    return 0;
}



/** fs_getpos
 * return the current position in the file in arg
 */
int fs_getpos(file_t * fd, void * arg) {
    // Do the general checks
    if (fd == NULL || arg == NULL) {
        return -EINVAL; // invalid argument was passed into a function
    }

    uint64_t pos = fd->file_pos;
    *(uint64_t*)arg = pos;
    return 0;
}



/** fs_setpos
 * set the current position in the file
 */
int fs_setpos(file_t * fd, void * arg) {
    // Do the general checks
    if (fd == NULL || arg == NULL) {
        return -EINVAL; // invalid argument was passed into a function
    }

    uint64_t newpos = *(uint64_t*)arg;
    if (newpos > fd->file_size) {
        return -EINVAL; // out of bound, invalid argument was passed into a function
    }

    fd->file_pos = newpos;
    return 0;
}



/**fs_getblksz
 * return the block size of the file system in arg
 */
int fs_getblksz(file_t * fd, void * arg) {
    // Do the general checks
    if (fd == NULL || arg == NULL) {
        return -EINVAL; // invalid argument was passed into a function
    }
    
    *(uint64_t*)arg = DATABLKSIZE;
    return 0;
}

