/* This file is originally from https://github.com/dword1511/stm32-vserprog		  *
 * revision 638cf019f54493386d09bc57e2cf23548c6bec00 , author "dword1511", license: GPLv3 */

#ifndef __STM32_VSERPOG_USB_CDC_H__
#define __STM32_VSERPOG_USB_CDC_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define USBCDC_PKT_SIZE_DAT 64
#define USBCDC_PKT_SIZE_INT 16

extern volatile bool usb_ready;

void     usbcdc_init(void);
uint16_t usbcdc_write(void *buf, size_t len);
void usbcdc_putc(uint8_t c);
uint8_t usbcdc_getc(void);

#endif /* __STM32_VSERPOG_USB_CDC_H__ */
