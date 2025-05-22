//           vioblk.c - VirtIO serial port (console)
//          

#include "virtio.h"
#include "intr.h"
#include "halt.h"
#include "heap.h"
#include "io.h"
#include "device.h"
#include "error.h"
#include "string.h"
#include "thread.h"
#include "lock.h"

//           COMPILE-TIME PARAMETERS
//          

#define VIOBLK_IRQ_PRIO 1



//           INTERNAL CONSTANT DEFINITIONS
//          

//           VirtIO block device feature bits (number, *not* mask)

#define VIRTIO_BLK_F_SIZE_MAX       1
#define VIRTIO_BLK_F_SEG_MAX        2
#define VIRTIO_BLK_F_GEOMETRY       4
#define VIRTIO_BLK_F_RO             5
#define VIRTIO_BLK_F_BLK_SIZE       6
#define VIRTIO_BLK_F_FLUSH          9
#define VIRTIO_BLK_F_TOPOLOGY       10
#define VIRTIO_BLK_F_CONFIG_WCE     11
#define VIRTIO_BLK_F_MQ             12
#define VIRTIO_BLK_F_DISCARD        13
#define VIRTIO_BLK_F_WRITE_ZEROES   14
#define VIRTIO_F_NOTIFICATION_DATA  38

//           INTERNAL TYPE DEFINITIONS

#define USED_BUFFER_NOTICE 0x1
#define CONFIG_CHANGE_NOTICE 0x2
//          

//           All VirtIO block device requests consist of a request header, defined below,
//           followed by data, followed by a status byte. The header is device-read-only,
//           the data may be device-read-only or device-written (depending on request
//           type), and the status byte is device-written.

struct vioblk_request_header {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

//           Request type (for vioblk_request_header)

#define VIRTIO_BLK_T_IN             0
#define VIRTIO_BLK_T_OUT            1

//           Status byte values

#define VIRTIO_BLK_S_OK         0
#define VIRTIO_BLK_S_IOERR      1
#define VIRTIO_BLK_S_UNSUPP     2

//           Main device structure.
//          
//           FIXME You may modify this structure in any way you want. It is given as a
//           hint to help you, but you may have your own (better!) way of doing things.

struct vioblk_device {
    volatile struct virtio_mmio_regs * regs;
    struct io_intf io_intf;
    uint16_t instno;
    uint16_t irqno;
    int8_t opened;
    int8_t readonly;

    //           optimal block size
    uint32_t blksz;
    //           current position
    uint64_t pos;
    //           sizeo of device in bytes
    uint64_t size;
    //           size of device in blksz blocks
    uint64_t blkcnt;

    struct {
        //           signaled from ISR
        struct condition used_updated;

        //           We use a simple scheme of one transaction at a time.

        union {
            struct virtq_avail avail;
            char _avail_filler[VIRTQ_AVAIL_SIZE(1)];
        };

        union {
            volatile struct virtq_used used;
            char _used_filler[VIRTQ_USED_SIZE(1)];
        };

        //           The first descriptor is an indirect descriptor and is the one used in
        //           the avail and used rings. The second descriptor points to the header,
        //           the third points to the data, and the fourth to the status byte.

        struct virtq_desc desc[4];
        struct vioblk_request_header req_header;
        uint8_t req_status; 
    } vq;

    //           Block currently in block buffer
    uint64_t bufblkno;
    //           Block buffer
    char * blkbuf;
};

static struct lock vioblk_lock;

//           INTERNAL FUNCTION DECLARATIONS
//          

static int vioblk_open(struct io_intf ** ioptr, void * aux);

static void vioblk_close(struct io_intf * io);

static long vioblk_read (
    struct io_intf * restrict io,
    void * restrict buf,
    unsigned long bufsz);

static long vioblk_write (
    struct io_intf * restrict io,
    const void * restrict buf,
    unsigned long n);

static int vioblk_ioctl (
    struct io_intf * restrict io, int cmd, void * restrict arg);

static void vioblk_isr(int irqno, void * aux);
//           IOCTLs

static int vioblk_getlen(const struct vioblk_device * dev, uint64_t * lenptr);
static int vioblk_getpos(const struct vioblk_device * dev, uint64_t * posptr);
static int vioblk_setpos(struct vioblk_device * dev, const uint64_t * posptr);
static int vioblk_getblksz (
    const struct vioblk_device * dev, uint32_t * blkszptr);

//           EXPORTED FUNCTION DEFINITIONS
//          
/**
 * @brief Initializes the VirtIO block device for I/O operations.
 * 
 * This function sets up the necessary I/O operations for the VirtIO block device, negotiates 
 * feature bits, and initializes the VirtQueue for data transfer. It registers the interrupt 
 * service routine and the device in the system.
 * 
 * @param regs Pointer to the VirtIO MMIO register structure for the block device.
 * @param irqno The interrupt request number associated with the block device.
 * 
 * @details 
 * - The function begins by resetting the device and acknowledging the driver.
 * - It then negotiates required and optional features.
 * - The block size is read from the device configuration if available, defaulting to 512 otherwise.
 * - Allocates and initializes the `vioblk_device` structure.
 * - Sets up a VirtQueue with indirect descriptors, establishing descriptors for the header, 
 *   buffer, and status byte.
 * - Registers the interrupt service routine (`vioblk_isr`) and makes the device available in 
 *   the system with `device_register`.
 */
void vioblk_attach(volatile struct virtio_mmio_regs * regs, int irqno) {
    //           FIXME add additional declarations here if needed
    // declaration for the operation struct
    	static const struct io_ops vioblk_ops = {
		.close = vioblk_close,
		.read = vioblk_read,
		.write = vioblk_write,
        .ctl = vioblk_ioctl
	};



    virtio_featset_t enabled_features, wanted_features, needed_features;
    struct vioblk_device * dev;
    uint_fast32_t blksz;
    int result;

    assert (regs->device_id == VIRTIO_ID_BLOCK);

    regs->status = 0; // clear the status bit for reset the device
    __sync_synchronize();
    regs->status |= VIRTIO_STAT_ACKNOWLEDGE;  //signal driver for the device
    __sync_synchronize();
    //           Signal device that we found a driver

    regs->status |= VIRTIO_STAT_DRIVER;
    //           fence o,io
    __sync_synchronize();

    //           Negotiate features. We need:
    //            - VIRTIO_F_RING_RESET and
    //            - VIRTIO_F_INDIRECT_DESC
    //           We want:
    //            - VIRTIO_BLK_F_BLK_SIZE and
    //            - VIRTIO_BLK_F_TOPOLOGY.

    virtio_featset_init(needed_features);
    virtio_featset_add(needed_features, VIRTIO_F_RING_RESET);
    virtio_featset_add(needed_features, VIRTIO_F_INDIRECT_DESC);
    virtio_featset_init(wanted_features);
    virtio_featset_add(wanted_features, VIRTIO_BLK_F_BLK_SIZE);
    virtio_featset_add(wanted_features, VIRTIO_BLK_F_TOPOLOGY);
    result = virtio_negotiate_features(regs,
        enabled_features, wanted_features, needed_features);

    if (result != 0) {
        kprintf("%p: virtio feature negotiation failed\n", regs);
        return;
    }

    //           If the device provides a block size, use it. Otherwise, use 512.

    if (virtio_featset_test(enabled_features, VIRTIO_BLK_F_BLK_SIZE))
        blksz = regs->config.blk.blk_size;
    else
        blksz = 512;

    debug("%p: virtio block device block size is %lu", regs, (long)blksz);

    //           Allocate initialize device struct

    dev = kmalloc(sizeof(struct vioblk_device) + blksz);
    memset(dev, 0, sizeof(struct vioblk_device));

    //           FIXME Finish initialization of vioblk device here
    // 
    // set the corresponding value for the vioblk device
    dev->regs = regs;
    dev->irqno = irqno;
    dev->blksz = blksz;
    dev->io_intf.ops = &vioblk_ops;
    dev->blkbuf = kmalloc(blksz);
    dev->opened = 0;
    dev->pos = 0;
    dev->bufblkno = 0; // set the current block number to a impossibel value

    // read capacity from device configuration
    dev->blkcnt = regs->config.blk.capacity;
    dev->size = dev->blkcnt * dev->blksz;    

    if(virtio_featset_test(enabled_features, VIRTIO_BLK_F_CONFIG_WCE))
    {
        //set the writebach cache if supported
       regs->config.blk.writeback = 1;
    }

    // initialize the condition variable
    condition_init(&dev->vq.used_updated,"used_updated");

    // fills out the descriptors
    //           The first descriptor is an indirect descriptor and is the one used in
    //           the avail and used rings. The second descriptor points to the header,
    //           the third points to the data, and the fourth to the status byte.
    // first desc, an indirect descriptor
    dev->vq.desc[0].addr = (uint64_t)&dev->vq.desc[1]; // pointer to the second desc
    dev->vq.desc[0].len = sizeof(dev->vq.desc) * 3; // three desc length
    dev->vq.desc[0].flags = VIRTQ_DESC_F_INDIRECT; // set flag as indirect desc

    // second desc, pointer to header
    dev->vq.desc[1].addr = (uint64_t)&dev->vq.req_header; // address of header
    dev->vq.desc[1].len = sizeof(dev->vq.req_header); // length of the corresponding part
    dev->vq.desc[1].flags = VIRTQ_DESC_F_NEXT; // flag for next desc
    dev->vq.desc[1].next = 1;

    // third desc, pointer to buffer
    dev->vq.desc[2].addr = (uint64_t)dev->blkbuf;   //address of buffer
    dev->vq.desc[2].len = dev->blksz;   // buffer size
    dev->vq.desc[2].flags = VIRTQ_DESC_F_NEXT; // have next desc
    dev->vq.desc[2].next = 2;

    // fourth desc, pointer to status byte
    dev->vq.desc[3].addr = (uint64_t)&dev->vq.req_status;   //address of status
    dev->vq.desc[3].len = sizeof(uint8_t);  //length for status
    dev->vq.desc[3].flags = VIRTQ_DESC_F_WRITE; // canbe write
    dev->vq.desc[3].next = 0; // setting to 0 for the end

    // attach the virtq
    uint16_t qid = 0; //0 for the main input output queue for block device

    // clear the buffer place for vq
    memset(&dev->vq.avail, 0, sizeof(dev->vq.avail));
    memset((void *)&dev->vq.used, 0, sizeof(dev->vq.used));

    // Set up the virtqueue
    regs->queue_sel = 0; // Select queue 0
    // check with the queue_num_max
    if (regs->queue_num_max == 0) {
        kprintf("Device %p reports queue size 0\n", regs);
        return;
    }
    if (regs->queue_num_max < 1) {
        kprintf("Device %p reports insufficient queue size %d\n", regs, regs->queue_num_max);
        return;
    }
    regs->queue_num = 1; // We use a queue size of 1

    // set the feature ok bit
    regs->status |= VIRTIO_STAT_FEATURES_OK;
    __sync_synchronize();

    // check for the feature ok bit
    if ((regs->status & VIRTIO_STAT_FEATURES_OK) == 0)
    {
        kprintf("%p: virtio feature negotiation failed\n", regs);
        return;
    }

    regs->queue_sel = 0;
    __sync_synchronize();

    virtio_attach_virtq(regs,qid,1,(uint64_t)&dev->vq.desc[0],(uint64_t)&dev->vq.used,(uint64_t)&dev->vq.avail);// attach virtq for device

    //signal that the queue is ready
    regs->queue_ready = 1;
    __sync_synchronize();

    // register the interrupt service routine
	intr_register_isr(irqno, VIOBLK_IRQ_PRIO, vioblk_isr, dev);

    // register the device
    int instno = device_register("blk",&vioblk_open,dev);
    dev->instno = instno;

    regs->status |= VIRTIO_STAT_DRIVER_OK;    
    //           fence o,oi
    __sync_synchronize();
}
/**
 * @brief Opens the VirtIO block device and prepares it for I/O operations.
 * 
 * This function enables the VirtQueue and interrupt line for the VirtIO block device,
 * ensuring it is ready for read and write operations. It returns the I/O operations
 * associated with the device.
 * 
 * @param ioptr Double pointer to the `io_intf` structure where the I/O operations will be set.
 * @param aux Pointer to auxiliary data, specifically the `vioblk_device` structure.
 * 
 * @return int Returns 0 on success. Returns -EBUSY if the device is already open, 
 *         or -1 if `queue_num_max` is not set.
 * 
 * @details 
 * - Checks if the device is already open; if so, returns -EBUSY.
 * - Validates that `queue_num_max` is properly set in the device’s registers.
 * - Enables the VirtQueue for data transfers and activates the interrupt line for the device.
 * - Sets the `ioptr` to the device’s `io_intf` structure and marks the device as open.
 */
int vioblk_open(struct io_intf ** ioptr, void * aux) {
    //           FIXME your code here
    // get the device pointer
    struct vioblk_device * const dev = aux;

    // lock initialization
    lock_init(&vioblk_lock, "vioblk_lock");

    // if the device be opened return busy
    if (dev->opened) return -EBUSY;

    // if the I/O interface has been referred
    if (dev->io_intf.refcnt)
		return -EBUSY;

    // check with the queue_num_max
    if (!dev->regs->queue_num_max){
        kprintf("%p: queue_num_max set faliure\n", dev->regs);
        return -1;
    }
    // enable the virtq
    int qid = 0; //qid for main input output queue
    virtio_enable_virtq(dev->regs,qid);

    // enable the interrupt line for virtio device
    intr_enable_irq(dev->irqno);

    // return the io operation
    *ioptr = &dev->io_intf;
    // Set the reference count to 1(when initialization)
    dev->io_intf.refcnt = 1;
    // set the opened flag to 1
	dev->opened = 1;

	return 0;
}

//           Must be called with interrupts enabled to ensure there are no pending
//           interrupts (ISR will not execute after closing).
/**
 * @brief Closes the VirtIO block device and resets its VirtQueue.
 * 
 * This function disables the VirtQueue and interrupts associated with the VirtIO block device, 
 * and resets flags and indices in the VirtQueue to prepare for future operations.
 * 
 * @param io Pointer to the `io_intf` structure representing the device's I/O interface.
 * 
 * @details 
 * - Retrieves the `vioblk_device` structure from the `io` pointer.
 * - Checks if the device is already closed; if so, it exits without action.
 * - Resets the VirtQueue flags and indices.
 * - Calls `virtio_reset_virtq` to reset the VirtQueue.
 * - Disables interrupts for the device.
 * - Sets the `opened` flag to 0, marking the device as closed.
 */
void vioblk_close(struct io_intf * io) {
    //           FIXME your code here
    // get the dev pointer from io
    struct vioblk_device *dev = (struct vioblk_device *)((char *)io - offsetof(struct vioblk_device, io_intf));

    // check whether the device is opened
    if (!dev->opened) {
        return; // Device is not open; nothing to close
    }

    //reset the qid
    int qid = 0; //qid for block device input and output

    //regs->queue_ready = 0; //may need to set ready to 0 #########
    // reset the corresponding flags
    dev->vq.avail.flags = VIRTQ_AVAIL_F_NO_INTERRUPT;
    dev->vq.used.flags = VIRTQ_USED_F_NO_NOTIFY;
    // clean the vq index
    dev->vq.avail.idx = 0;
    dev->vq.used.idx = 0;
    virtio_reset_virtq(dev->regs,qid);
    //memset(avail, 0, VIRTQ_AVAIL_SIZE(queue_size));
    //memset(used, 0, VIRTQ_USED_SIZE(queue_size));

    // disable the interrupt from device
    intr_disable_irq(dev->irqno);

    dev->opened = 0; // Mark the device as closed
}

/**
 * @brief Reads data from the VirtIO block device into a user-provided buffer.
 * 
 * This function reads a specified number of bytes from the device, handling alignment with 
 * block boundaries and notifying the device for each read request. The function blocks until 
 * the requested data is available, or until it reaches the end of the file (EOF).
 * 
 * @param io Pointer to the `io_intf` structure representing the device's I/O interface.
 * @param buf Pointer to the user-provided buffer where the read data will be stored.
 * @param bufsz The number of bytes to read into the buffer.
 * 
 * @return long The total number of bytes successfully read from the device, or a negative error code on failure.
 * 
 * @retval 0 if `bufsz` is zero or if the end of the device's data (EOF) is reached.
 * @retval -EINVAL if `buf` is NULL.
 * @retval -EIO if there is an error accessing the device position.
 * 
 * @details 
 * - Retrieves the current position within the device. If the position is past the end of the file, returns 0.
 * - Adjusts the read size if it exceeds the remaining data up to EOF.
 * - Clears the device's block buffer, then prepares and submits read requests in block-sized chunks.
 * - Notifies the device of each read request and waits for completion.
 * - Copies data from the device’s buffer into the user’s buffer, managing block boundaries and alignment.
 * - Updates the current position after each read and handles partial reads if necessary.
 */
long vioblk_read (
    struct io_intf * restrict io,
    void * restrict buf,
    unsigned long bufsz)
{
    // Get the device pointer from io
    struct vioblk_device *dev = (struct vioblk_device *)((char *)io - offsetof(struct vioblk_device, io_intf));

    // Check if the buffer size is 0
    if (bufsz == 0) return 0;

    // Check if the buffer is valid
    if (buf == NULL) return -EINVAL;

    // Get the current position
    uint64_t pos;
    if (vioblk_getpos(dev, &pos) != 0) {
        return -EIO;
    }

    // Check if we've reached the end of the file
    if (pos >= dev->size) return 0; // EOF

    lock_acquire(&vioblk_lock);

    // Adjust bufsz if too large
    if (pos + bufsz > dev->size)
        bufsz = dev->size - pos; // Adjust to read up to EOF

    // Initialize the counter and the pointer used in the loop
    unsigned long total_read = 0;
    char *p = buf;

    // Calculate the number of blocks to read
    uint64_t blknum = (pos % dev->blksz + bufsz) / dev->blksz;
    uint64_t blkrem = (pos % dev->blksz + bufsz) % dev->blksz;
    // if their is remainder, need to read one more block
    if (blkrem != 0) blknum++;

    // Clear data buffer
    memset(dev->blkbuf, 0, dev->blksz);

    uint64_t read_rem = bufsz;

    while(read_rem > 0) {
        // Prepare the request header
        dev->vq.req_header.type = VIRTIO_BLK_T_IN; // Read request, for device to write in
        dev->vq.req_header.reserved = 0;
        dev->vq.req_header.sector = pos / dev->blksz;

        // Set the descriptor flags
        dev->vq.desc[2].flags |= VIRTQ_DESC_F_WRITE; // Device writes to this buffer

        // Set up the avail ring
        dev->vq.avail.ring[dev->vq.avail.idx % 1] = 0; // Descriptor index 0, the length of queue is 1
        dev->vq.avail.idx++;

        __sync_synchronize();

        // Notify the device
        int qid = 0; // 0 for main input and output
        virtio_notify_avail(dev->regs, qid);

        // Wait for the device to process the request
        int s = intr_disable();
        while (dev->vq.used.idx != dev->vq.avail.idx)
            condition_wait(&dev->vq.used_updated);
        intr_restore(s);

        // Calculate the amount to copy
        unsigned long to_copy;
        uint64_t blk_offset = pos % dev->blksz;
        if (read_rem + blk_offset < dev->blksz) {
            to_copy = read_rem;
        } else {
            to_copy = dev->blksz - blk_offset;
        }

        // Copy data from blkbuf to user buffer
        memcpy(p, dev->blkbuf + blk_offset, to_copy);

        // Update the related parameters
        p += to_copy;
        total_read += to_copy;
        read_rem -= to_copy;
        pos += to_copy;

        // Update the device position
        if (vioblk_setpos(dev, &pos) != 0) {
            lock_release(&vioblk_lock);
            return -EIO;
        }
    }
    lock_release(&vioblk_lock);
    return total_read;
}

/**
 * @brief Writes data from a user-provided buffer to the VirtIO block device.
 * 
 * This function writes a specified number of bytes to the device, handling alignment with 
 * block boundaries. If necessary, it reads the existing block data to prevent partial overwrites 
 * and then writes the updated data. The function blocks until the data is successfully written.
 * 
 * @param io Pointer to the `io_intf` structure representing the device's I/O interface.
 * @param buf Pointer to the user-provided buffer containing the data to be written.
 * @param bufsz The number of bytes to write from the buffer to the device.
 * 
 * @return long The total number of bytes successfully written to the device, or a negative error code on failure.
 * 
 * @retval 0 if `bufsz` is zero.
 * @retval -EINVAL if `buf` is NULL.
 * @retval -EIO if there is an error accessing the device position or if the device returns an error status.
 * 
 * @details 
 * - Retrieves the current position within the device. Adjusts `bufsz` if it exceeds the remaining device size.
 * - Aligns writes to the device’s block boundaries, reading existing data for partial blocks if necessary.
 * - Prepares and submits write requests using the `VIRTIO_BLK_T_OUT` type.
 * - Notifies the device of each write request and waits for completion.
 * - Handles potential I/O errors by checking the status of each request.
 * - Updates the current position and total written count after each block is processed.
 */
long vioblk_write (
    struct io_intf * restrict io,
    const void * restrict buf,
    unsigned long bufsz)
{
    // Get the device pointer
    struct vioblk_device *dev = (struct vioblk_device *)((char *)io - offsetof(struct vioblk_device, io_intf));

    // Chech the bufsz if it is zero
    if (bufsz == 0) return 0;

    // Check the validity for buffer
    if (buf == NULL) return -EINVAL;

    // get the recent position
    uint64_t pos;
    if (vioblk_getpos(dev, &pos) != 0) {
        return -EIO;
    }

    lock_acquire(&vioblk_lock);
    // if the size is too large than adjust the size
    if (pos + bufsz > dev->size)
        bufsz = dev->size - pos; // adjust to end of deive

    // Initialize the pointer and counter
    unsigned long total_written = 0;
    const char *p = buf;

    // calculate the block that need to write
    uint64_t blknum = (pos % dev->blksz + bufsz) / dev->blksz;
    uint64_t blkrem = (pos % dev->blksz + bufsz) % dev->blksz;
    // If there is remainder, then one additional block need to read
    if (blkrem != 0) blknum++;

    uint64_t write_rem = bufsz;

    // When there is still need write
    while (write_rem > 0)
    {
        // calculate block offset
        uint64_t blk_offset = pos % dev->blksz;

        // Calculate the byte number for copy
        unsigned long to_copy = (write_rem + blk_offset < dev->blksz) ? write_rem : (dev->blksz - blk_offset);

        // if the block is not full copy then need to read first
        if (blk_offset != 0 || to_copy < dev->blksz)
        {
            // ready for the read request header
            dev->vq.req_header.type = VIRTIO_BLK_T_IN; // read request
            dev->vq.req_header.reserved = 0;
            dev->vq.req_header.sector = pos / dev->blksz;

            // Set the descriptor flags
            dev->vq.desc[2].flags |= VIRTQ_DESC_F_WRITE; // Device writes to this buffer

            // Set up the avail ring
            dev->vq.avail.ring[dev->vq.avail.idx % 1] = 0; // Descriptor index 0, the length of queue is 1
            dev->vq.avail.idx++;

            __sync_synchronize();

            // notify the device
            int qid = 0;
            virtio_notify_avail(dev->regs, qid);

            // Wait for the device to handle the request
            int s = intr_disable();
            uint16_t last_used_idx = dev->vq.used.idx;
            while (dev->vq.used.idx == last_used_idx)
                condition_wait(&dev->vq.used_updated);
            intr_restore(s);

            // check with the status of the request
            if (dev->vq.req_status != VIRTIO_BLK_S_OK)
            {
                lock_release(&vioblk_lock);
                return -EIO;
            }
        }
        else
        {
            // if the offset is 0
            memset(dev->blkbuf, 0, dev->blksz);
        }

        // copy the data to block buffer
        memcpy(dev->blkbuf + blk_offset, p, to_copy);

        // Ready for the write request
        dev->vq.req_header.type = VIRTIO_BLK_T_OUT; // write request
        dev->vq.req_header.reserved = 0;
        dev->vq.req_header.sector = pos / dev->blksz;

        // Set the descriptor flags
        dev->vq.desc[2].flags &= ~VIRTQ_DESC_F_WRITE; // Device reads this buffer

        // set the available ring
        dev->vq.avail.ring[dev->vq.avail.idx % 1] = 0; // 1 is the length of queue
        dev->vq.avail.idx++;

        __sync_synchronize();

        // notify the device
        int qid = 0;        // 0 for the main input and output queue
        virtio_notify_avail(dev->regs, qid);

        // wait for device to handle the request
        int s = intr_disable();
        uint16_t last_used_idx = dev->vq.used.idx;
        while (dev->vq.used.idx == last_used_idx)
            condition_wait(&dev->vq.used_updated);
        intr_restore(s);

        // check with the request status
        if (dev->vq.req_status != VIRTIO_BLK_S_OK) {
            lock_release(&vioblk_lock);
            return -EIO;
        }

        // update the parameter
        p += to_copy;
        total_written += to_copy;
        write_rem -= to_copy;
        pos += to_copy;

        // update the device position
        if (vioblk_setpos(dev, &pos) != 0) {
            lock_release(&vioblk_lock);
            return -EIO;
        }
    }
    lock_release(&vioblk_lock);
    return total_written;
}


int vioblk_ioctl(struct io_intf * restrict io, int cmd, void * restrict arg) {
    struct vioblk_device * const dev = (void*)io -
        offsetof(struct vioblk_device, io_intf);
    
    trace("%s(cmd=%d,arg=%p)", __func__, cmd, arg);
    
    switch (cmd) {
    case IOCTL_GETLEN:
        return vioblk_getlen(dev, arg);
    case IOCTL_GETPOS:
        return vioblk_getpos(dev, arg);
    case IOCTL_SETPOS:
        return vioblk_setpos(dev, arg);
    case IOCTL_GETBLKSZ:
        return vioblk_getblksz(dev, arg);
    default:
        return -ENOTSUP;
    }
}

/**
 * @brief Interrupt Service Routine (ISR) for the VirtIO block device.
 * 
 * This function handles interrupts from the VirtIO block device, acknowledging configuration 
 * changes or used buffer notifications. It broadcasts a signal to wake any waiting threads 
 * when a used buffer notification is received.
 * 
 * @param irqno The interrupt request number associated with this ISR.
 * @param aux Pointer to auxiliary data, specifically the `vioblk_device` structure for the device.
 * 
 * @details 
 * - Reads the interrupt status from the device registers to determine the cause of the interrupt.
 * - If a configuration change is detected, it acknowledges the `CONFIG_CHANGE_NOTICE`.
 * - If a used buffer notification is detected, it broadcasts a condition to wake threads waiting 
 *   on the `used_updated` condition variable and acknowledges the `USED_BUFFER_NOTICE`.
 * - Synchronizes memory to ensure changes take effect immediately.
 */
void vioblk_isr(int irqno, void * aux) {
    //           FIXME your code here
    struct vioblk_device *dev = aux;

    // fetch the interrupt status
    uint32_t intr_status = dev->regs->interrupt_status;
    
    // handle with the configuration
    if(intr_status & CONFIG_CHANGE_NOTICE)
    {
        dev->regs->interrupt_ack |= CONFIG_CHANGE_NOTICE;
    }
    //handle with the used buffer
    else if(intr_status & USED_BUFFER_NOTICE)
    {
        condition_broadcast(&(dev->vq.used_updated));
        dev->regs->interrupt_ack |= USED_BUFFER_NOTICE;
    }
    __sync_synchronize();
}

/**
 * @brief Retrieves the size of the VirtIO block device in bytes.
 * 
 * This function obtains the total size of the device and stores it in the provided pointer.
 * 
 * @param dev Pointer to the `vioblk_device` structure representing the block device.
 * @param lenptr Pointer to a `uint64_t` where the device size (in bytes) will be stored.
 * 
 * @return int Returns 0 on success. Returns -EINVAL if `lenptr` is NULL.
 * 
 * @details 
 * - Validates `lenptr` to ensure it is a valid pointer.
 * - Stores the total device size, as defined in `dev->size`, in `lenptr`.
 */
int vioblk_getlen(const struct vioblk_device * dev, uint64_t * lenptr) {
    //           FIXME your code here
    
    // check for the pointer validity
    if (!lenptr) return -EINVAL;
    // get the size for length
    *lenptr = dev->size;
    return 0;
}

/**
 * @brief Retrieves the current position within the VirtIO block device.
 * 
 * This function obtains the current read/write position of the device and stores it in the provided pointer.
 * 
 * @param dev Pointer to the `vioblk_device` structure representing the block device.
 * @param posptr Pointer to a `uint64_t` where the current position will be stored.
 * 
 * @return int Returns 0 on success. Returns -EINVAL if `posptr` is NULL.
 * 
 * @details 
 * - Validates `posptr` to ensure it is a valid pointer.
 * - Stores the current position within the device, as defined in `dev->pos`, in `posptr`.
 */
int vioblk_getpos(const struct vioblk_device * dev, uint64_t * posptr) {
    //           FIXME your code here

    // check for pointer validity
    if (!posptr) return -EINVAL;
    // get the position
    *posptr = dev->pos;
    return 0;
}

/**
 * @brief Sets the current position within the VirtIO block device.
 * 
 * This function updates the current read/write position of the device to the specified value.
 * 
 * @param dev Pointer to the `vioblk_device` structure representing the block device.
 * @param posptr Pointer to a `uint64_t` containing the new position to set.
 * 
 * @return int Returns 0 on success. Returns -EINVAL if `posptr` is NULL or if the position exceeds the device size.
 * 
 * @details 
 * - Validates `posptr` to ensure it is a valid pointer and that the position does not exceed the device size.
 * - Updates the current position within the device, setting `dev->pos` to the value in `*posptr`.
 */
int vioblk_setpos(struct vioblk_device * dev, const uint64_t * posptr) {
    //           FIXME your code here

    // check for the validity for input
    if (!posptr)    return -EINVAL;
    if (*posptr > dev->size) return -EINVAL;
    // get the position
    dev->pos = *posptr;
    return 0;
}

/**
 * @brief Retrieves the block size of the VirtIO block device.
 * 
 * This function obtains the block size (in bytes) of the device and stores it in the provided pointer.
 * 
 * @param dev Pointer to the `vioblk_device` structure representing the block device.
 * @param blkszptr Pointer to a `uint32_t` where the block size will be stored.
 * 
 * @return int Returns 0 on success. Returns -EINVAL if `blkszptr` is NULL.
 * 
 * @details 
 * - Validates `blkszptr` to ensure it is a valid pointer.
 * - Stores the block size, as defined in `dev->blksz`, in `blkszptr`.
 */
int vioblk_getblksz (
    const struct vioblk_device * dev, uint32_t * blkszptr)
{
    //           FIXME your code here

    // check for the validity for pointer
    if (!blkszptr)    return -EINVAL;
    // get the block size
    *blkszptr = dev->blksz;
    return 0;
}
