#include "main.h"
#include "flash.h"
#include "frser.h"
#include "uart.h"
#include "lpcfwh.h"
#include "nibble.h"
#include "extuart.h"

static uint8_t flash_prot_in_use=0;

void flash_set_safe(void)
{
	nibble_cleanup();
}

uint8_t flash_plausible_protocols(void)
{
	uint8_t prots = 0;
	flash_set_safe();
	if ((SUPPORTED_BUSTYPES&CHIP_BUSTYPE_LPC)&&(lpc_test())) {
		prots |= CHIP_BUSTYPE_LPC;
	}
	flash_set_safe();
	if ((SUPPORTED_BUSTYPES&CHIP_BUSTYPE_FWH)&&(fwh_test())) {
		prots |= CHIP_BUSTYPE_FWH;
	}
	/* Just to restore the HW state. */
	flash_select_protocol(flash_prot_in_use);
	return prots;
}

void flash_select_protocol(uint8_t allowed_protocols)
{
	allowed_protocols &= SUPPORTED_BUSTYPES;
	flash_set_safe();
	if ((allowed_protocols&CHIP_BUSTYPE_LPC)&&(lpc_test())) {
		DBG("F:LPC\r\n");
		flash_prot_in_use = CHIP_BUSTYPE_LPC;
		return;
	}
	flash_set_safe();
	if ((allowed_protocols&CHIP_BUSTYPE_FWH)&&(fwh_test())) {
		DBG("F:FWH\r\n");
		flash_prot_in_use = CHIP_BUSTYPE_FWH;
		return;
	}
	DBG("F:NONE\r\n");
	flash_prot_in_use = 0;
	return;
}


uint8_t flash_read(uint32_t addr)
{
	switch (flash_prot_in_use) {
	case 0:
	default:
		return 0xFF;
	case CHIP_BUSTYPE_LPC:
		return lpc_read_address(addr);
	case CHIP_BUSTYPE_FWH:
		return fwh_read_address(addr);

	}
}

void flash_readn(uint32_t addr, uint32_t len)
{
	if (len==0) len = ((uint32_t)1<<24);
	switch (flash_prot_in_use) {
	case 0:
	default:
		while (len--) SEND(0xFF);
		return;
	case CHIP_BUSTYPE_LPC:
		while (len--) SEND(lpc_read_address(addr++));
		return;
	case CHIP_BUSTYPE_FWH:
		fwh_read_n(addr, len);
		return;
	}
}

void flash_write(uint32_t addr, uint8_t data)
{
	switch (flash_prot_in_use) {
	case 0:
	default:
		return;
	case CHIP_BUSTYPE_LPC:
		lpc_write_address(addr,data);
		return;
	case CHIP_BUSTYPE_FWH:
		fwh_write_address(addr,data);
		return;

	}
}

