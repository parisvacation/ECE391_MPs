#include "syscall.h"
#include "string.h"
#include <stdint.h>
#include "io.h"

void main(void) {
    int result;
    uint64_t position; 

    // Open the shared file for writing in the parent process
    result = _fsopen(0, "to_write");
    if (result < 0) {
        _msgout("_fsopen failed in parent");
        _exit();
    }

    // Get the position
    result = _ioctl(0, IOCTL_GETPOS, &position);

    // Fork a new process
    // Parent process
    if (_fork()) {
        _usleep(10000);
        // Write 3 times
        const char* const write_buf_1 = "Par\n";
        for (int i = 0; i < 3; i++) {
            _msgout("Parent writing...\r\n");
            _write(0, write_buf_1, 4);
        }

        // Wait for child to finish
        _wait(0);

        // Read and print the file content
        _msgout("Parent reading...\r\n");

        // Set the position
        result = _ioctl(0, IOCTL_SETPOS, &position);

        char read_buf[25];
        result = _read(0, read_buf, sizeof(read_buf) - 1);
        if (result > 0) {
            read_buf[result] = '\0';
            _msgout(read_buf);
        }

        _close(0);
        _exit();
    } 

    // Child process
    else {
        // Write 3 times
        const char* const write_buf_2 = "Chi\n";
        for (int i = 0; i < 3; i++) {
            _msgout("Child writing...\r\n");
            _write(0, write_buf_2, 4);
        }

        _close(0);
        _exit();
    }
}
