#include <stdint.h>

void task_exit(void);
void task_switch(void);
void task_start(void *arg, void (*fp)(void*), void*sp);
int task_count(void);