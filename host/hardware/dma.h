/* host hardware/dma.h shim — no DMA on host, all stubs */
#ifndef _HW_DMA_H
#define _HW_DMA_H
#include <stdint.h>
typedef struct { int dummy; } dma_channel_config;
static inline int  dma_claim_unused_channel(int required) { (void)required; return 0; }
static inline dma_channel_config dma_channel_get_default_config(int ch) { (void)ch; dma_channel_config c = {0}; return c; }
static inline void channel_config_set_transfer_data_size(dma_channel_config *c, int sz) { (void)c; (void)sz; }
static inline void channel_config_set_dreq(dma_channel_config *c, int dreq) { (void)c; (void)dreq; }
static inline void dma_channel_configure(int ch, dma_channel_config *c, volatile void *write, const volatile void *read, uint32_t count, int trigger) { (void)ch; (void)c; (void)write; (void)read; (void)count; (void)trigger; }
static inline int  dma_channel_is_busy(int ch) { (void)ch; return 0; }
#define DMA_SIZE_16 1
#define DREQ_SPI0_TX 0
#endif
