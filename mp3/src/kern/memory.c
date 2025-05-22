// memory.c - Memory management
//

#ifndef TRACE
#ifdef MEMORY_TRACE
#define TRACE
#endif
#endif

#ifndef DEBUG
#ifdef MEMORY_DEBUG
#define DEBUG
#endif
#endif

#include "config.h"

#include "memory.h"
#include "console.h"
#include "halt.h"
#include "heap.h"
#include "csr.h"
#include "string.h"
#include "error.h"
#include "thread.h"
#include "process.h"

#include <stdint.h>

// EXPORTED VARIABLE DEFINITIONS
//

char memory_initialized = 0;
uintptr_t main_mtag;

// IMPORTED VARIABLE DECLARATIONS
//

// The following are provided by the linker (kernel.ld)

extern char _kimg_start[];
extern char _kimg_text_start[];
extern char _kimg_text_end[];
extern char _kimg_rodata_start[];
extern char _kimg_rodata_end[];
extern char _kimg_data_start[];
extern char _kimg_data_end[];
extern char _kimg_end[];

// INTERNAL TYPE DEFINITIONS
//

union linked_page {
    union linked_page * next;
    char padding[PAGE_SIZE];
};

struct pte {
    uint64_t flags:8;
    uint64_t rsw:2;
    uint64_t ppn:44;
    uint64_t reserved:7;
    uint64_t pbmt:2;
    uint64_t n:1;
};

// INTERNAL MACRO DEFINITIONS
//

#define VPN2(vma) (((vma) >> (9+9+12)) & 0x1FF)
#define VPN1(vma) (((vma) >> (9+12)) & 0x1FF)
#define VPN0(vma) (((vma) >> 12) & 0x1FF)
#define MIN(a,b) (((a)<(b))?(a):(b))

// INTERNAL FUNCTION DECLARATIONS
//

static inline int wellformed_vma(uintptr_t vma);
static inline int wellformed_vptr(const void * vp);
static inline int aligned_addr(uintptr_t vma, size_t blksz);
static inline int aligned_ptr(const void * p, size_t blksz);
static inline int aligned_size(size_t size, size_t blksz);

static inline uintptr_t active_space_mtag(void);
static inline struct pte * mtag_to_root(uintptr_t mtag);
static inline struct pte * active_space_root(void);

static inline void * pagenum_to_pageptr(uintptr_t n);
static inline uintptr_t pageptr_to_pagenum(const void * p);

static inline void * round_up_ptr(void * p, size_t blksz);
static inline uintptr_t round_up_addr(uintptr_t addr, size_t blksz);
static inline size_t round_up_size(size_t n, size_t blksz);
static inline void * round_down_ptr(void * p, size_t blksz);
static inline size_t round_down_size(size_t n, size_t blksz);
static inline uintptr_t round_down_addr(uintptr_t addr, size_t blksz);

static inline struct pte leaf_pte (
    const void * pptr, uint_fast8_t rwxug_flags);
static inline struct pte ptab_pte (
    const struct pte * ptab, uint_fast8_t g_flag);
static inline struct pte null_pte(void);

static inline void sfence_vma(void);

// INTERNAL GLOBAL VARIABLES
//

static union linked_page * free_list; // the free pages in a linked list

static struct pte main_pt2[PTE_CNT]
    __attribute__ ((section(".bss.pagetable"), aligned(4096))); // the root page table
static struct pte main_pt1_0x80000[PTE_CNT]
    __attribute__ ((section(".bss.pagetable"), aligned(4096))); // the level-1 page table
static struct pte main_pt0_0x80000[PTE_CNT]
    __attribute__ ((section(".bss.pagetable"), aligned(4096))); // the level-0 page table

// EXPORTED VARIABLE DEFINITIONS
//

// EXPORTED FUNCTION DEFINITIONS
// 

/**memory_init
 * 
 * Sets up page tables and performs virtual-to-physical 1:1 mapping of the kernel megapage.
 * Enables Sv39 paging. Initializes the heap memory manager.
 * Puts free pages on the free pages list. Allows S mode access of U mode memory.
 * 
 * Input: none
 * Output: none
 */
void memory_init(void) {
    const void * const text_start = _kimg_text_start;
    const void * const text_end = _kimg_text_end;
    const void * const rodata_start = _kimg_rodata_start;
    const void * const rodata_end = _kimg_rodata_end;
    const void * const data_start = _kimg_data_start;
    union linked_page * page;
    void * heap_start;
    void * heap_end; // also user_start
    size_t page_cnt;
    uintptr_t pma;
    const void * pp;

    trace("%s()", __func__);

    assert (RAM_START == _kimg_start);

    kprintf("           RAM: [%p,%p): %zu MB\n",
        RAM_START, RAM_END, RAM_SIZE / 1024 / 1024);
    kprintf("  Kernel image: [%p,%p)\n", _kimg_start, _kimg_end);

    // Kernel must fit inside 2MB megapage (one level 1 PTE)
    
    if (MEGA_SIZE < _kimg_end - _kimg_start)
        panic("Kernel too large");

    // Initialize main page table with the following direct mapping:
    // 
    //         0 to RAM_START:           RW gigapages (MMIO region)
    // RAM_START to _kimg_end:           RX/R/RW pages based on kernel image
    // _kimg_end to RAM_START+MEGA_SIZE: RW pages (heap and free page pool)
    // RAM_START+MEGA_SIZE to RAM_END:   RW megapages (free page pool)
    //
    // RAM_START = 0x80000000 (2G)
    // MEGA_SIZE = 2 MB
    // GIGA_SIZE = 1 GB
    
    // Identity mapping of two gigabytes (as two gigapage mappings)
    for (pma = 0; pma < RAM_START_PMA; pma += GIGA_SIZE)
        main_pt2[VPN2(pma)] = leaf_pte((void*)pma, PTE_R | PTE_W | PTE_G);
    
    // Third gigarange has a second-level page table
    main_pt2[VPN2(RAM_START_PMA)] = ptab_pte(main_pt1_0x80000, PTE_G);

    // First physical megarange of RAM is mapped as individual pages with
    // permissions based on kernel image region.

    main_pt1_0x80000[VPN1(RAM_START_PMA)] =
        ptab_pte(main_pt0_0x80000, PTE_G);

    kprintf("   Kernel text: [%p,%p)\n", text_start, text_end);
    kprintf(" Kernel rodata: [%p,%p)\n", rodata_start, rodata_end);
    kprintf("   Kernel data: [%p,%p)\n", data_start, _kimg_end);
    kprintf("Heap available: [%p,%p)\n", _kimg_end, RAM_START + MEGA_SIZE);
    
    for (pp = text_start; pp < text_end; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_X | PTE_G);
    }

    for (pp = rodata_start; pp < rodata_end; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_G);
    }

    for (pp = data_start; pp < RAM_START + MEGA_SIZE; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_W | PTE_G);
    }

    // Remaining RAM mapped in 2MB megapages

    for (pp = RAM_START + MEGA_SIZE; pp < RAM_END; pp += MEGA_SIZE) {
        main_pt1_0x80000[VPN1((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_W | PTE_G);
    }

    // Enable paging. This part always makes me nervous.

    main_mtag =  // Sv39
        ((uintptr_t)RISCV_SATP_MODE_Sv39 << RISCV_SATP_MODE_shift) |
        pageptr_to_pagenum(main_pt2);
    
    csrw_satp(main_mtag);
    sfence_vma();

    // Give the memory between the end of the kernel image and the next page
    // boundary to the heap allocator, but make sure it is at least
    // HEAP_INIT_MIN bytes.

    heap_start = _kimg_end;
    heap_end = round_up_ptr(heap_start, PAGE_SIZE);
    if (heap_end - heap_start < HEAP_INIT_MIN) {
        heap_end += round_up_size (
            HEAP_INIT_MIN - (heap_end - heap_start), PAGE_SIZE);
    }

    if (RAM_END < heap_end)
        panic("Not enough memory");
    
    // Initialize heap memory manager

    heap_init(heap_start, heap_end);

    kprintf("Heap allocator: [%p,%p): %zu KB free\n",
        heap_start, heap_end, (heap_end - heap_start) / 1024);

    free_list = heap_end; // heap_end is page aligned
    page_cnt = (RAM_END - heap_end) / PAGE_SIZE;

    kprintf("Page allocator: [%p,%p): %lu pages free\n",
        free_list, RAM_END, page_cnt);

    // Put free pages on the free page list
    // TODO: FIXME implement this (must work with your implementation of
    // memory_alloc_page and memory_free_page).

    // original version, failed with the arithmetics of ptrs
    // free_list = (union linked_page*)(RAM_START + MEGA_SIZE);
    // for (uint64_t pp = (uint64_t)free_list; pp < (uint64_t)(RAM_END - PAGE_SIZE); pp += PAGE_SIZE){
    //     page = (union linked_page*) pp;
    //     page->next = (union linked_page*)(pp+PAGE_SIZE);
    // }
    // union linked_page* last_free_pp = RAM_END - PAGE_SIZE;
    // last_free_pp->next = NULL;

    for(size_t i=0; i<page_cnt-1; i++){
        // union linked_page* next;
        page = free_list + 1;
        page->next = free_list;
        free_list = page;
        if(i == 0){
            page->next->next = NULL;
        }
    }
    
    // Allow supervisor to access user memory. We could be more precise by only
    // enabling it when we are accessing user memory, and disable it at other
    // times to catch bugs.

    csrs_sstatus(RISCV_SSTATUS_SUM);

    memory_initialized = 1;
}



/**memory_space_create
 * 
 * Creates a new memory space and makes it the currently active space. Returns a
 * memory space tag (type uintptr_t) that may be used to refer to the memory
 * space. The created memory space contains the same identity mapping of MMIO
 * address space and RAM as the main memory space. This function never fails; if
 * there are not enough physical memory pages to create the new memory space, it
 * panics.
 * 
 * Input: asid - address space identifier
 * Output: the new mtag for the new memory space (has been stored in satp)
 */
uintptr_t memory_space_create(uint_fast16_t asid){
    trace("%s(asid=%d)", __func__, asid);

    // allocate a new page for the root table
    struct pte* new_pt2 = memory_alloc_page();

    // Identity mapping of two gigabytes (as two gigapage mappings)
    for (uintptr_t pma = 0; pma < RAM_START_PMA; pma += GIGA_SIZE)
        new_pt2[VPN2(pma)] = leaf_pte((void*)pma, PTE_R | PTE_W | PTE_G);
    // map the RAM the same way as in main memory space
    new_pt2[VPN2(RAM_START_PMA)] = ptab_pte(main_pt1_0x80000, PTE_G);

    // get the new mtag and switch to the new memory space
    uintptr_t new_mtag = ((uintptr_t)RISCV_SATP_MODE_Sv39 << RISCV_SATP_MODE_shift) | ((uintptr_t)asid << RISCV_SATP_ASID_shift) | pageptr_to_pagenum(new_pt2);
    // memory_space_switch(new_mtag);
    sfence_vma();

    return new_mtag;
}



/**memory_space_clone 
 * 
 * clone your memory space for current process and return the mtag of the new memory space.
 * Should be used in thread_fork_to_user to setup the memory space for the child process.
 * 
 * Input: asid - address space identifier
 * Output: the mtag of the new memory space
*/
uintptr_t memory_space_clone(uint_fast16_t asid){
    trace("%s(asid=%d)", __func__, asid);

    // allocate a new page for the root table
    struct pte* new_pt2 = memory_alloc_page();

    // Identity mapping of two gigabytes (as two gigapage mappings)
    for (uintptr_t pma = 0; pma < RAM_START_PMA; pma += GIGA_SIZE)
        new_pt2[VPN2(pma)] = leaf_pte((void*)pma, PTE_R | PTE_W | PTE_G);
    // map the RAM the same way as in main memory space
    new_pt2[VPN2(RAM_START_PMA)] = ptab_pte(main_pt1_0x80000, PTE_G);

    // copy all user region tables
    // get the original root table (now active)
    struct pte* root = active_space_root();
    // loop through the user space and copy every valid pte
    for(uintptr_t vma = USER_START_VMA; vma < USER_END_VMA; vma += PAGE_SIZE){
        // get all indices for translation
        // uintptr_t vpn2 = VPN2(vma);
        // uintptr_t vpn1 = VPN1(vma);
        // uintptr_t vpn0 = VPN0(vma);

        // get the original level-0 pte
        // check the flags to see if valid
        // uint64_t flags2 = root[vpn2].flags;
        // if(!(flags2 & PTE_V))
        //     continue;
        
        // struct pte* pt1 = pagenum_to_pageptr(root[vpn2].ppn);
        // uint64_t flags1 = pt1[vpn1].flags;
        // if(!(flags1 & PTE_V))
        //     continue;
        
        // struct pte* pt0 = pagenum_to_pageptr(pt1[vpn1].ppn);
        // uint64_t flags0 = pt0[vpn0].flags;
        // if(!(flags0 & PTE_V))
        //     continue;
        
        struct pte* original_leaf = walk_pt(root, vma, 0); // 0: no create
        if(original_leaf == NULL)
            continue;
        struct pte* new_leaf = walk_pt(new_pt2, vma, 1); // 1: create new pt enabled
        new_leaf->flags = original_leaf->flags;
        // copy the old page to new page
        // allocate and setup new page
        void* new_page = pagenum_to_pageptr(new_leaf->ppn);
        void* orig_page = pagenum_to_pageptr(original_leaf->ppn);
        memcpy(new_page, orig_page, PAGE_SIZE);

        // // create the new pte
        // *new_leaf = *original_leaf;
        // new_leaf->ppn = pageptr_to_pagenum(new_page);
    }

    // get the new mtag
    uintptr_t new_mtag = ((uintptr_t)RISCV_SATP_MODE_Sv39 << RISCV_SATP_MODE_shift) | ((uintptr_t)asid << RISCV_SATP_ASID_shift) | pageptr_to_pagenum(new_pt2);

    return new_mtag;
}



/**memory_space_reclaim
 * 
 * Switch the active memory space to the main memory space
 * and reclaims the memory space that was active on entry.
 * All physical pages mapped by the memory space that are
 * not part of the global mapping are reclaimed.
 * 
 * Note: reclaim
 * free the allocated pages, mark the ptes as null
 * 
 * Input: none
 * Output: none
 * ASSUMPTION: Only the kernel page tables are global, and the user page tables are never global.
 * If this is not the case, a helper function is needed to check whether
 * the entries in the subtable is all global
 */
void memory_space_reclaim(void){
    trace("%s()", __func__);
    // switch the active memory space to the main memory space
    uintptr_t active_mtag = memory_space_switch(main_mtag); /* what about the last active? Reclaim! */
    sfence_vma();

    // reclaim the memory space that was active on entry
    struct pte* curr_pt2 = mtag_to_root(active_mtag);
    for (uint64_t i=0; i<PTE_CNT; i++){
        // the flag of this pte
        uint64_t flags2 = curr_pt2[i].flags;
        // check if valid, if not, continue
        if (!(flags2 & PTE_V)){
            continue;
        }

        // the page this pte is pointing at
        struct pte* page1 = (struct pte*)pagenum_to_pageptr(curr_pt2[i].ppn);
        // check if global, if not, reclaim
        uint64_t flags2_G = flags2 & PTE_G;
        if(!flags2_G){
            // check if this entry links to another subtable
            int64_t flags2_RWX = flags2 & (PTE_R | PTE_W | PTE_X);
            if(!flags2_RWX){ // contains a level-1 subtable
                for(uint64_t j=0; j<PTE_CNT; j++){
                    uint64_t flags1 = page1[j].flags;
                    // check if valid. continue if invalid.
                    if (!(flags1 & PTE_V))
                        continue;

                    struct pte* page0 = (struct pte*)pagenum_to_pageptr(page1[j].ppn);
                    // check if global. if not, reclaim.
                    if(!(flags1 & PTE_G)){
                        if(!(flags1 & (PTE_R | PTE_W | PTE_X))){
                            // contains a level-0 subtable
                            for(uint64_t k=0; k<PTE_CNT; k++){
                                uint64_t flags0 = page0[k].flags;
                                // check if valid. continue if invalid.
                                if(!(flags0 & PTE_V))
                                    continue;
                                
                                void* page = pagenum_to_pageptr(page0[k].ppn);
                                
                                if((uint64_t)page > (uint64_t)_kimg_end && (uint64_t)page < (uint64_t)RAM_END){
                                    memory_free_page(page);
                                    page0[k] = null_pte();
                                }
                            }
                            // ASSUMPTION: Only the kernel page tables are global, and
                            // the user page tables are never global. If this is not the case,
                            // a helper function is needed to check whether the entries
                            // in the subtable is all global
                            page1[j] = null_pte();
                        }
                    }
                }
                curr_pt2[i] = null_pte();
            }
        }
    }
}



/**memory_alloc_page
 * 
 * Allocate a physical page of memory using the free pages list.
 * Returns the virtual address of the direct mapped page as a void*.
 * Panics if there are no free pages available.
 * ezheap.c will call this function when the heap is full.
 * 
 * Input: none
 * Output: a void* ptr, which is the virtual addr of the direct mapped page
 */
void * memory_alloc_page(void){
    trace("%s()", __func__);
    if(free_list == NULL){
        panic("No free page available");
    }
    void* alloc_page = (void*)free_list;
    free_list = free_list->next;
    return alloc_page;
}


/**memory_free_page
 * 
 * Return a physical memory page to the physical page allocator (freepageslist).
 * The page must have been previously allocated by memory_alloc_page.
 * 
 * Input: pp - a pointer to a page
 * output: none
 */
void memory_free_page(void * pp){
    trace("%s(pp=%p)", __func__, pp);
    union linked_page* free_page = (union linked_page*) pp;
    free_page->next = free_list;
    free_list = free_page;
}



/**walk_pt
 * 
 * This function takes a pointer to your active root page table and a virtual memory address.
 * It walks down the page table structure using the VPN fields of vma, and
 * if create is non-zero, it will create the appropriate page tables to walk to the leaf page table (”level 0”).
 * It returns a pointer to the page table entry that represents the 4 kB page containing vma.
 * This function will only walk to a 4 kB page, not a megapage or gigapage.
 * 
 * Input: root - ptr to the root table
 *        vma  - the virtual memory address
 *        create - specify whether to create a new page table entry or not
 * Output: a ptr to a leaf pte
 * ASSUMPTION: the vma will be translated to the address of a 4kB page
 */
struct pte* walk_pt(struct pte* root, uintptr_t vma, int create){
    trace("%s(root=%p,vma=%p,create=%d)", __func__, root, vma, create);
    // get all indices for translation
    uintptr_t vpn2 = VPN2(vma);
    uintptr_t vpn1 = VPN1(vma);
    uintptr_t vpn0 = VPN0(vma);

    // root page table
    // check the flags to see if valid
    uint64_t flags2 = root[vpn2].flags;
    if(!(flags2 & PTE_V)){
        // if not allowed to create PTE, panic
        if (!create){
            if(flags2&(PTE_R|PTE_W|PTE_X)){
                panic("Invalid PTE at root page table");
            }
            return NULL;
        }
        // allowed, create
        struct pte* curr_pt1 = memory_alloc_page();
        memset(curr_pt1, 0, PAGE_SIZE);
        root[vpn2] = ptab_pte(curr_pt1, 0); // all flags except PTE_V are set to 0
    }

    // level-1 page table
    // Now root PTE is valid. Get level-1 PTE.
    struct pte* pt1 = pagenum_to_pageptr(root[vpn2].ppn);
    uint64_t flags1 = pt1[vpn1].flags;
    if(!(flags1 & PTE_V)){
        // if not allowed to create PTE, panic
        if (!create){
            if(flags1&(PTE_R|PTE_W|PTE_X)){
                panic("Invalid PTE at root page table");
            }
            return NULL;
        }
        // allowed, create
        struct pte* curr_pt0 = memory_alloc_page();
        memset(curr_pt0, 0, PAGE_SIZE);
        pt1[vpn1] = ptab_pte(curr_pt0, 0); // all flags except PTE_V are set to 0
    }

    // level-0 page table
    struct pte* pt0 = pagenum_to_pageptr(pt1[vpn1].ppn);
    uint64_t flags0 = pt0[vpn0].flags;
    if(!(flags0 & PTE_V)){
        // if not allowed to create PTE, panic
        if (!create){
            if(flags2&(PTE_R|PTE_W|PTE_X)){
                panic("Invalid PTE at root page table");
            }
            return NULL;
        }
        // allowed, create a new page
        void* page = memory_alloc_page();
        memset(page, 0, PAGE_SIZE);
        pt0[vpn0] = leaf_pte(page, 0); // all flags set to 0
    }

    // leaf pte
    struct pte* leaf = &pt0[vpn0];

    return leaf;
}



/**memory_alloc_and_map_page
 * 
 * Allocate a single physical page and maps a virtual address to it with provided flags.
 * Returns the mapped virtual memory address.
 * 
 * Input: vma - the virtual memory address
 *        rwxg_flags - the provided flags
 * Output: the virtual memory address
 */
void * memory_alloc_and_map_page(uintptr_t vma, uint_fast8_t rwxug_flags){
    trace("%s(vma=%p, rwxug_flags=%x)", __func__, vma, rwxug_flags);
    // allocate new page
    uintptr_t new_page = (uintptr_t)memory_alloc_page();
    // get the root
    struct pte* root = active_space_root();

    // get ptr to a new entry
    int create = 1; // a non-zero value meaning to create a new entry
    struct pte* new_entry = walk_pt(root, vma, create);
    *new_entry = leaf_pte((void*)new_page, rwxug_flags);
    sfence_vma();

    return (void*)vma;
}



/**memory_alloc_and_map_range
 * 
 * Allocates the range of memory and maps a virtual address with
 * the provided flags. Returns the mapped virtual memory address.
 * 
 * Input: vma - the virtual memory address
 *        size - the size of the space to map
 *        rwxg_flags - the provided flags
 * Output: the mapped vma
 */
void * memory_alloc_and_map_range(uintptr_t vma, size_t size, uint_fast8_t rwxug_flags){
    trace("%s(vma=%p, size=%d, rwxug_flags=%x)", __func__, vma, size, rwxug_flags);
    for(void* _vp = (void*) vma; _vp < (void*)(vma + size); _vp += PAGE_SIZE){
        _vp = memory_alloc_and_map_page((uintptr_t)_vp, rwxug_flags); // _vp does not change
    }

    return (void*)vma;
}



/**memory_set_page_flags
 * 
 * Sets the flags of the PTE associated with vp.
 * Only works with 4kB pages.
 * 
 * Input: vp - a virtual pointer to a virtual page
 *        rwxug_flags - the provided flags
 * Output: none
 */
void memory_set_page_flags(const void* vp, uint8_t rwxug_flags){
    trace("%s(vp=%p, rwxug_flags=%x)", __func__, vp, rwxug_flags);
    // get the root
    struct pte * root = active_space_root();
    // get the leaf entry
    int create = 1; // create new page if needed
    struct pte* leaf_entry = walk_pt(root, (uintptr_t)vp, create);
    leaf_entry->flags = rwxug_flags | PTE_A | PTE_D | PTE_V;
    sfence_vma();
}



/**memory_set_range_flags
 * 
 * Modify flags of all PTE within the specified virtual memory range.
 * 
 * Input: vma - the virtual memory address
 *        size - the size of the space to modify flags
 *        rwxg_flags - the provided flags
 * Output: none
 */
void memory_set_range_flags (const void * vp, size_t size, uint_fast8_t rwxug_flags){
    trace("%s(vma=%p, size=%d, rwxug_flags=%x)", __func__, vp, size, rwxug_flags);
    for(void* _vp = (void*) vp; _vp < (void*)(vp + size); _vp += PAGE_SIZE){
        memory_set_page_flags(_vp, rwxug_flags);
    }

    return;
}


/**valid_check_pt
 * 
 * Check if a page table contains any valid PTE.
 * 
 * Input: a ptr to the page table to be checked
 * Output: 1 for containing valid PTE (V=1), 0 for not
 */
int valid_check_pt(const struct pte* pt){
    for(unsigned i=0; i<PTE_CNT; i++){
        if(!(pt[i].flags & PTE_V))
            return 0;
    }

    return 1;
}



/**memory_unmap_and_free_user
 * 
 * Unmaps and frees ALL user space pages (that is, all pages with
 * the U flag asserted) in the current memory space.
 * 
 * Input: none
 * Output: none
 */
void memory_unmap_and_free_user(void){
    // get the root
    struct pte* root = active_space_root();

    // loop through the user address space
    for(uintptr_t vma = USER_START_VMA; vma < USER_END_VMA; vma += PAGE_SIZE){
        struct pte* leaf_pte=walk_pt(root,vma,0);
        if(!leaf_pte) // invalid
            continue;
        else{
            void* pp = pagenum_to_pageptr(leaf_pte->ppn);
            memory_free_page(pp);
            *leaf_pte=null_pte();
        }
   }
   
   // flush
   sfence_vma();
}





/**memory_validate_vptr_len
 * 
 * Ensure that the virtual pointer provided (vp) points to a mapped region
 * of size len and has at least the specified flags.
 * 
 * Input: vp - the provided virtual pointer
 *        len - the size of the space to be verified
 *        rwxug_flags - the specified flags
 * Output: 1 for success, 0 for failure
 */
int memory_validate_vptr_len (const void * vp, size_t len, uint_fast8_t rwxug_flags){
    trace("%s(vp=%p, len=%d, rwxug_flags=%x)", __func__, vp, len, rwxug_flags);
    // get the root
    struct pte* root = active_space_root();
    const void* end = vp + len;
    for(const void* vma = vp; vma < end; vma += PAGE_SIZE){
        // get the leaf entry
        struct pte* leaf = walk_pt(root, (uintptr_t)vma, 0); // 0: does not create any page table entry
        // if entry invalid, return -1
        if(!leaf){
            return -1;
        }

        if (!(leaf->flags & PTE_V))
            return -1;
        
        // if do not contain the specified flags, return 0
        if (!(leaf->flags & rwxug_flags))
            return -1;
    }

    return 1; // success
}


/**memory_validate_vstr
 * 
 * Ensure that the virtual pointer provided (vs) contains 
 * a NUL-terminated string.
 * 
 * Input: vs - the virtal pointer
 *        ug_flags - the provied flags
 * Output: 1 for success, 0 for failure
 */
int memory_validate_vstr (const char * vs, uint_fast8_t ug_flags){
    trace("%s(vs=%p, ug_flags=%x)", __func__, vs, ug_flags);
    // get the root
    struct pte* root = active_space_root();
    const void* vp = vs;
    char* pma; // contains physical memory address of the string

    do{
        uintptr_t offset = (uintptr_t)vp & (0xFFF);

        struct pte* leaf = walk_pt(root, (uintptr_t)vp, 0); // 0 for not create new pte
        if(!leaf){
            return -1;
        }
        // if entry invalid, return 0
        if (!(leaf->flags & PTE_V))
            return -1;
        // if do not contain the specified flags, return 0
        if (!(leaf->flags & ug_flags))
            return -1;
        
        uintptr_t page = (uintptr_t)pagenum_to_pageptr((uintptr_t)leaf->ppn);
        pma = (char*)(page + offset);
        vp++;
    } while(strcmp(pma, "\0"));
    
    return 1;

    // for(const void* vp = vs; strcmp(*((char*)vp),"\0")!=0; vp++){
    //     // if the vs points to an invalid region, return 0
    //     if(memory_validate_vptr_len(vp, 1, ug_flags)){
    //         return 0;
    //     }
    // }
}


/**memory_handle_page_fault
 * 
 * Handle a page fault at a virtual address. May choose to panic or to allocate a new page, 
 * depending on if the address is within the user region.
 * You must call this function when a store page fault is triggered by a user program.
 */
void memory_handle_page_fault(const void * vptr){
    trace("%s(vptr=%p)", __func__, vptr);
    // check if vptr is in the user range
    uintptr_t vma = (uintptr_t)vptr;
    if ((vma >= USER_START_VMA) && vma < USER_END_VMA){
        // inside the user range, allocate a new page for user
        memory_alloc_and_map_page(vma, PTE_R | PTE_W | PTE_U);
    }
    else{
        kprintf("vma = %p", vma);
        panic("Page fault: virtual address not in user range");
    }
}



// INTERNAL FUNCTION DEFINITIONS
//

static inline int wellformed_vma(uintptr_t vma) {
    // Address bits 63:38 must be all 0 or all 1
    uintptr_t const bits = (intptr_t)vma >> 38;
    return (!bits || !(bits+1));
}

static inline int wellformed_vptr(const void * vp) {
    return wellformed_vma((uintptr_t)vp);
}

static inline int aligned_addr(uintptr_t vma, size_t blksz) {
    return ((vma % blksz) == 0);
}

static inline int aligned_ptr(const void * p, size_t blksz) {
    return (aligned_addr((uintptr_t)p, blksz));
}

static inline int aligned_size(size_t size, size_t blksz) {
    return ((size % blksz) == 0);
}

static inline uintptr_t active_space_mtag(void) {
    return csrr_satp();
}

static inline struct pte * mtag_to_root(uintptr_t mtag) {
    return (struct pte *)((mtag << 20) >> 8);
}


static inline struct pte * active_space_root(void) {
    return mtag_to_root(active_space_mtag());
}

static inline void * pagenum_to_pageptr(uintptr_t n) {
    return (void*)(n << PAGE_ORDER);
}

static inline uintptr_t pageptr_to_pagenum(const void * p) {
    return (uintptr_t)p >> PAGE_ORDER;
}

static inline void * round_up_ptr(void * p, size_t blksz) {
    return (void*)((uintptr_t)(p + blksz-1) / blksz * blksz);
}

static inline uintptr_t round_up_addr(uintptr_t addr, size_t blksz) {
    return ((addr + blksz-1) / blksz * blksz);
}

static inline size_t round_up_size(size_t n, size_t blksz) {
    return (n + blksz-1) / blksz * blksz;
}

static inline void * round_down_ptr(void * p, size_t blksz) {
    return (void*)((uintptr_t)p / blksz * blksz);
}

static inline size_t round_down_size(size_t n, size_t blksz) {
    return n / blksz * blksz;
}

static inline uintptr_t round_down_addr(uintptr_t addr, size_t blksz) {
    return (addr / blksz * blksz);
}

static inline struct pte leaf_pte (
    const void * pptr, uint_fast8_t rwxug_flags)
{
    return (struct pte) {
        .flags = rwxug_flags | PTE_A | PTE_D | PTE_V,
        .ppn = pageptr_to_pagenum(pptr)
    };
}

static inline struct pte ptab_pte (
    const struct pte * ptab, uint_fast8_t g_flag)
{
    return (struct pte) {
        .flags = g_flag | PTE_V,
        .ppn = pageptr_to_pagenum(ptab)
    };
}

static inline struct pte null_pte(void) {
    return (struct pte) { };
}

static inline void sfence_vma(void) {
    asm inline ("sfence.vma" ::: "memory");
}
