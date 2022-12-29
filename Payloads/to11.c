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
void wmemcpy(uint32_t* dst, const uint32_t* src, size_t cnt) __attribute__((always_inline));
void wmemset(uint32_t* dst, uint32_t val, size_t cnt) __attribute__((always_inline));
char writeflashpage(uint32_t destaddr, const void* src);

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

char writeflashpage(uint32_t destaddr, const void* src)
{
    IAP iap_entry=(IAP)IAP_LOCATION;
    char flashok = 0;
    uint8_t sect = destaddr>=0x1A010000?(destaddr >> 16) + 7:(destaddr >> 13);
    uint32_t cmd[5];
    uint32_t* status = cmd;
    cmd[0] = 49; // init
    //((IAPINIT)iap_entry)(cmd); // 88b
    iap_entry(cmd, status); // 88b
    cmd[0] = 50;        // Prepare
    cmd[1] = sect;      // sector2, 8k (4000~5FFF)
    cmd[2] = sect;      // same
    cmd[3] = 0;         // bankA
    iap_entry(cmd, status); // 118b
    if (status[0] == CMD_SUCCESS) {
        cmd[0] = 59;      // Earse page
        cmd[1] = destaddr;
        cmd[2] = destaddr;
        cmd[3] = CPUKHZ;
        cmd[4] = 0;       // bankA
        iap_entry(cmd, status); // 168b
        if (status[0] == CMD_SUCCESS) {
            cmd[0] = 50;        // Prepare
            cmd[1] = sect;
            cmd[2] = sect;
            cmd[3] = 0;
            iap_entry(cmd, status);
            if (status[0] == CMD_SUCCESS) {
                cmd[0] = 51;    // Copy RAM to Flash
                cmd[1] = destaddr;
                cmd[2] = (uint32_t)src;
                cmd[3] = 0x200; // Page
                cmd[4] = CPUKHZ;
                iap_entry(cmd, status); // 208b
                if (status[0] == CMD_SUCCESS) {
                    //printf("Copy RAM to Flash OK!\n");
                    flashok = 1;
                } else {
                    //printf("Copy RAM to Flash faied: %d\n", status[0]);
                }
            } else {
                //printf("Prepare copy failed: %d\n", status[0]);
            }
        } else {
            //printf("Erase page failed: %d\n", status[0]);
        }
    } else {
        //printf("Prepare erase failed: %d\n", status[0]);
    }
	return flashok;
}

#define CRP1 0x12345678

void __main(void)
{
    char pagecache[0x200]; // task stack
    
    __disable_irq();
    FeedWWDT();
    if ((*(uint8_t*)0x1A002F89 == '0' || *(uint8_t*)0x1A002F89 == '2') && *(uint32_t*)0x1A005E04 == 0x32051976) {
        wmemcpy((uint32_t*)pagecache, (void*)0x1A002E00, 0x200 / 4);
        pagecache[0x189] = '1'; // 1A002F89
        writeflashpage(0x1A002E00, pagecache);
        
        wmemcpy((uint32_t*)pagecache, (void*)0x1A000000, 0x200 / 4);
        pagecache[0x139] = '1'; // 1A000139
        writeflashpage(0x1A000000, pagecache);
    }
    __enable_irq();
    return;
}

void FeedWWDT(void)
{
    if (LPC_WWDT->MOD & 1) {
        LPC_WWDT->FEED = 0xAA;
        LPC_WWDT->FEED = 0x55;
    }
}