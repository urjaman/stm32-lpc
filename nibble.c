/*
	This file was part of bbflash, now stm32-lpc.
	Copyright (C) 2013, Hao Liu and Robert L. Thompson
	Copyright (C) 2013 Urja Rannikko <urjaman@gmail.com>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "main.h"
#include "nibble.h"
#include "udelay.h"
#include "extuart.h"



#define FRAME_PORT			GPIOA
#define FRAME				GPIO5


void nibble_set_dir(uint8_t dir) {
	uint32_t moder = GPIOA_MODER & ~( GPIO_MODE_MASK(0) | GPIO_MODE_MASK(1) | GPIO_MODE_MASK(2) | GPIO_MODE_MASK(3) );
	if (!dir) {
		moder |= GPIO_MODE(0, GPIO_MODE_INPUT) | GPIO_MODE(1, GPIO_MODE_INPUT) |
				GPIO_MODE(2, GPIO_MODE_INPUT) | GPIO_MODE(3, GPIO_MODE_INPUT);
	} else {
		moder |= GPIO_MODE(0, GPIO_MODE_OUTPUT) | GPIO_MODE(1, GPIO_MODE_OUTPUT) |
				GPIO_MODE(2, GPIO_MODE_OUTPUT) | GPIO_MODE(3, GPIO_MODE_OUTPUT);
	}
	GPIOA_MODER = moder;

}

uint8_t nibble_read(void) {
	return gpio_get(GPIOA, GPIO0|GPIO1|GPIO2|GPIO3);
}

static void nibble_write_hi(uint8_t data) {
	uint32_t bsrr = data >> 4;
	bsrr = bsrr | ((bsrr ^ 0xF) << 16);
	GPIOA_BSRR = bsrr;
	while (nibble_read() != (bsrr&0xF)); // seems to work as a delay ;)
}

void nibble_write(uint8_t data) {
	uint32_t bsrr = data & 0xF;
	bsrr = bsrr | ((bsrr ^ 0xF) << 16);
	GPIOA_BSRR = bsrr;
	while (nibble_read() != (bsrr&0xF)); // seems to work as a delay ;)
}

#define clock_low() do { gpio_clear(CLK_PORT, CLK); nibdelay(); } while(0)
#define clock_high() do { gpio_set(CLK_PORT, CLK); nibdelay(); } while(0)



bool nibble_init(void) {
	int i;

	clock_high();
	gpio_set(FRAME_PORT, FRAME);
	nibble_set_dir(OUTPUT);
	nibble_write(0);
//	gpio_clear(GPIOA, GPIO7); // !RST

	for (i = 0; i < 24; i++)
		clock_cycle();
//	gpio_set(GPIOA, GPIO7); // !RST
	for (i = 0; i < 100; i++)
		clock_cycle();

	return true;
}

void nibble_cleanup(void) {
	/* TODO */
	nibble_set_dir(INPUT);
}

void clocked_nibble_write(uint8_t value) {
	clock_low();
	nibble_write(value);
	clock_high();
}

void clocked_nibble_write_hi(uint8_t value) {
	clock_low();
	nibble_write_hi(value);
	clock_high();
}

uint8_t clocked_nibble_read(void) {
	clock_cycle();
	nibdelay();
	nibdelay();
	nibdelay();
	nibdelay();
	return nibble_read();
}

void nibble_start(uint8_t start) {
	gpio_set(FRAME_PORT, FRAME);
	nibble_set_dir(OUTPUT);
	clock_high();
	gpio_clear(FRAME_PORT, FRAME);
	nibble_write(start);
	clock_cycle();
	gpio_set(FRAME_PORT, FRAME);
}

void nibble_hw_init(void) {
	/* port init */
	uint16_t porta_gpios = GPIO0|GPIO1|GPIO2|GPIO3|FRAME|GPIO7|GPIO15;
	uint16_t porta_gpios_pu_in = GPIO0|GPIO1|GPIO2|GPIO3;
	uint16_t porta_gpios_pu_out = FRAME|GPIO7;
	uint16_t porta_gpios_out = GPIO15;
	gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, porta_gpios_pu_in);
	gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, porta_gpios_pu_out);
	gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, porta_gpios_out);
	gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, porta_gpios);
	gpio_set(GPIOA, porta_gpios_pu_out | porta_gpios_out);

	gpio_mode_setup(CLK_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, CLK);
	gpio_set_output_options(CLK_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, CLK);

	/* Kick reset here so lpc/fwh.c doesnt need to know about how it is controlled. */
	gpio_clear(GPIOA, GPIO7); // !RST
	udelay(2);
	gpio_set(GPIOA, GPIO7);
}
