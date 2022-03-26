#include <stdlib.h>
#include "LPC43xx.h"


volatile uint32_t btl_magic __attribute__((at(0x20000000)));

//__no_init char iapmem[16] @ 0x10089FF0;

#define IAP_LOCATION *(volatile unsigned int *)(0x10400100)
typedef void (*IAP)(unsigned int [],unsigned int[]);
typedef void (*IAPINIT)(unsigned int []);

#define CMD_SUCCESS 0

//__attribute__((zero_init)) char pagecache[0x200];

void FeedWWDT(void);
void InitUSART3(void);
void delay200ms(void);
void wmemcpy(uint32_t* dst, const uint32_t* src, size_t cnt) __attribute__((always_inline));
void wmemset(uint32_t* dst, uint32_t val, size_t cnt) __attribute__((always_inline));
char writeflashpage(uint32_t destaddr, const void* src);

void InitUSART3(void)
{
    LPC_CCU1->CLK_M4_GPIO_CFG |= 1;
    while (!(LPC_CCU1->CLK_M4_GPIO_STAT & 1));
    LPC_CCU2->CLK_APB2_USART3_CFG |= 1;
    while (!(LPC_CCU2->CLK_APB2_USART3_STAT & 1U))
    LPC_CCU1->CLK_M4_USART3_CFG |= 1; // autoen,wakeupen
    while (!(LPC_CCU1->CLK_M4_USART3_STAT & 1U));
    LPC_SCU->SFSP2_3 = 0x242; // P2_3 -> Fun2, EUPN, EZI, ZIF
    LPC_USART3->IER = 0; // Disable interrupts
    LPC_USART3->LCR = 0x83; // 8Bit, DLAB
    // 12M/16=750000 750000/6.51=115207
    // 750000/12*13/24=33854
    // 750000/3/(1+13/11)=114650??
    // 750000/6*12/13=115384
    LPC_USART3->DLL = 6;
    LPC_USART3->DLM = 0;
    LPC_USART3->FDR = 1 | (12 << 4); // ·Ö×ÓÔÚ×ó?!
    LPC_USART3->LCR = 0x3; // DLAB=0
}

// 12MHz, IAR
void delay200ms(void)
{
    for (uint32_t i = 483877; i; i--) {
        __NOP();
        __NOP();
    }
}

void wmemcpy(uint32_t* dst, const uint32_t* src, size_t cnt)
{
    while (cnt--) {
        *dst++ = *src++;
    }
}

void wmemset(uint32_t* dst, uint32_t val, size_t cnt)
{
    while (cnt--) {
        *dst++ = val;
    }
}

// 180Mhz PLL, 12M IRC
#define CPUKHZ 180000

//__attribute__((noreturn)) void nakemain(void);
void __main(void) __attribute__((used, section("RESET")));

void __main(void)
{
    for (int loop = 12; loop; loop--) {
        LPC_GPIO_PORT->CLR[5] = 1 << 4; // ON
        FeedWWDT();
        delay200ms();
        LPC_GPIO_PORT->SET[5] = 1 << 4; // OFF
        FeedWWDT();
        delay200ms();
    }
    return;
}

void FeedWWDT(void)
{
    if (LPC_WWDT->MOD & 1) {
        LPC_WWDT->FEED = 0xAA;
        LPC_WWDT->FEED = 0x55;
    }
}