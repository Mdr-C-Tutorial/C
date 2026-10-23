#include "dma_rx_pos.h"
#include <stdio.h>
#include <string.h>

#define RX_BUF_SIZE 64u

struct collect_ctx {
    char out[128];
    size_t len;
    size_t call_count;
};

static void on_chunk(const uint8_t *data, size_t len, void *user_data) {
    struct collect_ctx *ctx = (struct collect_ctx *)user_data;
    if (ctx->len + len < sizeof(ctx->out)) {
        memcpy(&ctx->out[ctx->len], data, len);
        ctx->len += len;
        ctx->out[ctx->len] = '\0';
    }
    ctx->call_count++;
}

static void sim_dma_write(uint8_t *dma_buf,
                          size_t buf_size,
                          size_t *hw_cndtr,
                          const char *str) {
    size_t n = strlen(str);
    for (size_t i = 0u; i < n; ++i) {
        size_t write_idx = buf_size - *hw_cndtr;
        dma_buf[write_idx] = (uint8_t)str[i];
        if (*hw_cndtr == 1u) {
            *hw_cndtr = buf_size;
        } else {
            (*hw_cndtr)--;
        }
    }
}

static void run_step(const char *label,
                     uint8_t *dma_buf,
                     size_t *old_pos,
                     size_t *hw_cndtr,
                     const char *incoming) {
    if (incoming != NULL) {
        sim_dma_write(dma_buf, RX_BUF_SIZE, hw_cndtr, incoming);
    }

    size_t prev_pos = *old_pos;
    struct dma_rx_slice slice = dma_rx_diff(RX_BUF_SIZE, prev_pos, *hw_cndtr);

    struct collect_ctx ctx = {0};
    size_t drained = dma_rx_drain(dma_buf, RX_BUF_SIZE, old_pos, *hw_cndtr,
                                  on_chunk, &ctx);

    printf("[%s]\n", label);
    printf("  CNDTR = %2u, old_pos = %2u -> new_pos = %2u, spans = %u, bytes = %2u\n",
           (unsigned)*hw_cndtr, (unsigned)prev_pos,
           (unsigned)slice.new_pos, (unsigned)slice.span_count,
           (unsigned)drained);
    for (size_t i = 0u; i < slice.span_count; ++i) {
        printf("  span[%u]: offset = %2u, len = %2u\n",
               (unsigned)i, (unsigned)slice.spans[i].offset,
               (unsigned)slice.spans[i].len);
    }
    if (drained > 0u) {
        printf("  payload: \"%s\"\n", ctx.out);
    }
}

int main(void) {
    uint8_t rx_dma_buf[RX_BUF_SIZE] = {0};
    size_t old_pos = 0u;
    size_t hw_cndtr = RX_BUF_SIZE;

    run_step("1. IDLE after 12 bytes",
             rx_dma_buf, &old_pos, &hw_cndtr, "AT+GMR_OK_12");

    run_step("2. HT at 32 bytes",
             rx_dma_buf, &old_pos, &hw_cndtr, "0123456789ABCDEFGHIJ");

    run_step("3. IDLE at 56 bytes",
             rx_dma_buf, &old_pos, &hw_cndtr, "SENSOR:TEMP=25.4,HUMI=60");

    run_step("4. Wrap-around across 64-byte boundary",
             rx_dma_buf, &old_pos, &hw_cndtr, "WRAP_TAIL_AND_HEAD!!");

    run_step("5. Duplicate check without new bytes",
             rx_dma_buf, &old_pos, &hw_cndtr, NULL);

    return 0;
}
