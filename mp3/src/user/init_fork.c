#include "syscall.h"
#include "string.h"

void main(void){
    _msgout("Test begins.\r\n");

    _fork();

    _msgout("Hello, world!\r\n");
    _exit();
}