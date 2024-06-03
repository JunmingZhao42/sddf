#ifndef DEBUG_H
#define DEBUG_H

void print_int(int num);
void print_address(void* addr);
void ffiprint_int(unsigned char *c, long clen, unsigned char *a, long alen);
void ffiprint_char(unsigned char *c, long clen, unsigned char *a, long alen);
void ffiprint_string(unsigned char *c, long clen, unsigned char *a, long alen);
void ffiprint_address(unsigned char *c, long clen, unsigned char *a, long alen);
void ffitransfer_hw(unsigned char *c, long clen, unsigned char *a, long alen);
void ffifuncall(unsigned char *c, long clen, unsigned char *a, long alen);

#endif