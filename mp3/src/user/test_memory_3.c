
#include "syscall.h"
#include "string.h"

void main(void) {
    _msgout("Hello, world from gmzOS!");
    _msgout("Testing virtual memory: Read from a kernel page......");

    
    _msgout("Test read");
    _msgout("Expected message: 'sysmsgout refuse the request'");
    _msgout((const char *)0x801c1234);
}
