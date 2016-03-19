#include <stdint.h>
void extuart_init(void);
void extuart_putc(uint8_t c);
void extuart_sendstr(const char* p_);

#if 1
#define DBG(str) extuart_sendstr(str)
#else
#define DBG(str)
#endif

void dbg_present_val(const char* reg, uint32_t v);
