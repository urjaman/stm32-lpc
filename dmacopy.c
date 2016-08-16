#include "main.h"
#include <string.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/cortex.h>
#include "dmacopy.h"

void dmacpy_setup(void) {
	rcc_periph_clock_enable(RCC_DMA);

	dma_clear_interrupt_flags(DMA1, DMA_CHANNEL1, DMA_FLAGS);
	dma_channel_reset(DMA1, DMA_CHANNEL1);
	dma_disable_channel(DMA1, DMA_CHANNEL1);

	/* This chunk should be changed to one constant write. */
	dma_enable_mem2mem_mode(DMA1, DMA_CHANNEL1);
	dma_set_priority(DMA1, DMA_CHANNEL1, DMA_CCR_PL_LOW);
	dma_set_memory_size(DMA1, DMA_CHANNEL1, DMA_CCR_MSIZE_16BIT);
	dma_set_peripheral_size(DMA1, DMA_CHANNEL1, DMA_CCR_PSIZE_16BIT);
	dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL1);
	dma_enable_peripheral_increment_mode(DMA1, DMA_CHANNEL1);
	dma_set_read_from_peripheral(DMA1, DMA_CHANNEL1);
	dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL1);

	nvic_set_priority(NVIC_DMA1_CHANNEL1_IRQ, 192);
	nvic_enable_irq(NVIC_DMA1_CHANNEL1_IRQ);

}

/* Callback used as the busy flag. */
static void (* volatile dmacpy_cb)(int) = 0;

void dma1_channel1_isr(void) {
	void (*cb)(int) = dmacpy_cb;
	dmacpy_cb = 0;
	dma_disable_channel(DMA1, DMA_CHANNEL1);
	dma_clear_interrupt_flags(DMA1, DMA_CHANNEL1, DMA_TCIF);
	cb(0);
}

int dmacpy_w(void *dest, const void* src, size_t words, void(*cb)(int))
{
	while (dmacpy_cb) yield();
#if 0
	// it sounds like it'd make sense to avoid the programming of the DMAC and ISR for tiny transfers, but benchmark later...
	if (words<=1) {
		*(uint16_t*)dest = *(uint16_t*)src;
		cb(0);
		return 0;
	}
#endif
	dmacpy_cb = cb;
	dma_set_number_of_data(DMA1, DMA_CHANNEL1, words);
	dma_set_peripheral_address(DMA1, DMA_CHANNEL1, (uint32_t)src);
	dma_set_memory_address(DMA1, DMA_CHANNEL1, (uint32_t)dest);
	dma_enable_channel(DMA1, DMA_CHANNEL1);
	return 0;
}
