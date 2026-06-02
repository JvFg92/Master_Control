#ifndef SERVO_CTRL_H
#define SERVO_CTRL_H

//Servo ID:
typedef enum {
    SERVO_1 = 0, // GPIO 13
    SERVO_2 = 1, // GPIO 12
    SERVO_3 = 2, // GPIO 14
    SERVO_4 = 3  // GPIO 27
} servo_id_t;

// Public Functions:
void servo_ctrl_init(void);
void servo_set_angle(servo_id_t servo, int angle_0_to_180);

#endif