/* host hardware/gpio.h shim — keys backed by SDL state */
#ifndef _HW_GPIO_H
#define _HW_GPIO_H
#include <stdint.h>
#include <stdbool.h>
typedef enum { GPIO_FUNC_SIO, GPIO_FUNC_SPI, GPIO_FUNC_PWM, GPIO_FUNC_I2C } gpio_function_t;
#define GPIO_OUT 1
#define GPIO_IN  0
extern int host_gpio_get(int pin);  /* implemented in host_main.c */
static inline void gpio_init(uint32_t pin) { (void)pin; }
static inline void gpio_set_dir(uint32_t pin, int dir) { (void)pin; (void)dir; }
static inline void gpio_pull_up(uint32_t pin) { (void)pin; }
static inline void gpio_set_function(uint32_t pin, gpio_function_t f) { (void)pin; (void)f; }
static inline void gpio_put(uint32_t pin, bool v) { (void)pin; (void)v; }
static inline int  gpio_get(uint32_t pin) { return host_gpio_get(pin); }
#endif
