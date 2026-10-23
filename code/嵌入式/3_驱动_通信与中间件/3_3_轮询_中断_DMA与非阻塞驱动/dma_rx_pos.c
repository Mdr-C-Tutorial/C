#include "dma_rx_pos.h"

size_t dma_rx_calc_pos(size_t buf_size, size_t cndtr) {
    if (buf_size == 0u || cndtr == 0u || cndtr >= buf_size) {
        return 0u;
    }
    return buf_size - cndtr;
}

struct dma_rx_slice dma_rx_diff(size_t buf_size, size_t old_pos, size_t cndtr) {
    struct dma_rx_slice s = {0};
    if (buf_size == 0u || old_pos >= buf_size) {
        return s;
    }

    size_t pos = dma_rx_calc_pos(buf_size, cndtr);
    s.new_pos = pos;

    if (pos == old_pos) {
        return s;
    }

    if (pos > old_pos) {
        s.span_count = 1u;
        s.spans[0].offset = old_pos;
        s.spans[0].len = pos - old_pos;
        s.total_len = s.spans[0].len;
    } else {
        s.spans[0].offset = old_pos;
        s.spans[0].len = buf_size - old_pos;
        s.total_len = s.spans[0].len;
        s.span_count = 1u;

        if (pos > 0u) {
            s.spans[1].offset = 0u;
            s.spans[1].len = pos;
            s.total_len += pos;
            s.span_count = 2u;
        }
    }
    return s;
}

size_t dma_rx_drain(const uint8_t *dma_buf,
                    size_t buf_size,
                    size_t *old_pos,
                    size_t cndtr,
                    dma_rx_on_data_fn on_data,
                    void *user_data) {
    if (dma_buf == NULL || old_pos == NULL || buf_size == 0u) {
        return 0u;
    }

    struct dma_rx_slice s = dma_rx_diff(buf_size, *old_pos, cndtr);
    if (on_data != NULL) {
        for (size_t i = 0u; i < s.span_count; ++i) {
            on_data(&dma_buf[s.spans[i].offset], s.spans[i].len, user_data);
        }
    }
    *old_pos = s.new_pos;
    return s.total_len;
}
