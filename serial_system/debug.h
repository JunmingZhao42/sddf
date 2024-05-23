#ifndef DEBUG_H
#define DEBUG_H

void print_address(void* addr);
void print_int(int num);
void ffiprintf(unsigned char *c, long clen, unsigned char *a, long alen);
void ffiprint_int(unsigned char *c, long clen, unsigned char *a, long alen);

void int_to_byte2(int i, unsigned char *b);
int byte2_to_int(unsigned char *b);
void int_to_byte8(int i, unsigned char *b);
int byte8_to_int(unsigned char *b);
#endif