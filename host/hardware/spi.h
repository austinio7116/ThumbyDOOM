#ifndef _HW_SPI_H
#define _HW_SPI_H
#include <stddef.h>
#include <stdint.h>
typedef int spi_inst_t;
#define spi0 ((spi_inst_t*)0)
#define spi1 ((spi_inst_t*)1)
#define SPI_CPOL_1 1
#define SPI_CPHA_1 1
#define SPI_MSB_FIRST 0
typedef struct { uint32_t dr; uint32_t sr; } spi_hw_t;
static inline void spi_init(spi_inst_t *s, uint32_t hz) { (void)s; (void)hz; }
static inline void spi_set_format(spi_inst_t *s, int bits, int cpol, int cpha, int order) { (void)s; (void)bits; (void)cpol; (void)cpha; (void)order; }
static inline void spi_write_blocking(spi_inst_t *s, const uint8_t *src, size_t n) { (void)s; (void)src; (void)n; }
static inline spi_hw_t *spi_get_hw(spi_inst_t *s) { (void)s; static spi_hw_t hw; return &hw; }
#define SPI_SSPSR_BSY_BITS 0
#endif
