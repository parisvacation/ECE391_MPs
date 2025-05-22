#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stddef.h>
#include "trap.h"

/**
 * @brief Terminates the current process.
 */
static int sysexit(void);

/**
 * @brief Outputs a message to the system console.
 * 
 * @param msg The message to be output.
 * @return int Returns 0 on success, or a negative error code on failure.
 */
static int sysmsgout(const char *msg);

/**
 * @brief Opens a device.
 * 
 * @param fd The file descriptor.
 * @param name The name of the device.
 * @param instno The instance number of the device.
 * @return int Returns 0 on success, or a negative error code on failure.
 */
static int sysdevopen(int fd, const char *name, int instno);

/**
 * @brief Opens a file system.
 * 
 * @param fd The file descriptor.
 * @param name The name of the file system.
 * @return int Returns 0 on success, or a negative error code on failure.
 *         If the file descriptor is negative, returns the next available file descriptor
 */
static int sysfsopen(int fd, const char *name);

/**
 * @brief Closes a file descriptor.
 * 
 * @param fd The file descriptor to be closed.
 * @return int Returns 0 on success, or a negative error code on failure.
 */
static int sysclose(int fd);

/**
 * @brief Reads data from a file descriptor.
 * 
 * @param fd The file descriptor to read from.
 * @param buf The buffer to store the read data.
 * @param bufsz The size of the buffer.
 * @return long Returns the number of bytes read, or a negative error code on failure.
 */
static long sysread(int fd, void *buf, size_t bufsz);

/**
 * @brief Writes data to a file descriptor.
 * 
 * @param fd The file descriptor to write to.
 * @param buf The buffer containing the data to be written.
 * @param len The length of the data to be written.
 * @return long Returns the number of bytes written, or a negative error code on failure.
 */
static long syswrite(int fd, const void *buf, size_t len);

/**
 * @brief Performs an I/O control operation on a file descriptor.
 * 
 * @param fd The file descriptor.
 * @param cmd The control command.
 * @param arg The argument for the control command.
 * @return int Returns 0 on success, or a negative error code on failure.
 */
static int sysioctl(int fd, int cmd, void *arg);

/**
 * @brief Executes a file.
 * 
 * @param fd The file descriptor of the file to be executed.
 * @return int Returns 0 on success, or a negative error code on failure.
 */
static int sysexec(int fd);

/**
 * @brief Forks the current process.
 * 
 * @param tfr The trap frame containing the system call information.
 * @return int Returns the process id of the child process.
 */
static int sysfork(const struct trap_frame *tfr);

/**
 * @brief Wait for a certain child to exit before returning.
 * 
 * @param tid The thread id of the child thread to exit.
 * @return int Returns the thread id of the child thread.
 */
static int syswait(int tid);

/**
 * @brief Sleep for us number of microseconds.
 * 
 * @param us The number of microseconds to sleep.
 * @return int Returns 0 when sleeping is finished
 */
static int sysusleep(unsigned long us);

/**
 * @brief Handles a system call.
 * 
 * @param tfr The trap frame containing the system call information.
 */
extern void syscall_handler(struct trap_frame *tfr);

/**
 * @brief Dispatches a system call based on the trap frame.
 * 
 * @param tfr The trap frame containing the system call information.
 * @return uint64_t Returns the result of the system call.
 */
uint64_t syscall(struct trap_frame *tfr);

#endif
