#include <stdint.h>
#include <stddef.h>
#include <elf.h>
#include "io.h"
#include "elf.h" 
#include "heap.h" 
#include "memory.h"
#include "config.h"
#include "string.h"

#define EI_MAG0     0       // e_ident[] indexes
#define EI_MAG1     1
#define EI_MAG2     2
#define EI_MAG3     3
#define EI_CLASS    4
#define EI_DATA     5
#define EI_VERSION  6
#define EI_OSABI    7
#define EI_ABIVERSION 8
#define EI_PAD      9
#define EI_NIDENT   16

#define ELFMAG0     0x7f    // e_ident[EI_MAG0]
#define ELFMAG1     'E'     // e_ident[EI_MAG1]
#define ELFMAG2     'L'     // e_ident[EI_MAG2]
#define ELFMAG3     'F'     // e_ident[EI_MAG3]

#define ELFCLASS64  2       // 64-bit objects

#define EM_RISCV    243     // RISC-V architecture

#define ET_EXEC     2       // Executable file

#define PT_LOAD     1       // Loadable program segment

#define ELFDATA2LSB 1       // Little endian

#define PF_X		(1 << 0)	// Segment is executable
#define PF_W		(1 << 1)	// Segment is writable
#define PF_R		(1 << 2)	// Segment is readable

typedef struct {
    unsigned char e_ident[EI_NIDENT]; /* ELF identification */
    uint16_t e_type;                  /* Object file type */
    uint16_t e_machine;               /* Machine type */
    uint32_t e_version;               /* Object file vers ion */
    uint64_t e_entry;                 /* Entry point address */
    uint64_t e_phoff;                 /* Program header offset */
    uint64_t e_shoff;                 /* Section header offset */
    uint32_t e_flags;                 /* Processor-specific flags */
    uint16_t e_ehsize;                /* ELF header size */
    uint16_t e_phentsize;             /* Size of program header entry */
    uint16_t e_phnum;                 /* Number of program header entries */
    uint16_t e_shentsize;             /* Size of section header entry */
    uint16_t e_shnum;                 /* Number of section header entries */
    uint16_t e_shstrndx;              /* Section name string table index */
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;    /* Type of segment */
    uint32_t p_flags;   /* Segment attributes */
    uint64_t p_offset;  /* Offset in file */
    uint64_t p_vaddr;   /* Virtual address in memory */
    uint64_t p_paddr;   /* Reserved */
    uint64_t p_filesz;  /* Size of segment in file */
    uint64_t p_memsz;   /* Size of segment in memory */
    uint64_t p_align;   /* Alignment of segment */
} Elf64_Phdr;


/**
 * elf_load - Loads and validates an ELF executable into memory.
 *
 * Inputs: 
 * io -- Pointer to an I/O interface (struct io_intf*) to read the ELF file.
 * entryptr -- Pointer to a function pointer where the entry point of the ELF executable will be stored upon successful loading.
 *
 * Description:
 * This function reads and validates the ELF header from the provided I/O interface.
 * It checks for a valid ELF magic number, 64-bit format, RISC-V architecture, little-endian data encoding, and executable type.
 * 
 * If all validations pass, the function proceeds to load the ELF file's program
 * headers, ensuring that only PT_LOAD segments within the specified memory range
 * (between 0x80100000 and 0x81000000) are loaded. For each PT_LOAD segment,
 * the function allocates memory, reads the segment data, and copies it to the
 * designated virtual memory address. The .BSS section is zero-initialized.
 * 
 * Outputs:
 * - Sets *entryptr to the entry point address of the ELF executable if loading is successful.
 * 
 * Returns:
 * 0 -- success.
 * EIO -- the I/O interface is NULL.
 * ENOTSUP -- if there is an error reading the ELF header or program header.
 * EINVAL -- if the file is not a valid ELF executable or fails validation.
 */
int elf_load(struct io_intf *io, void (**entryptr)(void)) {
    if (io == NULL) 
        return -EIO;

    Elf64_Ehdr ehdr;

    // 1. Read the ELF header with io;
    long read_ELF_result = ioread_full(io, &ehdr, sizeof(Elf64_Ehdr));
    if (read_ELF_result < 0 || read_ELF_result < sizeof(Elf64_Ehdr)) {
        return -ENOTSUP; 
    }

    // 2. Check the magic numbers of e_ident, to see whether it is an available ELF file;
    if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 
        || ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3) {
        return -EINVAL; 
    }

    // 3. Check if the ELF file is 64-bit;
    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64){
        return -EINVAL;
    }

    // 4. Check if the ELF file is RISC-V architecture;
    if (ehdr.e_machine != EM_RISCV){
        return -EINVAL;
    }
    
    // 5. Check if the ELF file is little-endian;
    if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB){
        return -EINVAL;
    }

    // 6. Check if the ELF file is executable;
    if (ehdr.e_type != ET_EXEC) {
        return -EINVAL; 
    }

    // 7. Set the address of entry
    *entryptr = (void (*)(void)) ehdr.e_entry;

    // 8. Set the position to the beginning of Program header table
    if (ioseek(io, ehdr.e_phoff) < 0){
        return -ENOTSUP;
    }

    // 9. Loop through the Program header table;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr phdr;
        long offset = ehdr.e_phoff + i * ehdr.e_phentsize;

        // 9.1 Set the io to current program header
        if (ioseek(io, offset) < 0){
            return -ENOTSUP;
        }

        // 9.2. Read the Program header table with io;
        long read_pro_result = ioread_full(io, &phdr, ehdr.e_phentsize);
        if (read_pro_result < 0 || read_pro_result < ehdr.e_phentsize) {
            return -ENOTSUP; 
        }

        // 9.3. Elf_loader should only load program header entries of type PT_LOAD;
        if (phdr.p_type != PT_LOAD) continue;

        // 9.4. Check the range of virtual memory (between USER_START_VMA and USER_END_VMA);
        if (phdr.p_vaddr < USER_START_VMA || phdr.p_vaddr + phdr.p_memsz > USER_END_VMA) {
            continue;
        }

        // 9.5. Allocate and map physical pages for the segment
        memory_alloc_and_map_range(phdr.p_vaddr, phdr.p_memsz, (PTE_W|PTE_R));
        
        // 9.6. Jump to the segment offset and load segment data into memory
        if (ioseek(io, phdr.p_offset) < 0 || ioread_full(io, (void*)phdr.p_vaddr, phdr.p_filesz) < phdr.p_filesz) {
            return -ENOTSUP;
        }

        // 9.7. Initialize the .BSS section to zeros;
        memset((void*)(phdr.p_vaddr + phdr.p_filesz), 0, phdr.p_memsz - phdr.p_filesz);

        // 9.8. Set appropriate flags for the memory region after loading
        int flags = PTE_U | PTE_V;
        if (phdr.p_flags & PF_R) flags |= PTE_R;
        if (phdr.p_flags & PF_W) flags |= PTE_W;
        if (phdr.p_flags & PF_X) flags |= PTE_X;
        memory_set_range_flags((const void*)phdr.p_vaddr, phdr.p_memsz, flags);
    }

    return 0;
}
