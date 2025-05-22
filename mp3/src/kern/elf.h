//           elf.h - ELF executable loader
//          

#ifndef _ELF_H_
#define _ELF_H_

#include "io.h"
#include "error.h"

//           arg1: io interface from which to load the elf arg2: pointer to void
//           (*entry)(struct io_intf *io), which is a function pointer elf_load fills in
//           w/ the address of the entry point

//           int elf_load(struct io_intf *io, void (**entry)(struct io_intf *io)) Loads an
//           executable ELF file into memory and returns the entry point. The /io/
//           argument is the I/O interface, typically a file, from which the image is to
//           be loaded. The /entryptr/ argument is a pointer to an function pointer that
//           will be filled in with the entry point of the ELF file.
//           Return 0 on success or a negative error code on error.

int elf_load(struct io_intf *io, void (**entryptr)(void));

//           _ELF_H_
#endif

