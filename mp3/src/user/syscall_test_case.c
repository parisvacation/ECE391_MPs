#include "syscall.h"
#include "string.h"
#include "../kern/console.h"
#include "io.h"
#include <stdint.h>
void main(void) {
    const char * const Success = "Test Success!\r\n";
    const char * const Failed = "Test Failed\r\n";
    int result;
    uint64_t position;
    // Open ser1 device as fd=0

    _msgout("here testing msgout\r\n");
    _msgout(Success);

    _msgout("here testing devopen\r\n");

    result = _fsopen(2,"init0");

    result = _devopen(0,"ser",1);
    if (result < 0)return;
    _msgout(Success);
    char buf_s[10];

//#####
    _msgout("here testing read write\r\n");
    const char * const greeting = "Hello, world!\r\n";
    size_t slen;
    slen = strlen(greeting);
    _read(0,buf_s,1);
    result = _write(0,greeting,slen);
    if (result < 0)return;
    

    memset(buf_s,0,10);
    _read(0,buf_s,10);

    _msgout("here we use _msgout() to show what we have input into the ser1:");
    _msgout(buf_s);
    memset(buf_s,0,10);
    _msgout(Success);
    
    _msgout("here testing fsopen\r\n");
    result = _fsopen(1,"trek");

    // here test whether can open the file
    if (result < 0)return;
    _msgout(Success);

    _msgout("here testing ioctl\r\n");
    position=3;
    result=_ioctl(1,IOCTL_SETPOS,&position);
    result=_ioctl(1,IOCTL_GETPOS,&position);
    _close(1);
    _fsopen(1,"trek");

    if(position==3)_msgout(Success);
    else _msgout(Failed);

    _msgout("here testing close\r\n");
    result = _close(2);
    if (result < 0)return;
    _msgout(Success);

    _msgout("here testing exec\r\n");
    _exec(1);
}