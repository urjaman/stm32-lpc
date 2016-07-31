#include "main.h"
#include "dmacopy.h"
#include <libopencm3/stm32/st_usbfs.h>
#include <libopencm3/usb/usbd.h>
#include "usbopt.h"

static volatile uint8_t usbopt_addr;
static void usbopt_ep_write_cb(int r)
{
	(void)r;
	USB_SET_EP_TX_STAT(usbopt_addr, USB_EP_TX_STAT_VALID);
	usbopt_addr = 0;
}

uint16_t usbopt_ep_write_packet(usbd_device *dev, uint8_t addr, const void *buf, uint16_t len)
{
	(void)dev;
	addr &= 0x7F;

	if (usbopt_addr) {
		return 0;
	}

	if ((*USB_EP_REG(addr) & USB_EP_TX_STAT) == USB_EP_TX_STAT_VALID) {
		return 0;
	}

	void *a = USB_GET_EP_TX_BUFF(addr);
	USB_SET_EP_TX_COUNT(addr, len);
	uint16_t words = (len>>1) + (len&1);
	usbopt_addr = addr;
	dmacpy_w(a, buf, words, usbopt_ep_write_cb);
	return len;
}
