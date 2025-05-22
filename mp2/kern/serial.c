// serial.c - NS16550a serial port
// 

#include "serial.h"
#include "intr.h"
#include "halt.h"
#include "thread.h"

#include <stddef.h>
#include <stdint.h>

// COMPILE-TIME CONSTANT DEFINITIONS
//

#ifndef UART0_MMIO_BASE
#define UART0_MMIO_BASE 0x10000000
#endif

#ifndef UART1_MMIO_BASE
#define UART1_MMIO_BASE 0x10000100
#endif

#ifndef UART0_IRQNO
#define UART0_IRQNO 10
#endif

#ifndef SERIAL_RBUFSZ
#define SERIAL_RBUFSZ 64
#endif

// EXPORTED VARIABLE DEFINITIONS
//

char serial_initialized = 0;

char com0_initialized = 0;
char com1_initialized = 0;

// INTERNAL TYPE DEFINITIONS
// 

struct ns16550a_regs {
    union {
        char rbr; // DLAB=0 read
        char thr; // DLAB=0 write
        uint8_t dll; // DLAB=1
    };
    
    union {
        uint8_t ier; // DLAB=0
        uint8_t dlm; // DLAB=1
    };
    
    union {
        uint8_t iir; // read
        uint8_t fcr; // write
    };

    uint8_t lcr;
    uint8_t mcr;
    uint8_t lsr;
    uint8_t msr;
    uint8_t scr;
};

#define LCR_DLAB (1 << 7)
#define LSR_OE (1 << 1)
#define LSR_DR (1 << 0)
#define LSR_THRE (1 << 5)
#define IER_DRIE (1 << 0)
#define IER_THREIE (1 << 1)


struct ringbuf {
    uint16_t hpos; // head of queue (from where elements are removed)
    uint16_t tpos; // tail of queue (where elements are inserted)
    char data[SERIAL_RBUFSZ];
};

struct uart {
    volatile struct ns16550a_regs * regs;
    void (*putc)(struct uart * uart, char c);
    char (*getc)(struct uart * uart);
    struct condition rxbuf_not_empty;
    struct condition txbuf_not_full;
    struct ringbuf rxbuf;
    struct ringbuf txbuf;
};

// INTERAL FUNCTION DECLARATIONS
//

struct uart * com_init_common(int k);

static void com_putc_sync(struct uart * uart, char c);
static char com_getc_sync(struct uart * uart);

static void com_putc_async(struct uart * uart, char c);
static char com_getc_async(struct uart * uart);

static void uart_isr(int irqno, void * aux);

static void rbuf_init(struct ringbuf * rbuf);
static int rbuf_empty(const struct ringbuf * rbuf);
static int rbuf_full(const struct ringbuf * rbuf);
static void rbuf_put(struct ringbuf * rbuf, char c);
static char rbuf_get(struct ringbuf * rbuf);

static void rbuf_wait_not_empty(const struct ringbuf * rbuf);
static void rbuf_wait_not_full(const struct ringbuf * rbuf);


// INTERNAL MACRO DEFINITIONS
//



// INTERNAL GLOBAL VARIABLES
//

static struct uart uarts[NCOM];

// EXPORTED FUNCTION DEFINITIONS
//

void com_init_sync(int k) {
    struct uart * uart;

    assert (0 <= k && k < NCOM);

    uart = com_init_common(k);
    uart->putc = &com_putc_sync;
    uart->getc = &com_getc_sync;
}

void com_init_async(int k) {
    struct uart * uart;

    assert (0 <= k && k < NCOM);

    uart = com_init_common(k);
    uart->putc = &com_putc_async;
    uart->getc = &com_getc_async;

    rbuf_init(&uart->rxbuf);
    rbuf_init(&uart->txbuf);

    condition_init(&uart->rxbuf_not_empty, "rxbuf_not_empty");
    condition_init(&uart->txbuf_not_full, "txbuf_not_full");

    // Register ISR and enable the IRQ
    intr_register_isr(UART0_IRQNO + k, 1, uart_isr, uart);
    intr_enable_irq(UART0_IRQNO + k);

    // Enable data ready interrupts
    uart->regs->ier = IER_DRIE;
}

int com_initialized(int k) {
    assert (0 <= k && k < NCOM);
    return (uarts[k].putc != NULL);
}

void com_putc(int k, char c) {
    assert (0 <= k && k < NCOM);
    uarts[k].putc(uarts+k, c);
}

char com_getc(int k) {
    assert (0 <= k && k < NCOM);
    return uarts[k].getc(uarts+k);
}

// The following four functions are for compatibility with CP1.

void com0_init(void) {
    com_init_sync(0);
    com0_initialized = 1;
}

void com0_putc(char c) {
    com_putc(0, c);
}

char com0_getc(void) {
    return com_getc(0);
}

void com1_init(void) {
    com_init_async(1);
    com1_initialized = 1;
}

void com1_putc(char c) {
    com_putc(1, c);
}

char com1_getc(void) {
    return com_getc(1);
}

// INTERNAL FUNCTION DEFINITIONS
//

// Common initialization for sync and async com ports. Specifically:
// 
// 1. Initializes uart->regs,
// 2. Configures UART hardware divisor register, and
// 3. Flushes RBR and clears IER.
// 
// Returns a pointer to the uart object (for convenience).
//

struct uart * com_init_common(int k) {
    struct uart * const uart = uarts+k;
    assert (0 <= k && k < NCOM);

    uart->regs =
        (void*)UART0_MMIO_BASE +
        k*(UART1_MMIO_BASE-UART0_MMIO_BASE);

    // Configure UART0. We set the baud rate divisor to 1, the lowest value,
    // for the fastest baud rate. In a physical system, the actual baud rate
    // depends on the attached oscillator frequency. In a virtualized system,
    // it doesn't matter.
    
    uart->regs->lcr = LCR_DLAB;
    // fence o,o
    uart->regs->dll = 0x01;
    uart->regs->dlm = 0x00;
    // fence o,o
    uart->regs->lcr = 0; // DLAB=0

    uart->regs->rbr; // flush receive buffer
    uart->regs->ier = 0;

    return uart;
}

void com_putc_sync(struct uart * uart, char c) {
    // Spin until THR is empty (THRE=1)
    while (!(uart->regs->lsr & LSR_THRE))
        continue;

    uart->regs->thr = c;
}

char com_getc_sync(struct uart * uart) {
    // Spin until RBR contains a byte (DR=1)
    while (!(uart->regs->lsr & LSR_DR))
        continue;
    
    return uart->regs->rbr;
}

void com_putc_async(struct uart * uart, char c) {
    // FIXME your code goes here

    // Input:
    //      uart: the UART whose buffer is used to put character
    //      char: the character to be put
    // Output:
    //      None
    // Side effect:
    //      Write a character to the UART asynchronously
    //      Write the character c to the UART transmit ring buffer. If the transmit ring buffer is full
    //      it must wait for the condition txbuf_not_full to be signaled. After writing the character to the buffer,
    //      it should enable the transmit interrupt.

    // Disable interrupt
    int s = intr_disable();

    while(rbuf_full(&uart->txbuf)){
        // Wait for the condition txbuf not full to be signaled
        condition_wait(&uart->txbuf_not_full);
    }

    rbuf_put(&uart->txbuf, c);
    // Restore interrupt
    intr_restore(s);
    uart->regs->ier |= IER_THREIE;
}

char com_getc_async(struct uart * uart) {
    // FIXME your code goes here

    // Input:
    //      uart: the UART whose buffer which the character is from
    // Output:
    //      character from the UART buffer
    // Side effect:
    //      Read a character to the UART asynchronously
    //      Read a character from the UART receive buffer. If the receive ring buffer is empty,
    //      it must wait for the condition rxbuf_not_empty to be signaled. 
    //      After reading the character from the ring buffer, it should enable the data ready interrupt

    // Disable interrupt
    int s = intr_disable();

    while(rbuf_empty(&uart->rxbuf)){
        // Wait for the condition rxbuf not empty to be signaled
        condition_wait(&uart->rxbuf_not_empty);
    }

    char c = rbuf_get(&uart->rxbuf);
    // Restore interrupt
    intr_restore(s);
    uart->regs->ier |= IER_DRIE;
    return c;
}

static void uart_isr(int irqno, void * aux) {
    struct uart * const uart = aux;
    uint_fast8_t line_status;

    line_status = uart->regs->lsr;

    if (line_status & LSR_OE)
        panic("Receive buffer overrun");
    
    // FIXME your code goes here

    // Input:
    //      irqno: The Interrupt source ID
    //      aux: The arguments for Interrupt handler function
    // Output:
    //      None
    // Side effect:
    //    The interrupt service routine.
    //    It will manage the interrupts when data need to be read/write.

    // Handle DR case
    if (line_status & LSR_DR) {
        if (!rbuf_full(&uart->rxbuf)) {
            rbuf_put(&uart->rxbuf, uart->regs->rbr);
            // Cause sleeping threads to wake up
            condition_broadcast(&uart->rxbuf_not_empty);
        } else {
            uart->regs->ier &= ~IER_DRIE;
        }
    }

    // Handle THRE case
    if (line_status & LSR_THRE) {
        if (!rbuf_empty(&uart->txbuf)) {
            uart->regs->thr = rbuf_get(&uart->txbuf);
            // Cause sleeping threads to wake up  
            condition_broadcast(&uart->txbuf_not_full); 
        } else {
            uart->regs->ier &= ~IER_THREIE; 
        }
    }
}

void rbuf_init(struct ringbuf * rbuf) {
    rbuf->hpos = 0;
    rbuf->tpos = 0;
}

int rbuf_empty(const struct ringbuf * rbuf) {
    return (rbuf->hpos == rbuf->tpos);
}

int rbuf_full(const struct ringbuf * rbuf) {
    return ((uint16_t)(rbuf->tpos - rbuf->hpos) == SERIAL_RBUFSZ);
}

void rbuf_put(struct ringbuf * rbuf, char c) {
    uint_fast16_t tpos;

    tpos = rbuf->tpos;
    rbuf->data[tpos % SERIAL_RBUFSZ] = c;
    asm volatile ("" ::: "memory");
    rbuf->tpos = tpos + 1;
}

char rbuf_get(struct ringbuf * rbuf) {
    uint_fast16_t hpos;
    char c;

    hpos = rbuf->hpos;
    c = rbuf->data[hpos % SERIAL_RBUFSZ];
    asm volatile ("" ::: "memory");
    rbuf->hpos = hpos + 1;
    return c;
}