// plic.c - RISC-V PLIC
//

#include "plic.h"
#include "console.h"

#include <stdint.h>

// COMPILE-TIME CONFIGURATION
//

// *** Note to student: you MUST use PLIC_IOBASE for all address calculations,
// as this will be used for testing!

#ifndef PLIC_IOBASE
#define PLIC_IOBASE 0x0C000000
#endif

// define the base address of pending register
#ifndef PLIC_PENDING_BASE
#define PLIC_PENDING_BASE (PLIC_IOBASE + 0x00001000)
#endif

// define the base address of enable register
#ifndef PLIC_ENABLE_CONTEXT0
#define PLIC_ENABLE_CONTEXT0 (PLIC_IOBASE + 0x00002000)
#endif

// define the first address of threshold value
#ifndef PLIC_THRESHOLD_CONTEXT0
#define PLIC_THRESHOLD_CONTEXT0 (PLIC_IOBASE + 0x00200000)
#endif

// define the first address of claim register
#ifndef PLIC_CLAIM_CONTEXT0
#define PLIC_CLAIM_CONTEXT0 (PLIC_IOBASE + 0x00200004)
#endif

#define PLIC_SRCCNT 0x400
#define PLIC_CTXCNT 1

// define various size for enable register, threshold value address and claim register
#define ADDRESS_SIZE 32
#define ENABLE_SIZE 128
#define THRESHOLD_SIZE 4096
#define CLAIM_SIZE 4096

// INTERNAL FUNCTION DECLARATIONS
//

// *** Note to student: the following MUST be declared extern. Do not change these
// function delcarations!

extern void plic_set_source_priority(uint32_t srcno, uint32_t level);
extern int plic_source_pending(uint32_t srcno);
extern void plic_enable_source_for_context(uint32_t ctxno, uint32_t srcno);
extern void plic_disable_source_for_context(uint32_t ctxno, uint32_t srcno);
extern void plic_set_context_threshold(uint32_t ctxno, uint32_t level);
extern uint32_t plic_claim_context_interrupt(uint32_t ctxno);
extern void plic_complete_context_interrupt(uint32_t ctxno, uint32_t srcno);

// Currently supports only single-hart operation. The low-level PLIC functions
// already understand contexts, so we only need to modify the high-level
// functions (plic_init, plic_claim, plic_complete).

// EXPORTED FUNCTION DEFINITIONS
// 

void plic_init(void) {
    int i;

    // Disable all sources by setting priority to 0, enable all sources for
    // context 0 (M mode on hart 0).

    // Maybe the starting number of interrupt source should be one?
    for (i = 0; i < PLIC_SRCCNT; i++) {
        plic_set_source_priority(i, 0);
        plic_enable_source_for_context(0, i);
    }
}

extern void plic_enable_irq(int irqno, int prio) {
    trace("%s(irqno=%d,prio=%d)", __func__, irqno, prio);
    plic_set_source_priority(irqno, prio);
}

extern void plic_disable_irq(int irqno) {
    if (0 < irqno)
        plic_set_source_priority(irqno, 0);
    else
        debug("plic_disable_irq called with irqno = %d", irqno);
}

extern int plic_claim_irq(void) {
    // Hardwired context 0 (M mode on hart 0)
    trace("%s()", __func__);
    return plic_claim_context_interrupt(0);
}

extern void plic_close_irq(int irqno) {
    // Hardwired context 0 (M mode on hart 0)
    trace("%s(irqno=%d)", __func__, irqno);
    plic_complete_context_interrupt(0, irqno);
}

// INTERNAL FUNCTION DEFINITIONS
//

void plic_set_source_priority(uint32_t srcno, uint32_t level) {
    // FIXME your code goes here

    // Input: 
    //     srcno: Interrupt source ID
    //     level: Value of priority
    // Output:
    //      None
    // Side effect:
    //      Sets the priority level for a specific interrupt source.
    //      This function modifies the priority array, where each entry corresponds to a specific interrupt source.

    // Check if the source ID is available
    if (srcno < 1 || srcno >= PLIC_SRCCNT){
        return;
    }

    // Find the address for priority of Interrupt Source No.srcno
    volatile uint32_t* add_prio = (volatile uint32_t*) ((uintptr_t)(PLIC_IOBASE + 4 * srcno));
    *add_prio = level;
}

int plic_source_pending(uint32_t srcno) {
    // FIXME your code goes here

    // Input:
    //      srcno: Interrupt source ID
    // Output:
    //      1 -- If the interrupt is pending
    //      0 -- Otherwise
    // Side effect:
    //      Checks if an interrupt source is pending by inspecting the pending array.
    //      The pending bit is determined by checking the bit corresponding to srcno in the pending array

    int result;

    // Check if the source ID is available
    if (srcno < 1 || srcno >= PLIC_SRCCNT){
        result = 0;
        return result;
    }

    // Find the address for pending bit of Interrupt Source No.srcno
    volatile uint32_t* add_pend = (volatile uint32_t*) ((uintptr_t)(PLIC_PENDING_BASE + 4 * (srcno / ADDRESS_SIZE)));
    result = 1 & (*add_pend >> (srcno % ADDRESS_SIZE));

    return result;
}

void plic_enable_source_for_context(uint32_t ctxno, uint32_t srcno) {
    // FIXME your code goes here

    // Input:
    //      ctxno: PLIC context ID   
    //      srcno: Interrupt source ID
    // Output:
    //      None
    // Side effect:
    //      Enables a specific interrupt source for a given context.
    //      This function sets the appropriate bit in the enable array. 
    //      It calculates the index based on the source number and context, and sets the corresponding bit for the source.

    // Check if the source ID is available
    if (srcno < 1 || srcno >= PLIC_SRCCNT){
        return;
    }

    // Find the address for enable bit of Interrupt Source No.srcno
    volatile uint32_t* add_ctx_enable = (volatile uint32_t*) ((uintptr_t)(PLIC_ENABLE_CONTEXT0 + ctxno * ENABLE_SIZE));
    volatile uint32_t* add_enable = (volatile uint32_t*) ((uintptr_t)(add_ctx_enable + 4 * (srcno / ADDRESS_SIZE)));
    *add_enable |= (1 << (srcno % ADDRESS_SIZE));
}

void plic_disable_source_for_context(uint32_t ctxno, uint32_t srcid) {
    // FIXME your code goes here

    // Input:
    //      ctxno: PLIC context ID   
    //      srcid: Interrupt source ID
    // Output:
    //      None
    // Side effect:
    //      Disables a specific interrupt source for a given context.
    //      This function sets the appropriate bit in the enable array. 
    //      It calculates the index based on the source number and context, and sets the corresponding bit for the source.

    // Check if the source ID is available
    if (srcid < 1 || srcid >= PLIC_SRCCNT){
        return;
    }

    // Find the address for enable bit of Interrupt Source No.srcno
    volatile uint32_t* add_ctx_disable = (volatile uint32_t*) ((uintptr_t)(PLIC_ENABLE_CONTEXT0 + ctxno * ENABLE_SIZE));
    volatile uint32_t* add_disable = (volatile uint32_t*) ((uintptr_t)(add_ctx_disable + 4 * (srcid / ADDRESS_SIZE)));
    *add_disable &= ~(1 << (srcid % ADDRESS_SIZE));
}

void plic_set_context_threshold(uint32_t ctxno, uint32_t level) {
    // FIXME your code goes here

    // Input:
    //      ctxno -- PLIC context ID  
    //      level -- the value of threshold
    // Output:
    //      none
    // Side effect:
    //      Sets the interrupt priority threshold for a specific context.
    //      Interrupts with a priority lower than the threshold will not be handled by the context.

    // Find the address for priority threshold of context No.ctxno
    volatile uint32_t* add_thres = (volatile uint32_t*) ((uintptr_t)(PLIC_THRESHOLD_CONTEXT0 + ctxno * THRESHOLD_SIZE));
    *add_thres = level;
}

uint32_t plic_claim_context_interrupt(uint32_t ctxno) {
    // FIXME your code goes here

    // Input:
    //      ctxno: PLIC context ID
    // Output:
    //      Interrupt ID with the highest priority -- If pending 
    //      0 -- Otherwise
    // side effect:
    //      Claims an interrupt for a given context.
    //      This function reads from the claim register and returns the interrupt ID of the highest-priority pending interrupt. 
    //      It returns 0 if no interrupts are pending.

    uint32_t result;

    // Find the address for claim of context No.ctxno
    volatile uint32_t* add_claim = (volatile uint32_t*) ((uintptr_t)(PLIC_CLAIM_CONTEXT0 + ctxno * CLAIM_SIZE));
    result = *add_claim;

    return result;
}

void plic_complete_context_interrupt(uint32_t ctxno, uint32_t srcno) {
    // FIXME your code goes here

    // Input:
    //      ctxno: PLIC context ID 
    //      srcno: Interrupt source ID
    // Output:
    //      None
    // Side effect:
    //      Completes the handling of an interrupt for a given context.
    //      This function writes the interrupt source number back to the claim register, 
    //      notifying the PLIC that the interrupt has been serviced

    // Check if the source ID is available
    if (srcno < 1 || srcno >= PLIC_SRCCNT){
        return;
    }

    // Find the address for claim of context No.ctxno
    volatile uint32_t* add_claim = (volatile uint32_t*) ((uintptr_t)(PLIC_CLAIM_CONTEXT0 + ctxno * CLAIM_SIZE));
    *add_claim = srcno;
}