#include "led_ctrl.h"
#include "driver/gpio.h"

void led_ctrl_init(void) {
    // Reseta o pino para garantir que não há configurações antigas
    gpio_reset_pin(LED_GPIO);
    // Configura o pino como saída
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    // Inicia o LED desligado
    gpio_set_level(LED_GPIO, 0);
}

void led_ctrl_set_state(bool state) {
    gpio_set_level(LED_GPIO, state ? 1 : 0);
}