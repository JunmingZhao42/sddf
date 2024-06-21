#include "lwip/pbuf.h"

void ffipbuf_free(unsigned char *c, long clen, unsigned char *a, long alen) {
    // clen is the address of the pbuf to free
    pbuf_free((struct pbuf *) c);
}