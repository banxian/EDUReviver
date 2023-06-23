#ifndef _CONFIGSTORE_H
#define _CONFIGSTORE_H

#include <stdint.h>

struct patcher_config
{
    //const char* version;
    uint32_t sptop;     // 定位代码运行和覆盖位置(finearg头部)
    uint32_t lr;        // 原LR, 返回原usbcmd分拣流程
    uint32_t usbrx;
    bool isSES;         // SES需要使用R4-R6值
    uint8_t regCnt;     // 额外保存的寄存器数量(不含LR)
    //char cmdReg;      // 能自由使用的寄存器
    uint32_t regs[9];   // R4-R6值, 最多预留到R12
    uint8_t endgap;     // 栈尾空洞长度(IAR里可用, SES里不可用)
};

tm get_build_date(const char* version);
const patcher_config* find_patcher_config(const char* fwversion);
const patcher_config* analyst_firmware_stack(const void* fwbuf, size_t fwlen);
bool add_user_config(const char* fwversion, const patcher_config* config);

#endif