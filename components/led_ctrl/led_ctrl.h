#ifndef LED_CTRL_H
#define LED_CTRL_H

#include <stdbool.h>

// Definindo o pino do LED
#define LED_GPIO 33

// Funções públicas do módulo
void led_ctrl_init(void);
void led_ctrl_set_state(bool state);

#endif // LED_CTRL_H