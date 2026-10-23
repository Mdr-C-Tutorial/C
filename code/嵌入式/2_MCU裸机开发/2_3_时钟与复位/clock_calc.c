/*
 * clock_calc.c：根据 STM32F103 的 RCC_CFGR 与 FLASH_ACR 配置值，
 * 推算 SYSCLK、HCLK、PCLK1、TIM2~4、PCLK2、TIM1、ADC、USB 各路时钟频率并核对上限。
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define HSI_HZ 8000000u
#define HSE_HZ 8000000u

typedef struct {
    const char *name;
    uint32_t cfgr;
    uint32_t acr;
} clock_preset_t;

typedef struct {
    uint32_t sysclk_hz;
    uint32_t hclk_hz;
    uint32_t pclk1_hz;
    uint32_t tim_apb1_hz;
    uint32_t pclk2_hz;
    uint32_t tim_apb2_hz;
    uint32_t adcclk_hz;
    uint32_t usbclk_x2_hz; /* 为避免半整数，以 2 × USBCLK 保存 */
    bool usb_from_hse;
    uint32_t flash_ws;
} clock_tree_t;

static uint32_t decode_ahb_div(uint32_t hpre) {
    static const uint16_t table[16] = {
        1, 1, 1, 1, 1, 1, 1, 1,
        2, 4, 8, 16, 64, 128, 256, 512
    };
    return table[hpre & 0xFu];
}

static uint32_t decode_apb_div(uint32_t ppre) {
    static const uint8_t table[8] = {1, 1, 1, 1, 2, 4, 8, 16};
    return table[ppre & 0x7u];
}

static uint32_t decode_adc_div(uint32_t adcpre) {
    static const uint8_t table[4] = {2, 4, 6, 8};
    return table[adcpre & 0x3u];
}

static void evaluate_clock_tree(uint32_t cfgr, uint32_t acr, clock_tree_t *out) {
    uint32_t sws = (cfgr >> 2) & 0x3u;
    uint32_t pllsrc = (cfgr >> 16) & 0x1u;
    uint32_t pllxtpre = (cfgr >> 17) & 0x1u;
    uint32_t pllmul_bits = (cfgr >> 18) & 0xFu;
    uint32_t pllmul = (pllmul_bits >= 0xEu) ? 16u : (pllmul_bits + 2u);

    uint32_t pll_in_hz = 0;
    if (pllsrc == 0) {
        pll_in_hz = HSI_HZ / 2u;
    } else {
        pll_in_hz = (pllxtpre != 0) ? (HSE_HZ / 2u) : HSE_HZ;
    }
    uint32_t pll_vco_hz = pll_in_hz * pllmul;

    if (sws == 0u) {
        out->sysclk_hz = HSI_HZ;
    } else if (sws == 1u) {
        out->sysclk_hz = HSE_HZ;
    } else {
        out->sysclk_hz = pll_vco_hz;
    }

    uint32_t ahb_div = decode_ahb_div((cfgr >> 4) & 0xFu);
    uint32_t apb1_div = decode_apb_div((cfgr >> 8) & 0x7u);
    uint32_t apb2_div = decode_apb_div((cfgr >> 11) & 0x7u);
    uint32_t adc_div = decode_adc_div((cfgr >> 14) & 0x3u);
    uint32_t usbpre = (cfgr >> 22) & 0x1u;

    out->hclk_hz = out->sysclk_hz / ahb_div;
    out->pclk1_hz = out->hclk_hz / apb1_div;
    out->tim_apb1_hz = (apb1_div == 1u) ? out->pclk1_hz : (out->pclk1_hz * 2u);
    out->pclk2_hz = out->hclk_hz / apb2_div;
    out->tim_apb2_hz = (apb2_div == 1u) ? out->pclk2_hz : (out->pclk2_hz * 2u);
    out->adcclk_hz = out->pclk2_hz / adc_div;

    if (sws == 2u) {
        /* usbpre == 1 表示 1 分频（2×USBCLK = 2×PLL）；usbpre == 0 表示 1.5 分频（2×USBCLK = 4/3×PLL） */
        out->usbclk_x2_hz = (usbpre != 0) ? (pll_vco_hz * 2u) : ((pll_vco_hz * 4u) / 3u);
        out->usb_from_hse = (pllsrc != 0);
    } else {
        out->usbclk_x2_hz = 0;
        out->usb_from_hse = false;
    }

    out->flash_ws = acr & 0x7u;
}

static bool usb_is_valid(const clock_tree_t *t) {
    return t->usb_from_hse && (t->usbclk_x2_hz == 96000000u);
}

int main(void) {
    const clock_preset_t presets[] = {
        {
            .name = "Reset default (HSI 8 MHz)",
            .cfgr = 0x00000000u,
            .acr  = 0x00000030u
        },
        {
            .name = "HSE 8 MHz -> PLL x9 (72 MHz)",
            .cfgr = (0x2u << 2) | (0x4u << 8) | (0x2u << 14) |
                    (0x1u << 16) | (0x7u << 18),
            .acr  = 0x00000032u
        },
        {
            .name = "HSE 8 MHz -> PLL x6 (48 MHz)",
            .cfgr = (0x2u << 2) | (0x4u << 8) | (0x1u << 14) |
                    (0x1u << 16) | (0x4u << 18) | (0x1u << 22),
            .acr  = 0x00000031u
        },
        {
            .name = "HSI/2 4 MHz -> PLL x16 (64 MHz)",
            .cfgr = (0x2u << 2) | (0x4u << 8) | (0x2u << 14) |
                    (0x0u << 16) | (0xEu << 18),
            .acr  = 0x00000032u
        }
    };

    printf("%-32s %6s %6s %6s %7s %6s %6s %8s %4s\n",
           "Preset", "SYSCLK", "HCLK", "PCLK1", "TIM2-4",
           "PCLK2", "ADCCLK", "USBCLK", "WS");
    printf("---------------------------------------------------------------------------------------\n");

    size_t count = sizeof presets / sizeof presets[0];
    for (size_t i = 0; i < count; ++i) {
        clock_tree_t t;
        evaluate_clock_tree(presets[i].cfgr, presets[i].acr, &t);

        char usb_str[16];
        if (t.usbclk_x2_hz == 0) {
            snprintf(usb_str, sizeof usb_str, "off");
        } else {
            snprintf(usb_str, sizeof usb_str, "%u.%u%s",
                     t.usbclk_x2_hz / 2000000u,
                     (t.usbclk_x2_hz % 2000000u) / 200000u,
                     usb_is_valid(&t) ? "" : "*");
        }

        printf("%-32s %4u M %4u M %4u M %5u M %4u M %4u M %8s %4u\n",
               presets[i].name,
               t.sysclk_hz / 1000000u,
               t.hclk_hz / 1000000u,
               t.pclk1_hz / 1000000u,
               t.tim_apb1_hz / 1000000u,
               t.pclk2_hz / 1000000u,
               t.adcclk_hz / 1000000u,
               usb_str,
               t.flash_ws);
    }

    return 0;
}
