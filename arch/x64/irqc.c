#include "aex/string.h"
#include "aex/proc/task.h"
#include "aex/sys/irq.h"
#include "aex/sys/time.h"

//#include "aex/dev/cpu.h"
#include "cpu_int.h"

extern void* tss_ptr;
struct tss* cpu_tss = (struct tss*) &tss_ptr;

extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

uint8_t ist1[2048];
uint8_t ist2[2048];
uint8_t ist3[2048];
uint8_t ist4[2048];

void irq_set_mask(uint16_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8)
        port = 0x21;
    else {
        port = 0xA1;
        irq -= 8;
    }
    value = inportb(port) | (1 << irq);
    outportb(port, value);
}

void irq_clear_mask(uint16_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8)
        port = 0x21;
    else {
        port = 0xA1;
        irq -= 8;
    }
    value = inportb(port) & ~(1 << irq);
    outportb(port, value);
}

void irq_init() {
	outportb(0x20, 0x11);
	outportb(0xA0, 0x11);
	outportb(0x21, 0x20);
	outportb(0xA1, 0x28);
	outportb(0x21, 0x04);
	outportb(0xA1, 0x02);
	outportb(0x21, 0x01);
	outportb(0xA1, 0x01);
	outportb(0x21, 0x00);
	outportb(0xA1, 0x00);

    idt_set_entry(32, irq0, 0x8E, 2);
    idt_set_entry(33, irq1, 0x8E, 3);
    idt_set_entry(34, irq2, 0x8E, 4);
    idt_set_entry(35, irq3, 0x8E, 3);
    idt_set_entry(36, irq4, 0x8E, 4);
    idt_set_entry(37, irq5, 0x8E, 3);
    idt_set_entry(38, irq6, 0x8E, 4);
    idt_set_entry(39, irq7, 0x8E, 3);
    idt_set_entry(40, irq8, 0x8E, 4);
    idt_set_entry(41, irq9, 0x8E, 3);
    idt_set_entry(42, irq10, 0x8E, 4);
    idt_set_entry(43, irq11, 0x8E, 3);
    idt_set_entry(44, irq12, 0x8E, 4);
    idt_set_entry(45, irq13, 0x8E, 3);
    idt_set_entry(46, irq14, 0x8E, 4);
    idt_set_entry(47, irq15, 0x8E, 3);

    for (int i = 0; i < 16; i++)
        irq_clear_mask(i);

    irq_set_mask(7);

    memset((void*) cpu_tss, 0, sizeof(struct tss));

    cpu_tss->ist1 = (uint64_t) &ist1 + sizeof(ist1);
    cpu_tss->ist2 = (uint64_t) &ist2 + sizeof(ist2);
    cpu_tss->ist3 = (uint64_t) &ist3 + sizeof(ist3);
    cpu_tss->ist4 = (uint64_t) &ist4 + sizeof(ist4);
}

void irq_handler(struct regs* r) {
    int irq = r->int_no - 32;
    irq_enqueue((uint8_t) irq);

    if (r->int_no >= 40)
        outportb(0xA0, 0x20);

    outportb(0x20, 0x20);
}

void timer_set_hz(int hz) {
    int divisor = 1193181.6666 / hz; // Calculate our divisor
    time_set_tick_interval(((double) 1 / (1193181.666666666666 / divisor)) * 1000);

    outportb(0x43, 0x36);            // Set our command byte 0x36
    outportb(0x40, divisor & 0xFF);  // Set low byte of divisor
    outportb(0x40, divisor >> 8);    // Set high byte of divisor
}

void timer_init(int hz) {
    timer_set_hz(hz);
}