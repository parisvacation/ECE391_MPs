#include "syscall.h"
#include "string.h"
#include <stdint.h>
#include "io.h"

void main(void) {
    int result;
    uint64_t position; 

    // Open the shared file for writing in the parent process
    _msgout("opening a file to read and write...\n");
    result = _fsopen(0, "to_write");
    if (result < 0) {
        _msgout("_fsopen failed in parent\n");
        _exit();
    }
    _msgout("open success.\n");

    // Fork a new process
    // Parent process
    if (_fork()) {
        // Wait for child to finish
        _msgout("Parent tries to close the file, should not close. Expected output: ioclose: Did not use io->ops->close.\n");
        _close(0);

        _msgout("Wait for child to exit\n");
        _wait(0);

        _msgout("Parent exit.\n");
        _exit();
    } 

    // Child process
    else {
        // Get the position
        result = _ioctl(0, IOCTL_GETPOS, &position);

        // Child process writes to the file
        _msgout("Child writing......\n");
        const char* const write_buf = "Child writes here!\n";
        _write(0, write_buf, 19);

        // Set the position
        result = _ioctl(0, IOCTL_SETPOS, &position);

        // Read and print the file content
        _msgout("Child reading...\r\n");
        char read_buf[20];
        result = _read(0, read_buf, sizeof(read_buf) - 1);
        if (result > 0) {
            read_buf[result] = '\0';
            _msgout(read_buf);
        }

        _msgout("Child closes the file.\n");
        _close(0);

        _msgout("Child exit.\n");
        _exit();
    }
}
