#include <stddef.h>
#include "stm32f1xx.h"

/* C11 编译期断言：核对 GPIO_TypeDef 各成员偏移与 RM0008 第 9.5 节一致 */
_Static_assert(offsetof(GPIO_TypeDef, CRL)  == 0x00, "GPIO CRL offset mismatch");
_Static_assert(offsetof(GPIO_TypeDef, CRH)  == 0x04, "GPIO CRH offset mismatch");
_Static_assert(offsetof(GPIO_TypeDef, IDR)  == 0x08, "GPIO IDR offset mismatch");
_Static_assert(offsetof(GPIO_TypeDef, ODR)  == 0x0C, "GPIO ODR offset mismatch");
_Static_assert(offsetof(GPIO_TypeDef, BSRR) == 0x10, "GPIO BSRR offset mismatch");
_Static_assert(offsetof(GPIO_TypeDef, BRR)  == 0x14, "GPIO BRR offset mismatch");
_Static_assert(offsetof(GPIO_TypeDef, LCKR) == 0x18, "GPIO LCKR offset mismatch");

/* 核对 USART_TypeDef 各成员偏移与 RM0008 第 27.6.8 节一致 */
_Static_assert(offsetof(USART_TypeDef, SR)   == 0x00, "USART SR offset mismatch");
_Static_assert(offsetof(USART_TypeDef, DR)   == 0x04, "USART DR offset mismatch");
_Static_assert(offsetof(USART_TypeDef, BRR)  == 0x08, "USART BRR offset mismatch");
_Static_assert(offsetof(USART_TypeDef, CR1)  == 0x0C, "USART CR1 offset mismatch");
_Static_assert(offsetof(USART_TypeDef, CR2)  == 0x10, "USART CR2 offset mismatch");
_Static_assert(offsetof(USART_TypeDef, CR3)  == 0x14, "USART CR3 offset mismatch");
_Static_assert(offsetof(USART_TypeDef, GTPR) == 0x18, "USART GTPR offset mismatch");

/* 核对 RCC_TypeDef 关键成员偏移与 RM0008 第 7.3.11 节一致 */
_Static_assert(offsetof(RCC_TypeDef, CR)      == 0x00, "RCC CR offset mismatch");
_Static_assert(offsetof(RCC_TypeDef, CFGR)    == 0x04, "RCC CFGR offset mismatch");
_Static_assert(offsetof(RCC_TypeDef, APB2ENR) == 0x18, "RCC APB2ENR offset mismatch");
_Static_assert(offsetof(RCC_TypeDef, APB1ENR) == 0x1C, "RCC APB1ENR offset mismatch");
_Static_assert(offsetof(RCC_TypeDef, CSR)     == 0x24, "RCC CSR offset mismatch");

/* 核对 CMSIS-Core SCB_Type 关键成员偏移与 PM0056 第 4.4 节一致 */
_Static_assert(offsetof(SCB_Type, VTOR)  == 0x08, "SCB VTOR offset mismatch");
_Static_assert(offsetof(SCB_Type, AIRCR) == 0x0C, "SCB AIRCR offset mismatch");
_Static_assert(offsetof(SCB_Type, CFSR)  == 0x28, "SCB CFSR offset mismatch");
