uint16_t usbopt_ep_write_packet(usbd_device *dev, uint8_t addr, const void *buf, uint16_t len);
uint16_t usbopt_ep_read_packet(usbd_device *dev, uint8_t addr, void *buf, uint16_t len, void(*cb)(int));
