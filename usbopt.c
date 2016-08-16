#include "main.h"
#include "dmacopy.h"
#include <libopencm3/stm32/st_usbfs.h>
#include <libopencm3/usb/usbd.h>
#include "usbopt.h"

static volatile uint8_t usbopt_addr;
static void usbopt_ep_write_cb(int r)
{
	(void)r;
	uint16_t epnr = *USB_EP_REG(usbopt_addr);
	uint16_t epnrm = (epnr & USB_EP_NTOGGLE_MSK) | USB_EP_RX_CTR | USB_EP_TX_CTR;
	int dblbuf = ((epnr&(USB_EP_TYPE|USB_EP_KIND)) == (USB_EP_TYPE_BULK|USB_EP_KIND));
	if (dblbuf) epnrm |= USB_EP_RX_DTOG; /* toggle SW_BUF */
	SET_REG(USB_EP_REG(usbopt_addr), epnrm  | ( (epnr&USB_EP_TX_STAT) ^ USB_EP_TX_STAT_VALID) );
	usbopt_addr = 0;
}

uint16_t usbopt_ep_write_packet(usbd_device *dev, uint8_t addr, const void *buf, uint16_t len)
{
	(void)dev;
	addr &= 0x7F;
	if (usbopt_addr) {
		return 0;
	}
	uint16_t epnr = *USB_EP_REG(addr);
	int dblbuf = ((epnr&(USB_EP_TYPE|USB_EP_KIND)) == (USB_EP_TYPE_BULK|USB_EP_KIND));	
	if ((epnr & USB_EP_TX_STAT) == USB_EP_TX_STAT_VALID) {
		if (dblbuf) {
			/* Double-buffered, checking for SW_BUF == DTOG (==full, when status is valid) */
			if (!(((epnr >> 8) ^ epnr) & USB_EP_TX_DTOG)) { // here this is just the lower DTOG (0x40)
				return 0;
			}
		} else {
			return 0;
		}
	}
	void* tgtp;
	if (!dblbuf) {
		tgtp = USB_GET_EP_TX_BUFF(addr);
		USB_SET_EP_TX_COUNT(addr, len);
	} else {
		if (epnr&USB_EP_RX_DTOG) { /* SW_BUF */
			/*TX1*/
			USB_SET_EP_RX_COUNT(addr, len);
			tgtp = USB_GET_EP_RX_BUFF(addr);
		} else { 
			/*TX0*/
			USB_SET_EP_TX_COUNT(addr, len);
			tgtp = USB_GET_EP_TX_BUFF(addr);
		}
	}
	uint16_t words = (len>>1) + (len&1);
	usbopt_addr = addr;
	dmacpy_w(tgtp, buf, words, usbopt_ep_write_cb);
	return len;
}


static void (* volatile usbopt_read_cb)(int) = 0;
static uint16_t usbopt_read_len;
static uint8_t usbopt_read_addr;

static void usbopt_ep_read_cb(int r)
{
	(void)r;
	uint16_t epnr = *USB_EP_REG(usbopt_read_addr);
	int dblbuf = ((epnr&(USB_EP_TYPE|USB_EP_KIND)) == (USB_EP_TYPE_BULK|USB_EP_KIND));
	uint16_t epnrm = (epnr & USB_EP_NTOGGLE_MSK) | USB_EP_RX_CTR | USB_EP_TX_CTR;
	if (dblbuf) epnrm |= USB_EP_TX_DTOG; /* toggle SW_BUF */
	SET_REG(USB_EP_REG(usbopt_read_addr), epnrm  | ( (epnr&USB_EP_RX_STAT) ^ USB_EP_RX_STAT_VALID) );
	void (*cb)(int) = usbopt_read_cb;
	usbopt_read_cb = 0;
	cb(usbopt_read_len);
}

uint16_t usbopt_ep_read_packet(usbd_device *dev, uint8_t addr, void *buf, uint16_t len, void(*cb)(int))
{
	(void)dev;
	if (usbopt_read_cb)
		return 0;
	
	uint16_t epnr = *USB_EP_REG(addr);
	int dblbuf = ((epnr&(USB_EP_TYPE|USB_EP_KIND)) == (USB_EP_TYPE_BULK|USB_EP_KIND));
	if ((epnr & USB_EP_RX_STAT) == USB_EP_RX_STAT_VALID) {
		if (dblbuf) {
			/* Double-buffered, checking for SW_BUF == DTOG (==empty, when status is valid) */
			if (!(((epnr >> 8) ^ epnr) & USB_EP_TX_DTOG)) { // here this is just the lower DTOG (0x40)
				return 0;
			}
		} else {
			return 0;
		}
	}
	uint16_t ldat;
	const void* src;
	if (!dblbuf) {
		ldat = USB_GET_EP_RX_COUNT(addr);
		src = USB_GET_EP_RX_BUFF(addr);
	} else {
		if (epnr&USB_EP_TX_DTOG) { /* SW_BUF */
			/*RX1*/
			ldat = USB_GET_EP_RX_COUNT(addr);
			src = USB_GET_EP_RX_BUFF(addr);
		} else {
			/*RX0*/
			ldat = USB_GET_EP_TX_COUNT(addr);
			src = USB_GET_EP_TX_BUFF(addr);
		}
	}
	ldat &= 0x3FF;
	if (ldat < len) len = ldat;
	uint16_t words = (len>>1) + (len&1);
	usbopt_read_addr = addr;
	usbopt_read_len = len;
	usbopt_read_cb = cb;
	dmacpy_w(buf, src, words, usbopt_ep_read_cb);
	return len;
}
