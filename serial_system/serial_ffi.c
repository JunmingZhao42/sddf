/*
  Implements the foreign function interface (FFI) used in the CakeML basis
  library, as a thin wrapper around the relevant system calls.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <stdint.h>
#include <microkit.h>
#include <string.h>
#include <sel4/sel4.h>
#include "serial.h"
#include "shared_ringbuffer.h"
#include "serial_driver_data.h"
#include "debug.h"

#ifdef EVAL
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>
#endif

/* This flag is on by default. It catches CakeML's out-of-memory exit codes
 * and prints a helpful message to stderr.
 * Note that this is not specified by the basis library.
 * */
#define STDERR_MEM_EXHAUST
#define CONFIG_ENABLE_BENCHMARKS
/* clFFI (command line) */

#define IRQ_CH 1
#define TX_CH 8
#define RX_CH 10

// Base of the uart registers
uintptr_t uart_base;

/* Shared Memory regions. These all have to be here to keep compiler happy */
// Ring handle components
uintptr_t rx_avail;
uintptr_t rx_used;
uintptr_t tx_avail;
uintptr_t tx_used;
uintptr_t shared_dma_vaddr;

/* Pointers to shared_ringbuffers */
ring_handle_t rx_ring;
ring_handle_t tx_ring;

static char cml_memory[1024*1024*2];

/* exported in cake.S */
extern void cml_main(void);
extern void handle_rx(void);
extern void handle_tx(void);
extern void handle_irq(void);
extern void *cml_heap;
extern void *cml_stack;
extern void *cml_stackend;

extern char cake_text_begin;
extern char cake_codebuffer_begin;
extern char cake_codebuffer_end;

void ffiget_shared_mem_addr(unsigned char *c, long clen, unsigned char *a, long alen) {
    if (alen != 1 || clen < 1) {
        microkit_dbg_puts("get_shared_mem_addr: There are no arguments supplied when args are expected");
        a[0] = 0;
        return;
    }
    switch (clen) {
        case 1:
            *(uintptr_t *) a = rx_avail;
            break;
        case 2:
            *(uintptr_t *) a = rx_used;
            break;
        case 3:
            *(uintptr_t *) a = tx_avail;
            break;
        case 4:
            *(uintptr_t *) a = tx_used;
            break;
        case 5:
            *(uintptr_t *) a = shared_dma_vaddr;
            break;
        case 6:
            *(uintptr_t *) a = uart_base;
            break;
        default:
            microkit_dbg_puts("get_shared_mem_addr: Invalid argument supplied");
            a[0] = 0;
            break;
    }
}

#ifdef EVAL

/* Signal handler for SIGINT */

/* This is set to 1 when the runtime traps a SIGINT */
volatile sig_atomic_t caught_sigint = 0;

void do_sigint(int sig_num)
{
    signal(SIGINT, do_sigint);
    caught_sigint = 1;
}

void ffipoll_sigint (unsigned char *c, long clen, unsigned char *a, long alen)
{
    if (alen < 1) {
        return;
    }
    a[0] = (unsigned char) caught_sigint;
    caught_sigint = 0;
}

void ffikernel_ffi (unsigned char *c, long clen, unsigned char *a, long alen) {
    for (long i = 0; i < clen; i++) {
        putc(c[i], stdout);
    }
}

#else

void ffipoll_sigint (unsigned char *c, long clen, unsigned char *a, long alen) { }

void ffikernel_ffi (unsigned char *c, long clen, unsigned char *a, long alen) { }

#endif

void cml_exit(int arg) {
    microkit_dbg_puts("ERROR! We should not be getting here\n");
}

void cml_err(int arg) {
    if (arg == 3) {
        microkit_dbg_puts("Memory not ready for entry. You may have not run the init code yet, or be trying to enter during an FFI call.\n");
    }

  cml_exit(arg);
}

/* Need to come up with a replacement for this clear cache function. Might be worth testing just flushing the entire l1 cache, but might cause issues with returning to this file*/
void cml_clear() {
    microkit_dbg_puts("Trying to clear cache\n");
}

/*
 * BaudRate = RefFreq / (16 * (BMR + 1)/(BIR + 1) )
 * BMR and BIR are 16 bit
 * Function taken from seL4 util_libs serial.c implementation for imx8mm
 */
static void imx_uart_set_baud(long bps)
{
    imx_uart_regs_t *regs = (imx_uart_regs_t *) uart_base;

    uint32_t bmr, bir, fcr;
    fcr = regs->fcr;
    fcr &= ~UART_FCR_RFDIV_MASK;
    fcr |= UART_FCR_RFDIV(4);
    bir = 0xf;
    bmr = UART_REF_CLK / bps - 1;
    regs->bir = bir;
    regs->bmr = bmr;
    regs->fcr = fcr;
}

int serial_configure(
    long bps,
    int char_size,
    enum serial_parity parity,
    int stop_bits)
{
    imx_uart_regs_t *regs = (imx_uart_regs_t *) uart_base;

    uint32_t cr2;
    /* Character size */
    cr2 = regs->cr2;
    if (char_size == 8) {
        cr2 |= UART_CR2_WS;
    } else if (char_size == 7) {
        cr2 &= ~UART_CR2_WS;
    } else {
        return -1;
    }
    /* Stop bits */
    if (stop_bits == 2) {
        cr2 |= UART_CR2_STPB;
    } else if (stop_bits == 1) {
        cr2 &= ~UART_CR2_STPB;
    } else {
        return -1;
    }
    /* Parity */
    if (parity == PARITY_NONE) {
        cr2 &= ~UART_CR2_PREN;
    } else if (parity == PARITY_ODD) {
        /* ODD */
        cr2 |= UART_CR2_PREN;
        cr2 |= UART_CR2_PROE;
    } else if (parity == PARITY_EVEN) {
        /* Even */
        cr2 |= UART_CR2_PREN;
        cr2 &= ~UART_CR2_PROE;
    } else {
        return -1;
    }
    /* Apply the changes */
    regs->cr2 = cr2;
    /* Now set the board rate */
    imx_uart_set_baud(bps);

    microkit_dbg_puts("Configured serial, enabling uart\n");

    return 0;
}

void init_post(unsigned char *c, long clen, unsigned char *a, long alen) {
    // Setup the ring buffer mechanisms here as well as init the global serial driver data

    imx_uart_regs_t *regs = (imx_uart_regs_t *) uart_base;


    /* Line configuration */
    int ret = serial_configure(115200, 8, PARITY_NONE, 1);

    if (ret != 0) {
        microkit_dbg_puts("Error occured during line configuration\n");
    }

    /* Enable the UART */
    regs->cr1 |= UART_CR1_UARTEN;                /* Enable The uart.                  */
    regs->cr2 |= UART_CR2_RXEN | UART_CR2_TXEN;  /* RX/TX enable                      */
    regs->cr2 |= UART_CR2_IRTS;                  /* Ignore RTS                        */
    regs->cr3 |= UART_CR3_RXDMUXDEL;             /* Configure the RX MUX              */
    /* Initialise the receiver interrupt.                                             */
    regs->cr1 &= ~UART_CR1_RRDYEN;               /* Disable recv interrupt.           */
    regs->fcr &= ~UART_FCR_RXTL_MASK;            /* Clear the rx trigger level value. */
    regs->fcr |= UART_FCR_RXTL(1);               /* Set the rx tigger level to 1.     */
    regs->cr1 |= UART_CR1_RRDYEN;                /* Enable recv interrupt.            */
}

void ffidequeue_avail(unsigned char *c, long clen, unsigned char *a, long alen) {
    if (clen != 1 || alen != 1) {
        microkit_dbg_puts("dequeue_avail: There are no arguments supplied when args are expected");
        a[0] = 0;
        return;
    }
    // c has the address of the ring
    ring_handle_t *ring = (ring_handle_t *) c;
    uintptr_t buffer = 0;
    uint64_t buffer_len = 0;
    void *cookie = 0;

    int result = dequeue_avail(ring, &buffer, &buffer_len, &cookie);
    ((uintptr_t *) a)[0] = buffer;
    ((uintptr_t *) a)[1] = result;
}

void ffidriver_dequeue(unsigned char *c, long clen, unsigned char *a, long alen) {
    if (clen != 1 || alen != 1) {
        microkit_dbg_puts("driver_dequeue: There are no arguments supplied when args are expected");
        c[0] = 1;
        return;
    }
    // c has the address of the ring
    ring_handle_t *ring = (ring_handle_t *) c;
    void *cookie = 0;
    uintptr_t buffer = 0;
    uint64_t buffer_len = 0;
    int result = driver_dequeue(ring->used_ring, &buffer, &buffer_len, &cookie);
    ((uintptr_t *) a)[0] = buffer;
    ((uintptr_t *) a)[1] = result;
    ((uintptr_t *) a)[2] = buffer_len;
}

void init_pancake_mem() {
    unsigned long sz = 1024*1024; // 1 MB unit\n",
    unsigned long cml_heap_sz = sz;    // Default: 1 MB heap\n", (* TODO: parameterise *)
    unsigned long cml_stack_sz = sz;   // Default: 1 MB stack\n", (* TODO: parameterise *)
    cml_heap = cml_memory;
    cml_stack = cml_heap + cml_heap_sz;
    cml_stackend = cml_stack + cml_stack_sz;
}

void init_pancake_data() {
    uintptr_t *heap = (uintptr_t *) cml_heap;
    heap[0] = uart_base;
    heap[1] = rx_avail;
    heap[2] = rx_used;
    heap[3] = tx_avail;
    heap[4] = tx_used;
    heap[5] = shared_dma_vaddr;
    heap[6] = 0;
}

/*
Placing these functions in here for now. These are the entry points required by the core platform, however,
we can only have 1 entry point in our pancake program. So we will have to have these entry points in our c code.
*/

// Init function required by CP for every PD
void init(void) {
    microkit_dbg_puts(microkit_name);
    microkit_dbg_puts(": elf PD init function running\n");

    unsigned char c[1];
    long clen = 1;
    unsigned char a[1];
    long alen = 1;

    // Call init_post here to setup the ring buffer regions. The init_post case in the notified
    // switch statement may be redundant. Init post is now in the serial_driver_data file

    init_post(c, clen, a, alen);
    init_pancake_mem();
    init_pancake_data();
    cml_main();
}

// Entry points that are invoked on a serial interrupt, or notifications from the server using the TX and RX channels
void notified(microkit_channel ch) {
    if (ch == TX_CH) {
        handle_tx();
    }
    if (ch == RX_CH) {
        handle_rx();
    }
    if (ch == IRQ_CH) {
        handle_irq();
        microkit_irq_ack(IRQ_CH);
    }
}