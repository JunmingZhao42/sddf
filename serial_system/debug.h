#ifndef DEBUG_H
#define DEBUG_H

void print_address(void* addr);
void print_int(int num);
void ffiprintf(unsigned char *c, long clen, unsigned char *a, long alen);
void ffiprint_int(unsigned char *c, long clen, unsigned char *a, long alen);
void ffitransfer_hw(unsigned char *c, long clen, unsigned char *a, long alen);
void ffifuncall(unsigned char *c, long clen, unsigned char *a, long alen);

#endif