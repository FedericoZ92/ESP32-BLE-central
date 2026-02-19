#ifndef LED_H
#define LED_H

#include "led_strip.h"
#include "esp_log.h"

void led_init(void);
void led_off(void);
void led_red(void);
void led_green(void);
void led_blue(void);

#endif // LED_H
