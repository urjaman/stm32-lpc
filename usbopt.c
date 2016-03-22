#include <stdlib.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/stm32/st_usbfs.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/cortex.h>
/* Messing with your privates ;) */
#include <../lib/usb/usb_private.h>
#include <../lib/stm32/common/st_usbfs_core.h>

#include "main.h"
#include "debug.h"
#include "usbopt.h"

static inline void usbcopy(void* to, void* from, uint32_t n)
{
	uint32_t r;
	asm volatile (
		".syntax unified\n\t"
		".align 2\n\t"
		"0: subs %[n], #2\n\t"
		"ldrh %[dr], [%[from], %[n]]\n\t"
		"strh %[dr], [%[to], %[n]]\n\t"
		"bne 0b\n\t"
		".syntax divided\n\t"
		: [dr] "=&l" (r), [n] "+l" (n)
		: [from] "l" (from), [to] "l" (to)
	);
}

uint16_t usbopt_copy_rx(uint8_t addr, uint16_t* to) {
	if ((*USB_EP_REG(addr) & USB_EP_RX_STAT) == USB_EP_RX_STAT_VALID) {
		return 0;
	}
	uint16_t len = USB_GET_EP_RX_COUNT(addr) & 0x3ff;
	void* from = (uint16_t*)USB_GET_EP_RX_BUFF(addr);
	usbcopy(to, from, len+(len&1));
	USB_SET_EP_RX_STAT(addr, USB_EP_RX_STAT_VALID);
	return len;
}

uint16_t usbopt_copy_tx(uint8_t addr, uint32_t * from, uint16_t len) {
	addr &= 0x7F;
	if ((*USB_EP_REG(addr) & USB_EP_TX_STAT) == USB_EP_TX_STAT_VALID) {
		return 0;
	}
	void *to = USB_GET_EP_TX_BUFF(addr);
	usbcopy(to, from, len+(len&1));
	USB_SET_EP_TX_COUNT(addr, len);
	USB_SET_EP_TX_STAT(addr, USB_EP_TX_STAT_VALID);
	return len;
}


void usb_ep_set_doublebuffer(usbd_device *dev, uint8_t addr, uint16_t max_size)
{
	uint8_t dir = addr & 0x80;
	addr &= 0x7f;
	if (dir) {
		/* It's a TX, it already has TX stuff set. */
		USB_SET_EP_RX_ADDR(addr, dev->pm_top);
		st_usbfs_set_ep_rx_bufsize(dev, addr, max_size);
		dev->pm_top += max_size;
		USB_SET_EP_RX_STAT(addr, USB_EP_RX_STAT_DISABLED);
		/* RX DTOG, or SW_BUF clear it. Also set EP_KIND for double-buffer. */
		USB_CLR_EP_RX_DTOG(addr);
		USB_SET_EP_KIND(addr);
	} else {
		/* RX, as per above RX stuff set. */
		USB_SET_EP_TX_ADDR(addr, dev->pm_top);
		USB_SET_EP_TX_STAT(addr, USB_EP_TX_STAT_DISABLED);
                dev->pm_top += max_size;
		/* TX DTOG, or SW_BUF clear it. Also set EP_KIND for double-buffer. */
		USB_CLR_EP_TX_DTOG(addr);
		USB_SET_EP_KIND(addr);
	}

}

static uint8_t usb_ep_rx_ctr[8] = { 0 };

void usbopt_rx_dblbuf_cb(usbd_device *usbd_dev, uint8_t ep) {
	/* ISR, thus we can safely increment. (Nobody is gonna interrupt us and lookk at this array :P */
	usb_ep_rx_ctr[ep]++;
	USB_CLR_EP_RX_CTR(ep);
}



uint16_t usbopt_copy_rx_dblbuf(uint8_t addr, uint16_t* to) {
	cm_disable_interrupts();
	if (!usb_ep_rx_ctr[addr]) {
		cm_enable_interrupts();
		return 0;
	}
	usb_ep_rx_ctr[addr]--;
	cm_enable_interrupts();
	int sw_buf = *USB_EP_REG(addr) & USB_EP_TX_DTOG;
	uint16_t len = sw_buf ?  USB_GET_EP_RX_COUNT(addr) & 0x3ff : USB_GET_EP_TX_COUNT(addr);
	void* from = sw_buf ? USB_GET_EP_RX_BUFF(addr) : USB_GET_EP_TX_BUFF(addr);
	usbcopy(to, from, len+(len&1));
	/* Toggle dem DTOG.... umm SW_BUF. */
	*USB_EP_REG(addr) = (*USB_EP_REG(addr) & USB_EP_NTOGGLE_MSK) | (USB_EP_RX_CTR|USB_EP_TX_CTR) | USB_EP_TX_DTOG;
	//USB_SET_EP_RX_STAT(addr, USB_EP_RX_STAT_VALID);
	return len;
}

