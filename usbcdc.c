/* This file is originally from https://github.com/dword1511/stm32-vserprog		  *
 * revision 638cf019f54493386d09bc57e2cf23548c6bec00 , author "dword1511", license: GPLv3 */

#include <stdlib.h>

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>
#include <libopencm3/stm32/st_usbfs.h>
#include <libopencm3/stm32/desig.h>
#include <libopencm3/cm3/cortex.h>
#include "main.h"
#include "debug.h"


#define UID_LEN  (12 * 2 + 1) /* 12-byte, each byte turnned into 2-byte hex, then '\0'. */

#define DEV_VID    0x0483 /* ST Microelectronics */
#define DEV_PID    0x5740 /* STM32 */
#define DEV_VER    0x0009 /* 0.9 */

/* Serprog channel */
#define EP_INT     0x83
#define EP_OUT     0x82
#define EP_IN      0x01

/* Extuart channel (TODO). */
#define EP2_INT     0x85
#define EP2_OUT     0x84
#define EP2_IN      0x04

/* Debug console channel */
#define EP3_INT     0x87
#define EP3_OUT     0x86
#define EP3_IN      0x06


#define STR_MAN    0x01
#define STR_PROD   0x02
#define STR_SER    0x03

#include "usbcdc.h"

static const struct usb_device_descriptor dev = {
	.bLength            = USB_DT_DEVICE_SIZE,
	.bDescriptorType    = USB_DT_DEVICE,
	.bcdUSB             = 0x0200,
	.bDeviceClass       = USB_CLASS_CDC,
	.bDeviceSubClass    = 0,
	.bDeviceProtocol    = 0,
	.bMaxPacketSize0    = USBCDC_PKT_SIZE_DAT,
	.idVendor           = DEV_VID,
	.idProduct          = DEV_PID,
	.bcdDevice          = DEV_VER,
	.iManufacturer      = STR_MAN,
	.iProduct           = STR_PROD,
	.iSerialNumber      = STR_SER,
	.bNumConfigurations = 1,
};

/*
 * This notification endpoint isn't implemented. According to CDC spec its
 * optional, but its absence causes a NULL pointer dereference in Linux
 * cdc_acm driver.
 */
static const struct usb_endpoint_descriptor comm_endp[] = {{
		.bLength            = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType    = USB_DT_ENDPOINT,
		.bEndpointAddress   = EP_INT,
		.bmAttributes       = USB_ENDPOINT_ATTR_INTERRUPT,
		.wMaxPacketSize     = USBCDC_PKT_SIZE_INT,
		.bInterval          = 255,
	}
};

static const struct usb_endpoint_descriptor data_endp[] = {{
		.bLength            = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType    = USB_DT_ENDPOINT,
		.bEndpointAddress   = EP_IN,
		.bmAttributes       = USB_ENDPOINT_ATTR_BULK,
		.wMaxPacketSize     = USBCDC_PKT_SIZE_DAT,
		.bInterval          = 1,
	}, {
		.bLength            = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType    = USB_DT_ENDPOINT,
		.bEndpointAddress   = EP_OUT,
		.bmAttributes       = USB_ENDPOINT_ATTR_BULK,
		.wMaxPacketSize     = USBCDC_PKT_SIZE_DAT,
		.bInterval          = 1,
	}
};

static const struct usb_endpoint_descriptor comm_endp2[] = {{
		.bLength            = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType    = USB_DT_ENDPOINT,
		.bEndpointAddress   = EP2_INT,
		.bmAttributes       = USB_ENDPOINT_ATTR_INTERRUPT,
		.wMaxPacketSize     = USBCDC_PKT_SIZE_INT,
		.bInterval          = 255,
	}
};

static const struct usb_endpoint_descriptor data_endp2[] = {{
		.bLength            = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType    = USB_DT_ENDPOINT,
		.bEndpointAddress   = EP2_IN,
		.bmAttributes       = USB_ENDPOINT_ATTR_BULK,
		.wMaxPacketSize     = USBCDC_PKT_SIZE_DAT,
		.bInterval          = 1,
	}, {
		.bLength            = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType    = USB_DT_ENDPOINT,
		.bEndpointAddress   = EP2_OUT,
		.bmAttributes       = USB_ENDPOINT_ATTR_BULK,
		.wMaxPacketSize     = USBCDC_PKT_SIZE_DAT,
		.bInterval          = 1,
	}
};

static const struct usb_endpoint_descriptor comm_endp3[] = {{
		.bLength            = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType    = USB_DT_ENDPOINT,
		.bEndpointAddress   = EP3_INT,
		.bmAttributes       = USB_ENDPOINT_ATTR_INTERRUPT,
		.wMaxPacketSize     = USBCDC_PKT_SIZE_INT,
		.bInterval          = 255,
	}
};

static const struct usb_endpoint_descriptor data_endp3[] = {{
		.bLength            = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType    = USB_DT_ENDPOINT,
		.bEndpointAddress   = EP3_IN,
		.bmAttributes       = USB_ENDPOINT_ATTR_BULK,
		.wMaxPacketSize     = USBCDC_PKT_SIZE_DAT,
		.bInterval          = 1,
	}, {
		.bLength            = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType    = USB_DT_ENDPOINT,
		.bEndpointAddress   = EP3_OUT,
		.bmAttributes       = USB_ENDPOINT_ATTR_BULK,
		.wMaxPacketSize     = USBCDC_PKT_SIZE_DAT,
		.bInterval          = 1,
	}
};


static const struct {
	struct usb_cdc_header_descriptor header;
	struct usb_cdc_call_management_descriptor call_mgmt;
	struct usb_cdc_acm_descriptor acm;
	struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm_functional_descriptors = {
	.header = {
		.bFunctionLength    = sizeof(struct usb_cdc_header_descriptor),
		.bDescriptorType    = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_HEADER,
		.bcdCDC = 0x0110,
	},
	.call_mgmt = {
		.bFunctionLength    = sizeof(struct usb_cdc_call_management_descriptor),
		.bDescriptorType    = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
		.bmCapabilities     = 0,
		.bDataInterface     = 1,
	},
	.acm = {
		.bFunctionLength    = sizeof(struct usb_cdc_acm_descriptor),
		.bDescriptorType    = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_ACM,
		.bmCapabilities     = 0,
	},
	.cdc_union = {
		.bFunctionLength    = sizeof(struct usb_cdc_union_descriptor),
		.bDescriptorType    = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_UNION,
		.bControlInterface  = 0,
		.bSubordinateInterface0 = 1,
	},
};

static const struct {
	struct usb_cdc_header_descriptor header;
	struct usb_cdc_call_management_descriptor call_mgmt;
	struct usb_cdc_acm_descriptor acm;
	struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm_functional_descriptors2 = {
	.header = {
		.bFunctionLength    = sizeof(struct usb_cdc_header_descriptor),
		.bDescriptorType    = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_HEADER,
		.bcdCDC = 0x0110,
	},
	.call_mgmt = {
		.bFunctionLength    = sizeof(struct usb_cdc_call_management_descriptor),
		.bDescriptorType    = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
		.bmCapabilities     = 0,
		.bDataInterface     = 3,
	},
	.acm = {
		.bFunctionLength    = sizeof(struct usb_cdc_acm_descriptor),
		.bDescriptorType    = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_ACM,
		.bmCapabilities     = 0,
	},
	.cdc_union = {
		.bFunctionLength    = sizeof(struct usb_cdc_union_descriptor),
		.bDescriptorType    = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_UNION,
		.bControlInterface  = 2,
		.bSubordinateInterface0 = 3,
	},
};

static const struct {
	struct usb_cdc_header_descriptor header;
	struct usb_cdc_call_management_descriptor call_mgmt;
	struct usb_cdc_acm_descriptor acm;
	struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm_functional_descriptors3 = {
	.header = {
		.bFunctionLength    = sizeof(struct usb_cdc_header_descriptor),
		.bDescriptorType    = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_HEADER,
		.bcdCDC = 0x0110,
	},
	.call_mgmt = {
		.bFunctionLength    = sizeof(struct usb_cdc_call_management_descriptor),
		.bDescriptorType    = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
		.bmCapabilities     = 0,
		.bDataInterface     = 5,
	},
	.acm = {
		.bFunctionLength    = sizeof(struct usb_cdc_acm_descriptor),
		.bDescriptorType    = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_ACM,
		.bmCapabilities     = 0,
	},
	.cdc_union = {
		.bFunctionLength    = sizeof(struct usb_cdc_union_descriptor),
		.bDescriptorType    = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_UNION,
		.bControlInterface  = 4,
		.bSubordinateInterface0 = 5,
	},
};


static const struct usb_interface_descriptor comm_iface[] = {{
		.bLength              = USB_DT_INTERFACE_SIZE,
		.bDescriptorType      = USB_DT_INTERFACE,
		.bInterfaceNumber     = 0,
		.bAlternateSetting    = 0,
		.bNumEndpoints        = 1,
		.bInterfaceClass      = USB_CLASS_CDC,
		.bInterfaceSubClass   = USB_CDC_SUBCLASS_ACM,
		.bInterfaceProtocol   = USB_CDC_PROTOCOL_AT,
		.iInterface           = 0,

		.endpoint             = comm_endp,

		.extra                = &cdcacm_functional_descriptors,
		.extralen             = sizeof(cdcacm_functional_descriptors),
	}
};

static const struct usb_interface_descriptor data_iface[] = {{
		.bLength              = USB_DT_INTERFACE_SIZE,
		.bDescriptorType      = USB_DT_INTERFACE,
		.bInterfaceNumber     = 1,
		.bAlternateSetting    = 0,
		.bNumEndpoints        = 2,
		.bInterfaceClass      = USB_CLASS_DATA,
		.bInterfaceSubClass   = 0,
		.bInterfaceProtocol   = 0,
		.iInterface           = 0,

		.endpoint             = data_endp,
	}
};


static const struct usb_interface_descriptor comm_iface2[] = {{
		.bLength              = USB_DT_INTERFACE_SIZE,
		.bDescriptorType      = USB_DT_INTERFACE,
		.bInterfaceNumber     = 2,
		.bAlternateSetting    = 0,
		.bNumEndpoints        = 1,
		.bInterfaceClass      = USB_CLASS_CDC,
		.bInterfaceSubClass   = USB_CDC_SUBCLASS_ACM,
		.bInterfaceProtocol   = USB_CDC_PROTOCOL_AT,
		.iInterface           = 0,

		.endpoint             = comm_endp2,

		.extra                = &cdcacm_functional_descriptors2,
		.extralen             = sizeof(cdcacm_functional_descriptors),
	}
};

static const struct usb_interface_descriptor data_iface2[] = {{
		.bLength              = USB_DT_INTERFACE_SIZE,
		.bDescriptorType      = USB_DT_INTERFACE,
		.bInterfaceNumber     = 3,
		.bAlternateSetting    = 0,
		.bNumEndpoints        = 2,
		.bInterfaceClass      = USB_CLASS_DATA,
		.bInterfaceSubClass   = 0,
		.bInterfaceProtocol   = 0,
		.iInterface           = 0,

		.endpoint             = data_endp2,
	}
};

static const struct usb_interface_descriptor comm_iface3[] = {{
		.bLength              = USB_DT_INTERFACE_SIZE,
		.bDescriptorType      = USB_DT_INTERFACE,
		.bInterfaceNumber     = 4,
		.bAlternateSetting    = 0,
		.bNumEndpoints        = 1,
		.bInterfaceClass      = USB_CLASS_CDC,
		.bInterfaceSubClass   = USB_CDC_SUBCLASS_ACM,
		.bInterfaceProtocol   = USB_CDC_PROTOCOL_AT,
		.iInterface           = 0,

		.endpoint             = comm_endp3,

		.extra                = &cdcacm_functional_descriptors3,
		.extralen             = sizeof(cdcacm_functional_descriptors),
	}
};

static const struct usb_interface_descriptor data_iface3[] = {{
		.bLength              = USB_DT_INTERFACE_SIZE,
		.bDescriptorType      = USB_DT_INTERFACE,
		.bInterfaceNumber     = 5,
		.bAlternateSetting    = 0,
		.bNumEndpoints        = 2,
		.bInterfaceClass      = USB_CLASS_DATA,
		.bInterfaceSubClass   = 0,
		.bInterfaceProtocol   = 0,
		.iInterface           = 0,

		.endpoint             = data_endp3,
	}
};


static const struct usb_interface ifaces[] = {{
		.num_altsetting       = 1,
		.altsetting           = comm_iface,
	}, {
		.num_altsetting       = 1,
		.altsetting           = data_iface,
	}, {
		.num_altsetting       = 1,
		.altsetting           = comm_iface2,
	}, {
		.num_altsetting       = 1,
		.altsetting           = data_iface2,
	}, {
		.num_altsetting       = 1,
		.altsetting           = comm_iface3,
	}, {
		.num_altsetting       = 1,
		.altsetting           = data_iface3,
	}
};

static const struct usb_config_descriptor config = {
	.bLength              = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType      = USB_DT_CONFIGURATION,
	.wTotalLength         = 0,
	.bNumInterfaces       = 6,
	.bConfigurationValue  = 1,
	.iConfiguration       = 0,
	.bmAttributes         = 0x80,
	.bMaxPower            = 0x32,

	.interface            = ifaces,
};

/* Buffer to be used for control requests. */

static uint8_t usbd_control_buffer[128] ALIGNED;

static int cdcacm_control_request(usbd_device *usbd_dev UNUSED, struct usb_setup_data *req, uint8_t **buf UNUSED,
                                  uint16_t *len, void (**complete)(usbd_device *usbd_dev, struct usb_setup_data *req) UNUSED)
{
	switch (req->bRequest) {
	case USB_CDC_REQ_SET_CONTROL_LINE_STATE: {
		return 1;
	}
	case USB_CDC_REQ_SET_LINE_CODING:
		if (*len < sizeof(struct usb_cdc_line_coding))
			return 0;
		return 1;
	}
	return 0;
}

volatile bool usb_ready = false;

static void cdcacm_reset(void)
{
	usb_ready = false;
}

static void ep1_rx_callback(usbd_device *usb_dev UNUSED, uint8_t ep);

static void cdcacm_set_config(usbd_device *usbd_dev, uint16_t wValue)
{
	usbd_ep_setup(usbd_dev, EP_IN , USB_ENDPOINT_ATTR_BULK, 64, ep1_rx_callback);
	usbd_ep_setup(usbd_dev, EP_OUT, USB_ENDPOINT_ATTR_BULK, 64, NULL);
	usbd_ep_setup(usbd_dev, EP_INT, USB_ENDPOINT_ATTR_INTERRUPT, 16, NULL);

	usbd_ep_setup(usbd_dev, EP2_IN , USB_ENDPOINT_ATTR_BULK, 64, NULL);
	usbd_ep_setup(usbd_dev, EP2_OUT, USB_ENDPOINT_ATTR_BULK, 64, NULL);
	usbd_ep_setup(usbd_dev, EP2_INT, USB_ENDPOINT_ATTR_INTERRUPT, 16, NULL);

	usbd_ep_setup(usbd_dev, EP3_IN , USB_ENDPOINT_ATTR_BULK, 64, NULL);
	usbd_ep_setup(usbd_dev, EP3_OUT, USB_ENDPOINT_ATTR_BULK, 64, NULL);
	usbd_ep_setup(usbd_dev, EP3_INT, USB_ENDPOINT_ATTR_INTERRUPT, 16, NULL);

	usbd_register_control_callback(
	        usbd_dev,
	        USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
	        USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
	        cdcacm_control_request);

	if (wValue > 0) {
		usb_ready = true;
	}
}

static usbd_device *usbd_dev; /* Just a pointer, need not to be volatile. */

static char serial[UID_LEN];

/* Vendor, device, version. */
static const char *usb_strings[] = {
	"STM32-LPC",
	"VCP for flashrom serprog",
	serial,
};

void usbcdc_init(void)
{
	desig_get_unique_id_as_string(serial, UID_LEN);

	usbd_dev = usbd_init(&st_usbfs_v2_usb_driver, &dev, &config, usb_strings, 3, usbd_control_buffer,
	                     sizeof(usbd_control_buffer));

	usbd_register_set_config_callback(usbd_dev, cdcacm_set_config);
	usbd_register_reset_callback(usbd_dev, cdcacm_reset);

	/* NOTE: Must be called after USB setup since this enables calling usbd_poll(). */
	nvic_set_priority(NVIC_USB_IRQ, 255);
	nvic_enable_irq(NVIC_USB_IRQ);
}


/* Application-level functions */
uint16_t usbcdc_write(void *buf, size_t len)
{
	uint16_t ret;

	/* Blocking write */
	while (0 == (ret = usbd_ep_write_packet(usbd_dev, EP_OUT, buf, len)));
	return ret;
}

uint16_t usbcdc_write_ch3(void *buf, size_t len)
{
	return usbd_ep_write_packet(usbd_dev, EP3_OUT, buf, len);
}


static uint8_t usbcdc_txbuf[USBCDC_PKT_SIZE_DAT] ALIGNED;
static uint8_t usbcdc_txbuf_cnt = 0;

void usbcdc_putc(uint8_t c)
{
       usbcdc_txbuf[usbcdc_txbuf_cnt++] = c;
       if (usbcdc_txbuf_cnt >= USBCDC_PKT_SIZE_DAT) {
               usbcdc_write(usbcdc_txbuf, usbcdc_txbuf_cnt);
               usbcdc_txbuf_cnt = 0;
	}
}


#define RX_SLOTS 4
static uint8_t usbcdc_rxbuf[RX_SLOTS][USBCDC_PKT_SIZE_DAT] ALIGNED;
static uint8_t usbcdc_rxbuf_cnt[RX_SLOTS];
static volatile uint8_t usbcdc_rxb_wslot = 0;
static uint8_t usbcdc_rxb_rslot = 0;

static void ep1_rx(void)
{
	uint8_t nslot = (usbcdc_rxb_wslot+1) & (RX_SLOTS-1);
	if (nslot != usbcdc_rxb_rslot) {
		uint16_t l;
		if ((l=usbd_ep_read_packet(usbd_dev, EP_IN, usbcdc_rxbuf[nslot], USBCDC_PKT_SIZE_DAT))) {
			usbcdc_rxbuf_cnt[nslot] = l;
			usbcdc_rxb_wslot = nslot;
			DBG("-READ-");
		}
	}
}


static void ep1_rx_callback(usbd_device *usb_dev UNUSED, uint8_t ep)
{
	USB_CLR_EP_RX_CTR(ep);
	DBG("RXC-");
	ep1_rx();
}


static uint16_t usbcdc_fetch_packet(void)
{
	if (usbcdc_txbuf_cnt) {
               usbcdc_write(usbcdc_txbuf, usbcdc_txbuf_cnt);
               usbcdc_txbuf_cnt = 0;
	}
	DBG("-FTCH");
	/* Blocking read. Assume RX user buffer is empty. TODO: consider setting a timeout */
	while (usbcdc_rxb_rslot == usbcdc_rxb_wslot) {
		uint16_t dummy;
		//DBG("X");
		usbd_ep_read_packet(usbd_dev, EP2_IN, &dummy, 2);
		//DBG("Y");
		 usbd_ep_read_packet(usbd_dev, EP3_IN, &dummy, 2);
		//DBG("Z");
		yield();
	}
	usbcdc_rxb_rslot = (usbcdc_rxb_rslot+1) & (RX_SLOTS-1);
	/* If there was a "skipped" callback due to slots full, give it a kick from here. */
	cm_disable_interrupts();
	ep1_rx();
	cm_enable_interrupts();
	return 1;
}

uint8_t usbcdc_getc(void)
{
	static uint8_t usbcdc_rxbuf_off = 0;
	uint8_t c;
	if (usbcdc_rxbuf_off>=usbcdc_rxbuf_cnt[usbcdc_rxb_rslot]) {
		usbcdc_fetch_packet();
		usbcdc_rxbuf_off = 0;
		DBG("-ED.");
	}
	c = usbcdc_rxbuf[usbcdc_rxb_rslot][usbcdc_rxbuf_off++];
	return c;
}

/* Interrupts */

void usb_isr(void)
{
	usbd_poll(usbd_dev);
}
