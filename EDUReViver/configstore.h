#ifndef _CONFIGSTORE_H
#define _CONFIGSTORE_H

#include <stdint.h>

struct patcher_config
{
    //const char* version;
    uint32_t sp;    // 定位代码运行和覆盖位置
    uint32_t lr;    // 返回dispatchcmd正常执行
    uint32_t usbrx;
    bool isSES;     // SES需要使用R4-R6值
    char cmdReg;    // 能自由使用的寄存器
    uint32_t R4, R5, R6;
    bool nopad;     // 栈末尾没有4字节空洞
};

tm get_build_date(const char* version);
const patcher_config* find_patcher_config(const char* fwversion);
const patcher_config* analyst_firmware_stack(const void* fwbuf, size_t fwlen);
bool add_user_config(const char* fwversion, const patcher_config* config);

#endif