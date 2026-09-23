#ifndef TEST_BUTTON_CHEAT_ARDUINO_H
#define TEST_BUTTON_CHEAT_ARDUINO_H
#include <stdint.h>
#define INPUT_PULLUP 2
#define FALLING 3
#define LOW 0
#define HIGH 1
unsigned long millis();
int digitalRead(int pin);
void pinMode(int pin, int mode);
int digitalPinToInterrupt(int pin);
void attachInterrupt(int pin, void (*callback)(), int mode);
#endif
