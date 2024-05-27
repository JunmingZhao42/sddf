#include <stdio.h>
#include <stdlib.h>
#include <microkit.h>

void print_address(void* addr) {
    char buf[16];
    int i;
    unsigned long int_addr = (unsigned long)addr;

    // Convert the address to a string representation
    for (i = 0; i < 16; i++) {
        unsigned char nibble = (int_addr >> (4 * (15 - i))) & 0xF;
        if (nibble < 10) {
            buf[i] = '0' + nibble;
        } else {
            buf[i] = 'A' + (nibble - 10);
        }
    }

    // Print the "0x" prefix
    microkit_dbg_putc('0');
    microkit_dbg_putc('x');

    // Print the address string
    for (i = 0; i < 16; i++) {
        microkit_dbg_putc(buf[i]);
    }
    microkit_dbg_putc('\n');
}

void print_int(int num) {
    if (num < 0) {
        microkit_dbg_putc('-');
        num = -num;
    }

    if (num == 0) {
        microkit_dbg_putc('0');
        return;
    }

    char buf[10];
    int i = 0;

    while (num != 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }

    while (i > 0) {
        microkit_dbg_putc(buf[--i]);
    }
}

void ffiprintf(unsigned char *c, long clen, unsigned char *a, long alen) {
    microkit_dbg_puts("my printf: ");
    microkit_dbg_putc(((char) clen) + 48);
    microkit_dbg_putc(',');
    microkit_dbg_putc(((char) alen) + 48);
    microkit_dbg_putc('\n');
}

void ffiprint_int(unsigned char *c, long clen, unsigned char *a, long alen) {

    microkit_dbg_puts("The address of c is -- (");
    microkit_dbg_puts(c);
    microkit_dbg_puts(")\n");

    char arg = c[0];

    microkit_dbg_puts("We received the following int --- (");
    microkit_dbg_puts(arg);
    microkit_dbg_puts(")\n");
}

/*
    * helper function to copy half word from *c to *a
    * TODO: should be replaced later by pancake
    * c: memory address
    * a: memory address
*/
void ffitransfer_hw(unsigned char *c, long clen, unsigned char *a, long alen) {
    if (clen != 1 || alen != 1) {
        microkit_dbg_puts("ld_hw: There are no arguments supplied when args are expected");
        c[0] = 0;
        return;
    }
    uint32_t value = *(uint32_t *)c;
    *(uint32_t *) a = value;
}

void ffifuncall(unsigned char *c, long clen, unsigned char *a, long alen) {
    if (clen != 1) {
        microkit_dbg_puts("ld_hw: There are no arguments supplied when args are expected");
        c[0] = 0;
        return;
    }
    // c is the address of the function to call
    void (*func)(void) = (void (*)(void))c;
    func();
}
