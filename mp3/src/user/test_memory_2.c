
#include "syscall.h"
#include "string.h"

void main(void) {
    _msgout("Hello, world from gmzOS!");
    _msgout("Testing virtual memory: Write to a kernel page......");

    char* ptr = (char*) 0x80001468;
    _msgout("No message should be after this line except 'Page fault: virtual address not in user range'!");
    ptr[0] = 'C';
    _msgout("You are wrong if you see me");
}
