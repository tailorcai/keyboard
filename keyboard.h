#include "pico/stdlib.h"

#ifndef KEYBOARD_H_
#define KEYBOARD_H_

void keyboard_init();
bool keyboard_update();

bool key_is_rising(int i); 
bool key_is_falling(int i);
bool key_code(int i);

void press(int key_code);
void release(int key_code);

uint8_t * get_key_report();

#endif /* KEYBOARD_H_ */