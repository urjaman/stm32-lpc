#ifndef _USBOPT_H_
#define _USBOPT_H_
#include <stdint.h>

/* These are for no-callback single-buffered use. */
uint16_t usbopt_copy_rx(uint8_t addr, uint16_t* to);
uint16_t usbopt_copy_tx(uint8_t addr, uint32_t * from, uint16_t len);

/* This twiddles an ep to double-buffer mode, given it has already been setup with the normal code. */
void usb_ep_set_doublebuffer(usbd_device *dev, uint8_t addr, uint16_t max_size);

void usbopt_rx_dblbuf_cb(usbd_device *usbd_dev, uint8_t ep);

/* This is for callback=above, double-buffered use. */
uint16_t usbopt_copy_rx_dblbuf(uint8_t addr, uint16_t* to);


#endif

