#include <stdlib.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/stm32/st_usbfs.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/cortex.h>

#include "main.h"
#include "debug.h"
#include "usbdma.h"
#include "mutex.h"

void usbdma_setup(void) {
	rcc_periph_clock_enable(RCC_DMA);

	dma_clear_interrupt_flags(DMA1, DMA_CHANNEL2, DMA_FLAGS);
	dma_clear_interrupt_flags(DMA1, DMA_CHANNEL3, DMA_FLAGS);

	dma_channel_reset(DMA1, DMA_CHANNEL2);
	dma_channel_reset(DMA1, DMA_CHANNEL3);

	dma_disable_channel(DMA1, DMA_CHANNEL2);
	dma_disable_channel(DMA1, DMA_CHANNEL3);

	dma_enable_mem2mem_mode(DMA1, DMA_CHANNEL2);
	dma_enable_mem2mem_mode(DMA1, DMA_CHANNEL3);


	dma_set_priority(DMA1, DMA_CHANNEL2, DMA_CCR_PL_LOW);
	dma_set_priority(DMA1, DMA_CHANNEL3, DMA_CCR_PL_LOW);

	dma_set_memory_size(DMA1, DMA_CHANNEL2,DMA_CCR_MSIZE_16BIT);
	dma_set_peripheral_size(DMA1, DMA_CHANNEL3,DMA_CCR_PSIZE_16BIT);

	dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL2);
	dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL3);

	dma_enable_peripheral_increment_mode(DMA1, DMA_CHANNEL2);
	dma_enable_peripheral_increment_mode(DMA1, DMA_CHANNEL3);

	dma_set_read_from_peripheral(DMA1, DMA_CHANNEL2);
	dma_set_read_from_memory(DMA1, DMA_CHANNEL3);

	dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL2);
	dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL3);

	nvic_set_priority(NVIC_DMA1_CHANNEL2_3_IRQ, 192);
	nvic_enable_irq(NVIC_DMA1_CHANNEL2_3_IRQ);

}



static mutex_t dma_rx_busy = MUTEX_UNLOCKED;
static uint8_t dma_rx_addr = 0;


uint16_t usbdma_start_rx(uint8_t addr,uint8_t *buf) {
	if (mutex_trylock(&dma_rx_busy)) {
		if ((*USB_EP_REG(addr) & USB_EP_RX_STAT) == USB_EP_RX_STAT_VALID) {
			mutex_unlock(&dma_rx_busy);
			return 0;
		}
		uint16_t len = USB_GET_EP_RX_COUNT(addr) & 0x3ff;
		uint16_t lenw = (len/2) + (len&1);
		volatile uint16_t * tb = (volatile uint16_t*)USB_GET_EP_RX_BUFF(addr);
		if (lenw<=1) {
			*(uint16_t*)buf = *tb;
			USB_SET_EP_RX_STAT(addr, USB_EP_RX_STAT_VALID);
			mutex_unlock(&dma_rx_busy);
			return len;
		}
		dma_rx_addr = addr;
		dma_set_number_of_data(DMA1, DMA_CHANNEL2, lenw);
		dma_set_peripheral_address(DMA1, DMA_CHANNEL2, (uint32_t)tb);
		dma_set_memory_address(DMA1, DMA_CHANNEL2, (uint32_t)buf);
		dma_enable_channel(DMA1, DMA_CHANNEL2);
		return len;
	}
	return 0;
}

int usbdma_rx_ready(uint8_t addr)
{
	if (dma_rx_busy) {
		if (dma_rx_addr != addr) return 1;
		return 0;
	}
	return 1;
}

static mutex_t dma_tx_busy = MUTEX_UNLOCKED;
static uint8_t dma_tx_addr = 0;
static uint16_t dma_tx_len = 0;
static void (*dma_tx_cb)(void) = 0;

uint16_t usbdma_start_tx(uint8_t addr, void(*cb)(void), uint8_t*buf, uint16_t len) {
	if (mutex_trylock(&dma_tx_busy)) {
		addr &= 0x7F;
		if ((*USB_EP_REG(addr) & USB_EP_TX_STAT) == USB_EP_TX_STAT_VALID) {
			mutex_unlock(&dma_tx_busy);
			return 0;
		}
		uint16_t lenw = (len/2) + (len&1);

		dma_tx_addr = addr;
		dma_tx_cb = cb;
		dma_tx_len = len;

		dma_set_number_of_data(DMA1, DMA_CHANNEL3, lenw);
		dma_set_peripheral_address(DMA1, DMA_CHANNEL3, (uint32_t)USB_GET_EP_TX_BUFF(addr));
		dma_set_memory_address(DMA1, DMA_CHANNEL3, (uint32_t)buf);
		dma_enable_channel(DMA1, DMA_CHANNEL3);
		return len;
	}
	return 0;
}


void dma1_channel2_3_isr(void) {
	/* RX Complete */
	if (dma_get_interrupt_flag(DMA1, DMA_CHANNEL2, DMA_TCIF)) {
		USB_SET_EP_RX_STAT(dma_rx_addr, USB_EP_RX_STAT_VALID);
		dma_disable_channel(DMA1, DMA_CHANNEL2);
		dma_clear_interrupt_flags(DMA1, DMA_CHANNEL2, DMA_TCIF);
		mutex_unlock(&dma_rx_busy);
	}

	/* TX Complete */
	if (dma_get_interrupt_flag(DMA1, DMA_CHANNEL3, DMA_TCIF)) {
		dma_disable_channel(DMA1, DMA_CHANNEL3);
		dma_clear_interrupt_flags(DMA1, DMA_CHANNEL3, DMA_TCIF);
		USB_SET_EP_TX_COUNT(dma_tx_addr, dma_tx_len);
		USB_SET_EP_TX_STAT(dma_tx_addr, USB_EP_TX_STAT_VALID);
		dma_tx_cb();
		mutex_unlock(&dma_tx_busy);
	}
}
