#pragma once

#include <stddef.h>
#include <stdint.h>

#define INPUT_SHIFT_FLAG   0x01
#define INPUT_CONTROL_FLAG 0x02
#define INPUT_ALT_FLAG     0x04

// Signals a key press
void input_kb_press(uint8_t key);
// Signals a key release
void input_kb_release(uint8_t key);

int input_fetch_keymap(char* name, char keymap[1024]);

size_t   input_kb_get(uint8_t* c, uint8_t* modifiers, size_t last);
uint32_t input_kb_available(size_t last);
void     input_kb_wait(size_t last);
size_t   input_kb_sync();