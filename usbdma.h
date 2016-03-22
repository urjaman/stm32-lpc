#ifndef _USBDMA_H_
#define _USBDMA_H_
#include <stdint.h>
void usbdma_setup(void);
uint16_t usbdma_start_rx(uint8_t addr, uint8_t *buf);
int usbdma_rx_ready(uint8_t addr);
uint16_t usbdma_start_tx(uint8_t addr, void(*cb)(void), uint8_t*buf, uint16_t len);

#endif
