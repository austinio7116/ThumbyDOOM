#ifndef _HW_PWM_H
#define _HW_PWM_H
#include <stdint.h>
#include <stdbool.h>
typedef struct { int dummy; } pwm_config;
static inline pwm_config pwm_get_default_config(void) { pwm_config c = {0}; return c; }
static inline void pwm_config_set_clkdiv_int(pwm_config *c, int d) { (void)c; (void)d; }
static inline void pwm_config_set_wrap(pwm_config *c, uint16_t w) { (void)c; (void)w; }
static inline uint pwm_gpio_to_slice_num(uint pin) { (void)pin; return 0; }
static inline void pwm_init(uint slice, pwm_config *c, bool start) { (void)slice; (void)c; (void)start; }
static inline void pwm_set_gpio_level(uint pin, uint32_t level) { (void)pin; (void)level; }
static inline void pwm_clear_irq(uint slice) { (void)slice; }
static inline void pwm_set_irq_enabled(uint slice, bool en) { (void)slice; (void)en; }
typedef unsigned int uint;
#endif
