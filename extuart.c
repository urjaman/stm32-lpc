#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>

#include "extuart.h"

void extuart_init(void)
{
	rcc_periph_clock_enable(RCC_USART1);

	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9);

	gpio_set_af(GPIOA, GPIO_AF1, GPIO9);

	usart_set_baudrate(USART1, 115200);
	usart_set_databits(USART1, 8);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_stopbits(USART1, USART_CR2_STOP_1_0BIT);
	usart_set_mode(USART1, USART_MODE_TX);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);

	usart_enable(USART1);
}

void extuart_putc(uint8_t c)
{
	usart_send_blocking(USART1, c);
}

void extuart_sendstr(const char* p_)
{
	const uint8_t *p = (const uint8_t*)p_;
	while (*p) {
		extuart_putc(*p++);
	}
}

