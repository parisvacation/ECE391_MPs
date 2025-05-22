
#include "syscall.h"
#include "string.h"

void main(void) {
    _msgout("Hello, world from gmzOS!");
    _msgout("Testing virtual memory paging......");

    // reading/writing to an unmapped virtual memory addr in a userspace program
    _msgout("Testing demand paging...");

    char *unmapped_addr = (char *)0xC0018000; // Example address outside mapped range
    _msgout("Writing to unmapped virtual address...");
    *unmapped_addr = 'A'; // This should trigger a page fault and allocate a new page
    *(unmapped_addr+1) = '\0';
    _msgout("Writing success.");
    _msgout("Read from the virtual address...");
    _msgout(unmapped_addr); // print the string
    if (*unmapped_addr == 'A') {
        _msgout("Demand paging successful!");
    } else {
        _msgout("Demand paging failed!");
    }

    // using repeated pointer arithmetic operations to show successful paging
    _msgout("Initializing another ptr pointing to our string");
    char ** ptr_to_str = (char **)0xC0036000;
    *ptr_to_str = unmapped_addr;
    _msgout("read the string using ptr_to_str:");
    _msgout(*ptr_to_str);
    _msgout("read the string using &unmapped_addr[0]:");
    _msgout(&unmapped_addr[0]);
    _msgout("done");
}
