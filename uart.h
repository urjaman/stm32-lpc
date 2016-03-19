/* Dummy translator for libfrser */

#include "usbcdc.h"
#define RECEIVE() usbcdc_getc()
#define SEND(n) usbcdc_putc(n)
/* This is just the frser reported value. */
#define UART_BUFLEN 4096
