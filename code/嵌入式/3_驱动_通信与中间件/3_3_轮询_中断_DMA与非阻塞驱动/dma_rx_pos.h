#ifndef DMA_RX_POS_H
#define DMA_RX_POS_H

#include <stddef.h>
#include <stdint.h>

struct dma_rx_span {
    size_t offset;
    size_t len;
};

struct dma_rx_slice {
    size_t new_pos;
    size_t total_len;
    size_t span_count;
    struct dma_rx_span spans[2];
};

typedef void (*dma_rx_on_data_fn)(const uint8_t *data, size_t len, void *user_data);

size_t dma_rx_calc_pos(size_t buf_size, size_t cndtr);

struct dma_rx_slice dma_rx_diff(size_t buf_size, size_t old_pos, size_t cndtr);

size_t dma_rx_drain(const uint8_t *dma_buf,
                    size_t buf_size,
                    size_t *old_pos,
                    size_t cndtr,
                    dma_rx_on_data_fn on_data,
                    void *user_data);

#endif
