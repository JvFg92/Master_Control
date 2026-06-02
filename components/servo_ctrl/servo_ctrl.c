#include "servo_ctrl.h"
#include "driver/ledc.h"

const int SERVO_PINS[4] = {13, 12, 14, 27};
const ledc_channel_t SERVO_CHANNELS[4] = {
    LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3
};

void servo_ctrl_init(void) {
    // 1. Configura o Timer do PWM (50Hz para servos)
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_13_BIT, // Resolução de 8192 passos
        .freq_hz          = 50,                // 50Hz = período de 20ms
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // 2. Configura os 4 canais do PWM acoplados ao mesmo Timer
    for (int i = 0; i < 4; i++) {
        ledc_channel_config_t ledc_channel = {
            .channel    = SERVO_CHANNELS[i],
            .duty       = 0,
            .gpio_num   = SERVO_PINS[i],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .hpoint     = 0,
            .timer_sel  = LEDC_TIMER_0
        };
        ledc_channel_config(&ledc_channel);
    }
}

void servo_set_angle(servo_id_t servo, int angle) {
    // Trava os limites entre 0 e 180 graus
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    // Cálculo do Duty Cycle:
    // MG90S espera pulso de ~0.5ms (0 graus) a ~2.5ms (180 graus).
    // Em 13 bits (8192 passos) para 20ms:
    // 0.5ms = (0.5/20) * 8192 = ~205 passos
    // 2.5ms = (2.5/20) * 8192 = ~1024 passos
    uint32_t duty = 205 + ((angle * (1024 - 205)) / 180);

    // Aplica o sinal PWM ao canal selecionado
    ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNELS[servo], duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNELS[servo]);
}