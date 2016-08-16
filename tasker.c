#include <stdint.h>
#include <stdlib.h>

#define S(s) _S(s)
#define _S(s) #s

#define MAX_TASKS 4

#define USED __attribute__((used))
#define NAKED __attribute__((naked))

struct taskm {
	uint16_t current;
	uint16_t max;
	uint32_t sp[MAX_TASKS];
};

static struct taskm tasker_data USED = {};
void task_exit(void) USED;
void task_switch(void) NAKED;
void task_start(void *arg, void (*fp)(void*), void*sp) NAKED;
int task_count(void);

void task_switch(void) {
	asm volatile( 
	".syntax unified\n\t"
	"push {r4-r7,lr}\n\t"
	"mov r0, r8\n\t"
	"mov r1, r9\n\t"
	"mov r2, r10\n\t"
	"mov r3, r11\n\t"
	"push {r0-r3}\n\t"
	"ldr r0, =tasker_data\n\t"
	"adds r1, r0, #4\n\t"
	"ldrh r2, [r0]\n\t"
	"ldrh r3, [r0,#2]\n\t"
	"lsls r4, r2, #2\n\t"
	"mov r7, sp\n\t"
	"str r7, [r1, r4]\n\t"
	"adds r2, #1\n\t"
	"cmp r2, r3\n\t"
	"bls 0f\n\t"
	"ldr r2, =#0\n\t"
	"0:strh r2, [r0]\n\t"
	"lsls r4, r2, #2\n\t"
	"ldr r7, [r1, r4]\n\t"
	"mov sp, r7\n\t"
	"100:pop {r0-r3}\n\t"
	"mov r8, r0\n\t"
	"mov r9, r1\n\t"
	"mov r10, r2\n\t"
	"mov r11, r3\n\t"
	"pop {r4-r7,pc}\n\t"
	".pool\n\t"
	);
}

void task_start(void *arg, void (*fp)(void*), void*sp) {
	asm volatile( 
	".syntax unified\n\t"
	"push {r4-r7,lr}\n\t"
	"mov r4, r8\n\t"
	"mov r5, r9\n\t"
	"mov r6, r10\n\t"
	"mov r7, r11\n\t"
	"push {r4-r7}\n\t"
	"ldr r5, =tasker_data\n\t"
	"ldrh r3, [r5]\n\t"
	"adds r6, r5, #4\n\t"
	"lsls r4, r3, #2\n\t"
	"mov r7, sp\n\t"
	"str r7, [r6, r4]\n\t"
	"ldrh r3, [r5,#2]\n\t"
	"adds r3, #1\n\t"
	"cmp r3, #" S(MAX_TASKS) "\n\t"
	"beq 100b\n\t" // shorthand to just pop things out and exit instead of overflowing the task struct
	"strh r3, [r5]\n\t"
	"strh r3, [r5, #2]\n\t"
	"mov sp, %[nsp]\n\t"
	"ldr r2, =task_exit\n\t"
	"mov lr, r2\n\t"
	"bx %[fp]\n\t"
	".pool\n\t"
	:: [arg] "r" (arg), [fp] "r" (fp), [nsp] "r" (sp) // these are defined to show gcc that they're used;
	// the function is naked so we know they're r0,r1,r2.
	);
}

void task_exit(void) {
	for(;;) {
		/* Delete mee.... */
		if (tasker_data.max > tasker_data.current) {
			for (unsigned int i=tasker_data.current; i<tasker_data.max;i++) 
				tasker_data.sp[i] = tasker_data.sp[i+1];
			tasker_data.current = tasker_data.max;
		}
		if ((tasker_data.max)&&(tasker_data.current == tasker_data.max)) tasker_data.max--;
		task_switch(); /* This shouldnt return now, but the loop is a relatively safe protection against WTF ... */
	}
}

int task_count(void) {
	return tasker_data.max+1;
}