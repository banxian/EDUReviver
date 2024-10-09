#include "targetver.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <map>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <io.h>
#include "configstore.h"
extern "C" {
#include "capstone/capstone.h"
};
#include "addon_func.h"


typedef std::map<std::string, patcher_config> ConfigMap;

//#define CFG_CNT 4
//patcher_config suppcfgs[CFG_CNT] = {
//    {
//        "J-Link V10 compiled Feb  2 2018 18:12:40", 0x100840A0, 0x1A010718, 0x1A010EDA, false
//    },
//    {
//        "J-Link V10 compiled Feb 21 2019 12:48:07", 0x10084008, 0x1A010A76, 0x1A011222, false
//    },
//    {
//        "J-Link V11 compiled Jul 29 2021 14:56:35", 0x10081D10, 0x1A01A110, 0x1A041EAC, true, 6, 0x10084765, 0x100838F0, 0x145
//    },
//    {
//        "J-Link V11 compiled Dec  9 2021 14:14:49", 0x10081D18, 0x1A01B25C, 0x1A04337C, true, 4, 0x145, 0x100838F0, 0x1008479E
//    }
//};

ConfigMap g_patchercfgs;

bool loadFirmwareConfigs(const wchar_t* cfgpath);

std::string normalize_version(const char* fwversion) {
    const char* compiled = strstr(fwversion, "compiled ");
    if (compiled == 0) {
        return fwversion;
    }
    compiled += strlen("compiled ");
    size_t pos = compiled - fwversion;
    std::string str(fwversion);
    str[pos] = toupper(str[pos]);
    str[pos + 1] = tolower(str[pos + 1]);
    str[pos + 2] = tolower(str[pos + 2]);
    return str;
}

const patcher_config* find_patcher_config(const char* fwversion)
{
    if (g_patchercfgs.empty()) {
        loadFirmwareConfigs(L"firmwarecfgs.lst");
        loadFirmwareConfigs(L"firmwarecfgs_usr.lst");
    }
    ConfigMap::const_iterator it = g_patchercfgs.find(normalize_version(fwversion));
    if (it == g_patchercfgs.end()) {
        return 0;
    }
    return &it->second;
}

bool loadFirmwareConfigs(const wchar_t* cfgpath)
{
    char* cfgbuf;
    int len = readallcontent(cfgpath, (void**)&cfgbuf);
    if (len == -1) {
        return false;
    }
    len++;
    cfgbuf = (char*)realloc(cfgbuf, len);
    int pos = -1;
    char* line = cfgbuf;
    while (++pos < len) {
        if (cfgbuf[pos] == '\n' || cfgbuf[pos] == '\r' || pos == len - 1) {
            cfgbuf[pos] = 0;
            if (line == cfgbuf + pos) {
                line++;
                continue;
            }
            if (strstr(line, "//") == 0) {
                char version[0x71];
                char issesstr[0x71];
                char regsstr[0x71];
                patcher_config item = {0,};
                //"J-Link V10 compiled Feb  2 2018 18:12:40", 0x100840A8, 0x1A010718, 0x1A010EDA, false
                //"J-Link V11 compiled Dec 14 2022 09:09:01", 0x10081D18, 0x1A035B9A, 0x1A045899, true, 0, 0x0, 0x10080F80, 0x145
                int cnt = sscanf(line, "\"%[^\"]\", 0x%X, 0x%X, 0x%X, %[^,], %hhd, %[^\n]", version, &item.sptop, &item.lr, &item.usbrx, issesstr, &item.endgap, regsstr);
                if (cnt >= 5) {
                    issesstr[0x70] = 0;
                    regsstr[0x70] = 0;
                    version[0x70] = 0;
                    item.isSES = _stricmp(issesstr, "true") == 0;
                    char* reg = strtok(regsstr, " ,");
                    int i = 0;
                    while (reg) {
                        item.regs[i++] = strtol(reg, 0, 16);
                        reg = strtok(NULL, " ,");
                    }
                    item.regCnt = i;
                    g_patchercfgs.insert(std::make_pair(std::string(version), item));
                } else {
                    printf("Bad line in config file: %s\n", line);
                }
            }
            line = cfgbuf + pos + 1;
        }
    }
    free(cfgbuf);
    return true;
}

bool add_user_config(const char* fwversion, const patcher_config* config)
{
    std::string version = normalize_version(fwversion);
    ConfigMap::const_iterator it = g_patchercfgs.find(version);
    if (it == g_patchercfgs.end()) {
        g_patchercfgs.insert(std::make_pair(version, *config));
        int fd = _wopen(L"firmwarecfgs_usr.lst", O_CREAT | O_RDWR | O_BINARY, S_IREAD | S_IWRITE); // O_BINARY not available in OSX
        if (fd == -1) {
            return false;
        }
        if (_lseek(fd, -1, SEEK_END) >= 0) {
            char end;
            _read(fd, &end, sizeof(end));
            if (end != '\n' && end != '\r') {
                _write(fd, "\r\n", 2);
            }
        }
        char line[128];
        int linelen;
        if (config->isSES) {
            linelen = sprintf_s(line, _countof(line), "\"%s\", 0x%08X, 0x%08X, 0x%08X, true, %d", version.c_str(), config->sptop, config->lr, config->usbrx, config->endgap);
            for (int i = 0; i < config->regCnt; i++) {
                linelen += sprintf_s(&line[linelen], _countof(line)-linelen, ", 0x%X", config->regs[i]);
            }
        } else {
            linelen = sprintf_s(line, _countof(line), "\"%s\", 0x%08X, 0x%08X, 0x%08X, false", version.c_str(), config->sptop, config->lr, config->usbrx);
        }
        _write(fd, line, linelen);
        _close(fd);
        return true;
    }
    return false;
}

struct RomReader
{
    unsigned char* fRomBuf;
    uint32_t fRomSize;
    uint32_t fRomBase;
    uint32_t fRamBase;
    bool isValid();
    uint32_t read_uint32_addr(uint32_t addr);
    uint32_t read_uint32_offset(uint32_t off);
    unsigned char* buf_at_addr(uint32_t addr);
    char* get_banner();
    tm get_build_date();
    RomReader(const void* rom, size_t romlen);
    ~RomReader();
};

bool RomReader::isValid()
{
    return fRomBuf != 0;
}

uint32_t RomReader::read_uint32_addr(uint32_t addr)
{
    return read_uint32_offset(addr - fRomBase);
}

uint32_t RomReader::read_uint32_offset(uint32_t off)
{
    return *(uint32_t*)(fRomBuf + off);
}

unsigned char* RomReader::buf_at_addr(uint32_t addr)
{
    return fRomBuf + (addr - fRomBase);
}

char* RomReader::get_banner()
{
    return (char*)fRomBuf + (fRomBase == 0x1A008000?0x130:0x210);
}

tm get_build_date(const char* version)
{
    tm date = {0, };
    // J-Link V9 compiled Oct 25 2018 11:46:07
    // J-Link V10 compiled Feb 21 2019 12:48:07
    // 01234567890123456789
    // Feb 21 2019 12:48:07
    //const char* compiled = version + strlen(version) - strlen("Feb 21 2019 12:48:07");
    const char* compiled = strstr(version, "compiled ");
    if (compiled == 0) {
        return date;
    }
    compiled += strlen("compiled ");
    date.tm_year = atoi(compiled + 7);
    date.tm_mday = atoi(compiled + 4);
    date.tm_hour = atoi(compiled + 12);
    date.tm_min = atoi(compiled + 15);
    date.tm_sec = atoi(compiled + 18);
    date.tm_mon = 0;
    const char* mon = "JanFebMarAprMayJunJulAugSepOctNovDec";
    for (int i = 0; i < 12; i++, mon += 3) {
        if (_strnicmp(compiled, mon, 3) == 0) {
            date.tm_mon = i + 1;
            break;
        }
    }
    return date;
}

tm RomReader::get_build_date()
{
    return ::get_build_date(get_banner());
}

RomReader::RomReader(const void* rom, size_t romlen)
    : fRomBuf(0)
    , fRomSize(0)
    , fRomBase(0)
    , fRamBase(0)
{
    //fRomBuf = (unsigned char*)malloc(romlen);
    //memcpy(fRomBuf, rom, romlen);
    fRomBuf = (unsigned char*)rom;
    fRomSize = romlen;
    uint32_t stackaddr = *(uint32_t*)fRomBuf;
    uint32_t resetaddr = *((uint32_t*)fRomBuf + 1);
    if (resetaddr >> 24 == 0x1A) {
        // LPC
        fRomBase = 0x1A008000;
    } else {
        // STM
        fRomBase = 0x08010000;
    }
    fRamBase = stackaddr;
}

RomReader::~RomReader()
{
    //if (fRomBuf) {
    //    free(fRomBuf);
    //    fRomBuf = 0;
    //}
}

uint32_t extract_ldr_pool(const cs_insn* insn)
{
    if (insn->id == ARM_INS_LDR && insn->detail->arm.operands[1].type == ARM_OP_MEM && insn->detail->arm.operands[1].mem.base == ARM_REG_PC) {
        return uint32_t((insn->address & ~3) + 4) + insn->detail->arm.operands[1].mem.disp;
    } else {
        return -1;
    }
}

typedef std::vector<int> IntVec;

void calc_stack(cs_insn *insn, size_t count, IntVec& vec);
void calc_call(cs_insn *insn, size_t count, IntVec& calls);
size_t guess_func_size(csh handle, const uint8_t *code, size_t maxsize, uint32_t address);

#define FREE_AND_EXIT(ptr) cs_free(insn, count); cs_close(&handle); return ptr;

bool is_reg_add_imm(cs_insn * insn, int reg)
{
    return insn->id == ARM_INS_ADD && 
        insn->detail->arm.operands[0].type == ARM_OP_REG && insn->detail->arm.operands[0].reg == reg &&
        insn->detail->arm.operands[1].type == ARM_OP_IMM;
}

#define SESTASKMAINSIZE 0x2500
extern bool read_emu_mem(uint32_t addr, void* buf, size_t size);

const patcher_config* analyst_firmware_stack(const void* fwbuf, size_t fwlen)
{
    RomReader reader(fwbuf, fwlen);
    if (!reader.isValid()) {
        return NULL;
    }
    tm date = reader.get_build_date();
    printf("Build date: %04d %02d/%02d %02d:%02d:%02d\n", date.tm_year, date.tm_mon, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec);

    uint32_t resetaddr = reader.read_uint32_offset(4);
    csh handle;
    cs_insn *insn;
    size_t count;    

    if (cs_open(CS_ARCH_ARM, cs_mode(CS_MODE_THUMB | CS_MODE_MCLASS), &handle) != CS_ERR_OK) {
        return NULL;
    }
    cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
    count = cs_disasm(handle, reader.buf_at_addr(resetaddr & ~1), 8, resetaddr & ~1, 0, &insn);
    if (count <= 0) {
        errprintf("Reset_Handler doesn't contains code. Not a valid firmware?\n");
        cs_close(&handle);
        return NULL;
    }
    uint32_t startaddr;
    // SES固件?
    if (count == 2) {
        // LPC4300_Startup.s, no vfp
        if (insn[0].id == ARM_INS_BL && insn[1].id == ARM_INS_BL) {
            startaddr = insn[1].detail->arm.operands[0].imm;
            printf("_start: %08X\n", startaddr);
        } else {
            errprintf("Firmware was not build by SES\n");
            FREE_AND_EXIT(NULL);
        }
        cs_free(insn, count);
        // SEGGER_THUMB_Startup.s
        // LDR     R4, =__SEGGER_init_table__
        count = cs_disasm(handle, reader.buf_at_addr(startaddr), 4, startaddr, 0, &insn);
        cs_arm_op op0 = insn[0].detail->arm.operands[0];
        cs_arm_op op1 = insn[0].detail->arm.operands[1];
        uint32_t inittable;
        if (count && insn[0].id == ARM_INS_LDR && op0.type == ARM_OP_REG && op0.reg == ARM_REG_R4 && op1.type == ARM_OP_MEM && op1.reg == ARM_REG_PC) {
            inittable = extract_ldr_pool(&insn[0]);
            inittable = reader.read_uint32_addr(inittable);
        } else {
            errprintf("Firmware was not build by SES.\n");
            FREE_AND_EXIT(NULL);
        }
        cs_free(insn, count);
        // find __SEGGER_init_done
        int ahead = 0;
        uint32_t mainaddr = 0;
        while (ahead++ < 20 && mainaddr == 0) {
            uint32_t initfunc = reader.read_uint32_addr(inittable);
            inittable += 4;
            // SEGGER_crtinit_v7em_little.o
            // zpak|copy C
            // ADDS    R4, #8
            count = cs_disasm(handle, reader.buf_at_addr(initfunc & ~1), 0x10, initfunc & ~1, 4, &insn);
            if (count == 4 && is_reg_add_imm(&insn[2], ARM_REG_R4)) {
                inittable += insn[2].detail->arm.operands[1].imm;
            }
            if (count == 4 && is_reg_add_imm(&insn[3], ARM_REG_R4)) {
                inittable += insn[3].detail->arm.operands[1].imm;
            }
            // BL      main
            // B       exit
            if (count >= 2 && insn[0].id == ARM_INS_BL && insn[1].id == ARM_INS_B && insn[1].detail->arm.operands[0].imm == insn[1].address) {
                mainaddr = insn[0].detail->arm.operands[0].imm;
            }
            cs_free(insn, count);
        }
        if (mainaddr == 0) {
            errprintf("Firmware was not build by SES?\n");
            cs_close(&handle);
            return NULL;
        }
        // V11 Ses Size: 1D2
        count = cs_disasm(handle, reader.buf_at_addr(mainaddr & ~1), 0x200, (mainaddr & ~1), 0, &insn);
        uint32_t taskstack = 0, stacksize = 0;
        uint32_t maintaskaddr = 0, taskstackinit = 0;
        for (size_t j = 0; j < count; j++) {
            //printf("%08x\t%s\t\t%s\n", (uint32_t)insn[j].address, insn[j].mnemonic, insn[j].op_str);
            cs_arm* arm = &insn[j].detail->arm;
            if ((insn[j].id == ARM_INS_MOV || insn[j].id == ARM_INS_MOVW) && arm->operands[0].type == ARM_OP_REG && arm->operands[1].type == ARM_OP_IMM) {
                if (arm->operands[0].reg == ARM_REG_R0) {
                    stacksize = arm->operands[1].imm;
                }
                if (arm->operands[0].reg == ARM_REG_R1) {
                    taskstack = arm->operands[1].imm;
                }
                if (arm->operands[0].reg == ARM_REG_R3 && taskstackinit) {
                    maintaskaddr = arm->operands[1].imm;
                }
            }
            if (insn[j].id == ARM_INS_MOVT && arm->operands[0].type == ARM_OP_REG && arm->operands[1].type == ARM_OP_IMM) {
                if (arm->operands[0].reg == ARM_REG_R0) {
                    *((uint16_t*)&stacksize + 1) = arm->operands[1].imm;
                }
                if (arm->operands[0].reg == ARM_REG_R1) {
                    *((uint16_t*)&taskstack + 1) = arm->operands[1].imm;
                }
                if (arm->operands[0].reg == ARM_REG_R3 && taskstackinit) {
                    *((uint16_t*)&maintaskaddr + 1) = arm->operands[1].imm;
                }
            }
            // STRD.W  R1, R0, [SP]
            if (insn[j].id == ARM_INS_STRD && arm->operands[2].type == ARM_OP_MEM && arm->operands[2].mem.base == ARM_REG_SP) {
                taskstackinit = (taskstack + stacksize) & ~0xF;
                //printf("taskstackinit: %08X\n", taskstackinit);
            }
            // BL      OS_TaskCreate
            if (taskstackinit && insn[j].id == ARM_INS_BL) {
                break;
            }
        }
        cs_free(insn, count);
        //dispatchcmd成为了taskmain的一部分
        //1A0197AC 000 98 B0                       SUB     SP, SP, #0x60
        //1A01B25A 060 80 47                       BLX     R0
        count = cs_disasm(handle, reader.buf_at_addr(maintaskaddr & ~1), SESTASKMAINSIZE, (maintaskaddr & ~1), 0, &insn);
        IntVec spdvec;
        calc_stack(insn, count, spdvec);
        uint32_t cmdtable = 0;
        int cmdcnt = 0;
        uint32_t r456val[9];
        bool r456dirty[9] = {true, true, true, true, true, true, true, true, true};
        int spd1, spd2;
        uint32_t dispatchlr;
        uint32_t usbrxbuf = 0;
        int rawCmdRegIdx = -1;
        for (size_t j = 0; j < count; j++) {
            //printf("%08x %03x \t%s\t\t%s\n", (uint32_t)insn[j].address, -spdvec[j], insn[j].mnemonic, insn[j].op_str);
            cs_arm* arm = &insn[j].detail->arm;
            // BLX R0
            if (j > 5 && insn[j].id == ARM_INS_BLX && arm->operands[0].type == ARM_OP_REG) {
                // LDR.W   R0, [R1,R0,LSL#2]
                cs_arm* prevarm = &insn[j-1].detail->arm;
                // index uxtb cmd
                // base cmdtable
                // shift 2
                if (insn[j-1].id == ARM_INS_LDR && prevarm->operands[0].reg == arm->operands[0].reg && 
                    prevarm->operands[1].type == ARM_OP_MEM && prevarm->operands[1].shift.type == ARM_SFT_LSL && prevarm->operands[1].shift.value == 2)
                {
                    dispatchlr = (uint32_t)insn[j+1].address;
                    spd1 = spdvec[j];
                    int cmdreg = prevarm->operands[1].mem.index;
                    int tablereg = prevarm->operands[1].mem.base;
                    j--;
                    // 寻找这对组合
                    //ADDS    R6, #0x65
                    //UXTB    R0, R6
                    while (j--) {
                        cs_arm* arm = &insn[j].detail->arm;
                        cs_arm* prevarm = &insn[j-1].detail->arm;
                        if (insn[j].id == ARM_INS_UXTB && arm->operands[0].reg == cmdreg &&
                            is_reg_add_imm(&insn[j-1], arm->operands[1].reg))
                        {
                            // R6=rawcmd
                            cmdcnt = prevarm->operands[1].imm;
                            printf("rawcmd reg: %s, val: %08X\n", cs_reg_name(handle, prevarm->operands[0].reg), cmdcnt + 0xE0);
                            rawCmdRegIdx = prevarm->operands[0].reg - ARM_REG_R0;
                            break;
                        }
                    }
                    while (j++) {
                        //MOV     R1, #g_usbcmds
                        cs_arm* arm = &insn[j].detail->arm;
                        if ((insn[j].id == ARM_INS_MOV || insn[j].id == ARM_INS_MOVW) && arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg == tablereg && arm->operands[1].type == ARM_OP_IMM) {
                            cmdtable = arm->operands[1].imm;
                        }
                        if (insn[j].id == ARM_INS_MOVT && arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg == tablereg && arm->operands[1].type == ARM_OP_IMM) {
                            *((uint16_t*)&cmdtable + 1) = arm->operands[1].imm;
                            break;
                        }
                    }
                    // 只解析到准备调用usbfunc的地方, 下面就是LDR和BLX了
                    if (cmdcnt && cmdtable) {
                        printf("g_usbcmds: %08X, count: %d\n", cmdtable, cmdcnt);
                        break;
                    }
                }
                // lookup usbdfuncs table later
            }
            // 跟踪taskmain函数中出现的目的R4~R6赋值
            const char* unknown = 0;
            if (arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg >= ARM_REG_R4 && arm->operands[0].reg <= ARM_REG_R12) {
                // 持续跟踪R4/R5/R6的MOVW/MOVT对值
                uint32_t* lpreg = &r456val[arm->operands[0].reg - ARM_REG_R4];
                bool* lpdirty = &r456dirty[arm->operands[0].reg - ARM_REG_R4];
                if ((insn[j].id == ARM_INS_MOV || insn[j].id == ARM_INS_MOVW) && arm->operands[1].type == ARM_OP_IMM) {
                    *lpreg = arm->operands[1].imm;
                    *lpdirty = false;
                } else
                if (insn[j].id == ARM_INS_MOVT && arm->operands[1].type == ARM_OP_IMM) {
                    *((uint16_t*)lpreg + 1) = arm->operands[1].imm;
                    *lpdirty = false;
                } else
                // 跟踪V12固件出现的新型R6赋值
                // ADD R6, SP, #0xB0+lpR6
                if ((insn[j].id == ARM_INS_ADD || insn[j].id == ARM_INS_SUB) && arm->operands[2].type == ARM_OP_IMM) {
                    if (arm->operands[1].type == ARM_OP_REG && arm->operands[1].reg == ARM_REG_SP) {
                        uint32_t spval = maintaskaddr + spdvec[j];
                        *lpreg = insn[j].id == ARM_INS_ADD?spval+arm->operands[2].imm:spval-arm->operands[2].imm;
                        *lpdirty = false;
                    } else {
                        unknown = "Source   ";
                        *lpdirty = true;
                    }
                } else
                if (insn[j].id != ARM_INS_STR && insn[j].id != ARM_INS_STRB && insn[j].id != ARM_INS_STRH && insn[j].id != ARM_INS_STRD 
                    && insn[j].id != ARM_INS_CMP && insn[j].id != ARM_INS_TST && insn[j].id != ARM_INS_CBZ && insn[j].id != ARM_INS_CBNZ && insn[j].id != ARM_INS_BLX)
                {
                    unknown = "Operation";
                    *lpdirty = true;
                }
            }
#ifdef _DEBUG
            if (unknown) {
                //printf("Unsupported %s %08x %03x \t%s\t\t%s\n", unknown, (uint32_t)insn[j].address, -spdvec[j], insn[j].mnemonic, insn[j].op_str);
            }
#endif
        }
        cs_free(insn, count);
#define r4val r456val[0]
#define r5val r456val[1]
#define r6val r456val[2]
        printf("R4: %08X R5: %08X R6: %08X\n", r4val, r5val, r6val);
        if (cmdcnt == 0) {
            errprintf("Analyst failed on command table!\n");
            cs_close(&handle);
            return NULL;
        }
        // 后半部分指针从后数
        uint32_t cmde0func = reader.read_uint32_addr(cmdtable + (cmdcnt - 1 - (0xFF - 0xE0)) * 4);
        uint32_t cmdfefunc = reader.read_uint32_addr(cmdtable + (cmdcnt - 1 - (0xFF - 0xFE)) * 4);
        size_t e0size = guess_func_size(handle, reader.buf_at_addr(cmde0func & ~1), 0x70, (cmde0func & ~1));
        printf("cmd_e0_fine_write_read: %08X, %X bytes\n", cmde0func, e0size);
        // 寻找usbrxbuf崩溃点, 5C
        count = cs_disasm(handle, reader.buf_at_addr(cmde0func & ~1), e0size, (cmde0func & ~1), 0, &insn);
        calc_stack(insn, count, spdvec);
        // 第一次call是settimer(-1), 第二次是usbrx(arg,12), 第三次是usbrx(writebuf,writelen)
        // 第二可能不内联, 第三可能跟if(writelen)条件一起外联为usbrxif
        // 考虑可能分开内外联, 不能限制两次rx寄存器/地址
        IntVec calls;
        calc_call(insn, count, calls);
        if (calls.size() < 5) {
            errprintf("unexpected cmd_e0_fine_write_read calls count: %u.\n", calls.size());
            FREE_AND_EXIT(NULL);
        }
        size_t firstcall = calls[0], firstrx = calls[1], secondrx = calls[2];
        uint32_t argspdelta, savedregs, endgap;
        bool rxaddrknown = (insn[firstrx].id == ARM_INS_BL || insn[secondrx].id == ARM_INS_BL);
        uint32_t argstack;
        for (size_t i = 0; i < count; i++) {
            cs_arm* arm = &insn[i].detail->arm;
            if (i < firstcall) {
                if (insn[i].id == ARM_INS_PUSH && arm->operands[arm->op_count-1].reg == ARM_REG_LR) {
                    savedregs = arm->op_count - 1;
                }
            }
            if (i < firstrx) {
                if (insn[i].id == ARM_INS_MOV && arm->op_count == 2 && arm->operands[1].type == ARM_OP_REG && arm->operands[1].reg == ARM_REG_SP) {
                    argspdelta = 0;
                }
                if (insn[i].id == ARM_INS_ADD && arm->op_count == 3 && arm->operands[1].type == ARM_OP_REG && arm->operands[1].reg == ARM_REG_SP) {
                    argspdelta = arm->operands[2].imm;
                }
            }
            if (i == secondrx) {
                spd2 = spdvec[i]; // -30~-40
                argstack = taskstackinit + spd1 + spd2 + argspdelta;
                endgap = (-spd2) - (savedregs + 1)*4 - 0x30 - argspdelta;
                printf("argstack: %08X original LR: %08X, endgap: %d\n", argstack, dispatchlr, endgap);
            }
        }
        if (rxaddrknown) {
            usbrxbuf = insn[(insn[firstrx].id == ARM_INS_BL)?firstrx:secondrx].detail->arm.operands[0].imm;
        } else {
            int funcsreg, blxrxreg = insn[firstrx].detail->arm.operands[0].reg;
            // first movw/movt before firstrx call
            for (size_t i = 0; i < firstrx; i++) {
                if (insn[i].id == ARM_INS_MOVT) {
                    funcsreg = insn[i].detail->arm.operands[0].reg;
                    break;
                }
            }
            // 寻找指针源, 以便后面回溯指针赋值
            uint32_t pipefuncs;
            int midreg = ARM_REG_INVALID, basereg = ARM_REG_INVALID;
            int rxdisp;
            for (size_t j = firstcall+1; j < firstrx; j++) {
                cs_arm* arm = &insn[j].detail->arm;
                if (midreg == ARM_REG_INVALID) {
                    // MOV32   R6, #g_usbd_funcs
                    if (arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg == funcsreg && arm->operands[1].type == ARM_OP_IMM) {
                        if ((insn[j].id == ARM_INS_MOV || insn[j].id == ARM_INS_MOVW)) {
                            pipefuncs = arm->operands[1].imm;
                        } else if (insn[j].id == ARM_INS_MOVT) {
                            *((uint16_t*)&pipefuncs + 1) = arm->operands[1].imm;
                            printf("g_pipe_funcs: %08X\n", pipefuncs);
                            midreg = arm->operands[0].reg;
                        }
                    }
                } else if (basereg == ARM_REG_INVALID) {
                    if (insn[j].id == ARM_INS_LDR &&
                        arm->operands[0].type == ARM_OP_REG && 
                        arm->operands[1].type == ARM_OP_MEM && arm->operands[1].mem.base == midreg && arm->operands[1].mem.disp == 0)
                    {
                        // LDR     R0, [R6]
                        basereg = arm->operands[0].reg;
                    }
                } else if (insn[j].id == ARM_INS_LDR &&
                    arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg == blxrxreg && 
                    arm->operands[1].type == ARM_OP_MEM && arm->operands[1].mem.base == basereg)
                {
                    // LDR     R2, [R0,#8]
                    rxdisp = arm->operands[1].mem.disp;
                    break;
                }
            }
            // 求得cmd_e0_fine_write_read最后的发送
            uint32_t usbtxbuf = insn[calls.back()].detail->arm.operands[0].imm;
            printf("usbtxbuf: %08X\n", usbtxbuf);
            cs_free(insn, count);
            // 在apptaskmain里寻找对usbdfuncs的赋值, 假设第一个是对的
            //ROM:1A019116 0B0  MOV     R0, #g_usbd_funcs
            //ROM:1A01911E 0B0  MOV     R1, #usbdfuncs
            //ROM:1A019126 0B0  STR     R1, [R0]
            count = cs_disasm(handle, reader.buf_at_addr(maintaskaddr & ~1), SESTASKMAINSIZE, (maintaskaddr & ~1), 0, &insn);
            uint32_t r0val, r1val;
            for (size_t j = 0; j < count - 1; j++) {
                //printf("%08x\t%s\t\t%s\n", (uint32_t)insn[j].address, insn[j].mnemonic, insn[j].op_str);
                cs_arm* arm = &insn[j].detail->arm;
                if (arm->operands[1].type == ARM_OP_IMM && arm->operands[0].type == ARM_OP_REG && (arm->operands[0].reg == ARM_REG_R0 || arm->operands[0].reg == ARM_REG_R1)) {
                    if ((insn[j].id == ARM_INS_MOV || insn[j].id == ARM_INS_MOVW)) {
                        (arm->operands[0].reg == ARM_REG_R0?r0val:r1val) = arm->operands[1].imm;
                    } else if (insn[j].id == ARM_INS_MOVT) {
                        *((uint16_t*)(arm->operands[0].reg == ARM_REG_R0?&r0val:&r1val) + 1) = arm->operands[1].imm;
                    } 
                }
                if (insn[j].id == ARM_INS_STR &&
                    arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg == ARM_REG_R1 && 
                    arm->operands[1].type == ARM_OP_MEM && arm->operands[1].mem.disp == 0 && arm->operands[1].mem.base == ARM_REG_R0 && r0val == pipefuncs)
                {
                    uint32_t rxbeginaddr = reader.read_uint32_addr(r1val + rxdisp);
                    cs_insn* ins2;
                    size_t cnt2 = cs_disasm(handle, reader.buf_at_addr(rxbeginaddr & ~1), 0x100, (rxbeginaddr & ~1), 0, &ins2);
                    bool hitted = false;
                    for (size_t j = 0; j < cnt2 - 1; j++) {
#ifdef _DEBUG
                        printf("%08x\t%s\t\t%s\n", (uint32_t)ins2[j].address, ins2[j].mnemonic, ins2[j].op_str);
#endif
                        cs_arm* arm = &ins2[j].detail->arm;
                        //ROM:1A0458B0  MOVW    R3, #3000
                        //ROM:1A0458B4  BL      sub_1A0410A0
                        if (ins2[j].id == ARM_INS_MOVW && arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg == ARM_REG_R3 &&
                            arm->operands[1].type == ARM_OP_IMM && arm->operands[1].imm == 3000 &&
                            ins2[j+1].id == ARM_INS_BL)
                        {
                            usbrxbuf = reader.read_uint32_addr(r1val + rxdisp); // TODO: use dispatch from fine_read_write
                            printf("usbrxbuf: %08X\n", usbrxbuf);
                            hitted = true;
                            break;
                        }
                    }
                    cs_free(ins2, cnt2);
                    if (hitted) {
                        break;
                    }
                }
            }
        }
        cs_free(insn, count);
        // 优先从CMD_READ_EMU_MEM处理函数里面抓R4~R9
        bool userealreg = false;
        uint32_t realregs[9];
        count = cs_disasm(handle, reader.buf_at_addr(cmdfefunc & ~1), 4, (cmdfefunc & ~1), 1, &insn);
        if (insn[0].id == ARM_INS_PUSH) {
            cs_arm* arm = &insn[0].detail->arm;
            if (arm->op_count > savedregs && arm->operands[arm->op_count-1].reg == ARM_REG_LR) {
                uint32_t r4addr = taskstackinit + spd1 - arm->op_count * 4;
                //printf("r4addr: 0x%08X\n", r4addr);
                if (read_emu_mem(r4addr, realregs, savedregs * sizeof(uint32_t))) {
                    userealreg = true;
                }
            }
        }
        cs_free(insn, count);
        uint32_t* finalregs = userealreg?realregs:r456val;
        // version, sp, lr, uxbrx, true, cmdReg, R4, R5, R6
        if (rawCmdRegIdx >= 4 && rawCmdRegIdx <= 12) {
            finalregs[rawCmdRegIdx - 4] = cmdcnt + 0xE0;
            r456dirty[rawCmdRegIdx - 4] = false;
        }
        if (userealreg == false) {
            bool fatal = false;
            for (size_t i = 0; i < savedregs; i++) {
                if (r456dirty[i]) {
                    errprintf("R%d val is bad!\n", 4 + i);
                    if (i != rawCmdRegIdx - 4) {
                        fatal = true;
                    }
                }
            }
            if (fatal) {
                cs_close(&handle);
                return NULL;
            }
        }
        patcher_config item = {argstack, dispatchlr, usbrxbuf, true, (char)savedregs};
        memcpy(item.regs, finalregs, savedregs * sizeof(uint32_t));
        item.endgap = endgap;
        add_user_config(reader.get_banner(), &item);
    }
    // IAR固件?
    if (count == 4) {
        if (insn[2].id == ARM_INS_LDR && insn[2].detail->arm.operands[1].reg == ARM_REG_PC) {
            startaddr = extract_ldr_pool(&insn[2]);
            startaddr = reader.read_uint32_addr(startaddr);
            printf("__iar_program_start: %08X\n", startaddr);
        } else {
            errprintf("Firmware not build by EWARM\n");
            FREE_AND_EXIT(NULL);
        }
        cs_free(insn, count);
        count = cs_disasm(handle, reader.buf_at_addr((startaddr & ~1) + 8), 4, (startaddr & ~1) + 8, 0, &insn);
        if (count <= 0 || insn[0].id != ARM_INS_BL) {
            errprintf("Firmware not build by EWARM.\n");
            FREE_AND_EXIT(NULL);
        }
        uint32_t __cmainaddr = insn[0].detail->arm.operands[0].imm;
        cs_free(insn, count);
        // BEQ     _call_main
        count = cs_disasm(handle, reader.buf_at_addr((__cmainaddr & ~1) + 6), 8, (__cmainaddr & ~1) + 6, 0, &insn);
        if (count <= 0 || insn[0].id != ARM_INS_B) {
            errprintf("Firmware not build by EWARM?\n");
            FREE_AND_EXIT(NULL);
        }
        uint32_t callmainaddr = insn[0].detail->arm.operands[0].imm;
        cs_free(insn, count);
        // BL      main
        count = cs_disasm(handle, reader.buf_at_addr((callmainaddr & ~1) + 2), 8, (callmainaddr & ~1) + 2, 0, &insn);
        if (count <= 0 || insn[0].id != ARM_INS_BL) {
            errprintf("Firmware not build by EWARM!\n");
            FREE_AND_EXIT(NULL);
        }
        uint32_t mainaddr = insn[0].detail->arm.operands[0].imm;
        cs_free(insn, count);
        // v10/v9 main function size 50/56
        count = cs_disasm(handle, reader.buf_at_addr(mainaddr & ~1), 0x70, (mainaddr & ~1), 0, &insn);
        IntVec spdvec;
        calc_stack(insn, count, spdvec);
        //size_t seekend = 0;
        uint32_t maintaskaddr = -1, taskstackinit = -1;
        int step = 0;
        for (size_t j = 0; j < count; j++) {
            //printf("%08x %03x \t%s\t\t%s\n", (uint32_t)insn[j].address, -spdvec[j], insn[j].mnemonic, insn[j].op_str);
            cs_arm* arm = &insn[j].detail->arm;
            // LDR     R3, =(app_maintask+1)
            if (step == 0 && insn[j].id == ARM_INS_LDR && 
                arm->op_count == 2 && arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg == ARM_REG_R3 &&
                arm->operands[1].type == ARM_OP_MEM && arm->operands[1].mem.base == ARM_REG_PC) {
                    maintaskaddr = reader.read_uint32_addr(extract_ldr_pool(&insn[j]));
                    step++;
            }
            // BL      app_createtask
            if (step == 1 && insn[j].id == ARM_INS_BL) {
                uint32_t taskstack, stacksize;
                bool stackok = false, sizeok = false;
                int stackreg = ARM_REG_INVALID, sizereg = ARM_REG_INVALID;
                while(j--) {
                    //printf("%08x %03x \t%s\t\t%s\n", (uint32_t)insn[j].address, -spdvec[j], insn[j].mnemonic, insn[j].op_str);
                    cs_arm* arm = &insn[j].detail->arm;
                    // 先找[SP]和[SP+4]填充stackreg和sizereg
                    // STR     R1, [SP,#0x10+StackSize]
                    // STR     R0, [SP,#0x10+pStack]
                    if (insn[j].id == ARM_INS_STR && arm->op_count == 2 && arm->operands[0].type == ARM_OP_REG && arm->operands[1].type == ARM_OP_MEM && arm->operands[1].mem.base == ARM_REG_SP) {
                        if (arm->operands[1].mem.disp == 4) {
                            if (sizereg == ARM_REG_INVALID) {
                                sizereg = arm->operands[0].reg;
                            }
                        } else if (arm->operands[1].mem.disp == 0) {
                            if (stackreg == ARM_REG_INVALID) {
                                stackreg = arm->operands[0].reg;
                            }
                        }
                    }
                    // LDR     R0, =unk_100832F8
                    if (insn[j].id == ARM_INS_LDR && 
                        arm->op_count == 2 && arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg == stackreg &&
                        arm->operands[1].type == ARM_OP_MEM && arm->operands[1].mem.base == ARM_REG_PC) {
                            taskstack = reader.read_uint32_addr(extract_ldr_pool(&insn[j]));
                            stackok = true;
                    }
                    //  MOV.W   R1, #0xE00
                    if (insn[j].id == ARM_INS_MOV && 
                        arm->op_count == 2 && arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg == sizereg &&
                        arm->operands[1].type == ARM_OP_IMM && arm->operands[1].imm >= 0x100) {
                            stacksize = arm->operands[1].imm;
                            sizeok = true;
                    }
                    if (stackok && sizeok) {
                        taskstackinit = (taskstack + stacksize) & ~7;
                        break;
                    }
                }
                break;
            }
        }
        cs_free(insn, count);
        if (maintaskaddr == -1 || taskstackinit == -1) {
            errprintf("Analyst failed on embOS!\n");
            cs_close(&handle);
            return NULL;
        }
        // 寻找dispatchusbcmd 110/10C
        count = cs_disasm(handle, reader.buf_at_addr(maintaskaddr & ~1), 0x120, (maintaskaddr & ~1), 0, &insn);
        calc_stack(insn, count, spdvec);
        step = 0;
        uint32_t target = -1;
        int spd1, spd2, spd3;
        for (size_t j = 0; j < count; j++) {
            //printf("%08x %03x \t%s\t\t%s\n", (uint32_t)insn[j].address, -spdvec[j], insn[j].mnemonic, insn[j].op_str);
            cs_arm* arm = &insn[j].detail->arm;
            // LDRB.W  R0, [SP,#8+cmd]
            if (step == 0 && insn[j].id == ARM_INS_LDRB && 
                arm->op_count == 2 && arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg == ARM_REG_R0 &&
                arm->operands[1].type == ARM_OP_MEM && arm->operands[1].mem.base == ARM_REG_SP) {
                    //seekend = j;
                    step++;
            }
            if (step == 1 && insn[j].id == ARM_INS_BL) {
                target = arm->operands[0].imm;
                printf("dispatchusbcmd: %08X\n", target);
                spd1 = spdvec[j];
                step++;
                break;
            }
        }
        cs_free(insn, count);
        if (target == -1) {
            errprintf("Failed to find dispatchusbcmd!\n");
            return NULL;
        }
        // 寻找usbcmdtable
        count = cs_disasm(handle, reader.buf_at_addr(target & ~1), 0x120, (target & ~1), 0, &insn);
        calc_stack(insn, count, spdvec);
        uint32_t dispatchlr, cmdtable;
        int cmdcnt = 0;
        for (size_t j = 0; j < count; j++) {
            //printf("%08x %03x \t%s\t\t%s\n", (uint32_t)insn[j].address, -spdvec[j], insn[j].mnemonic, insn[j].op_str);
            cs_arm* arm = &insn[j].detail->arm;
            // BLX     R0
            if (insn[j].id == ARM_INS_BLX && arm->operands[0].type == ARM_OP_REG) {
                spd2 = spdvec[j];
                dispatchlr = (uint32_t)insn[j].address + insn[j].size;
                step = 0;
                int reg = ARM_REG_INVALID;
                // 往回寻找ADD/LDR
                while (j--) {
                    //printf("%08x %03x \t%s\t\t%s\n", (uint32_t)insn[j].address, -spdvec[j], insn[j].mnemonic, insn[j].op_str);
                    cs_arm* arm = &insn[j].detail->arm;
                    // V10/V9会有不同的寻址指令
                    // ADR.W   R0, g_usb_cmdtable
                    // LDR     R0, =g_cmdtable
                    if (step == 0 && 
                        ((insn[j].id == ARM_INS_ADDW && arm->op_count == 3 && arm->operands[1].type == ARM_OP_REG && arm->operands[1].reg == ARM_REG_PC) || 
                        (insn[j].id == ARM_INS_LDR  && arm->op_count == 2 && arm->operands[1].type == ARM_OP_MEM && arm->operands[1].mem.base == ARM_REG_PC)) &&
                        arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg == ARM_REG_R0
                        ) {
                            if (insn[j].id == ARM_INS_LDR) {
                                cmdtable = reader.read_uint32_addr(extract_ldr_pool(&insn[j]));
                            } else {
                                cmdtable = (insn[j].address &~3) + 4 + arm->operands[2].imm;
                            }
                            printf("cmdtable: %08X\n", cmdtable);
                            step++;
                    }
                    // UXTB    R4, R4
                    if (step == 1 && insn[j].id == ARM_INS_UXTB) {
                        reg = arm->operands[1].reg;
                        step++;
                    }
                    // ADDS    R4, #0x63
                    if (step == 2 && insn[j].id == ARM_INS_ADD && arm->op_count == 2 && arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg == reg) {
                        cmdcnt = arm->operands[1].imm;
                        step++;
                        break;
                    }
                }
                break;
            }
        }
        cs_free(insn, count);
        if (cmdcnt == 0) {
            errprintf("Analyst failed on command table!\n");
            cs_close(&handle);
            return NULL;
        }
        uint32_t cmde0func = reader.read_uint32_addr(cmdtable + (cmdcnt - 1 - (0xFF - 0xE0)) * 4); // e0肯定第二段
        printf("cmd_e0_fine_write_read: %08X\n", cmde0func);
        // 寻找usbrxbuf崩溃点, 4c
        size_t e0size = guess_func_size(handle, reader.buf_at_addr(cmde0func & ~1), 0x60, (cmde0func & ~1));
        count = cs_disasm(handle, reader.buf_at_addr(cmde0func & ~1), e0size, (cmde0func & ~1), 0, &insn);
        IntVec calls;
        calc_call(insn, count, calls);
        if (calls.size() < 5) {
            errprintf("unexpected cmd_e0_fine_write_read calls count: %u.\n", calls.size());
            FREE_AND_EXIT(NULL);
        }
        calc_stack(insn, count, spdvec);
        size_t firstrx = calls[1], secondrx = calls[2];
        uint32_t usbrxbuf = insn[firstrx].detail->arm.operands[0].imm;
        spd3 = spdvec[secondrx];
        int argspdelta = -1;
        for (size_t i = 0; i < firstrx; i++) {
            //printf("%08x %03x \t%s\t\t%s\n", (uint32_t)insn[j].address, -spdvec[j], insn[j].mnemonic, insn[j].op_str);
            cs_arm* arm = &insn[i].detail->arm;
            if (insn[i].id == ARM_INS_MOV && arm->op_count == 2 && arm->operands[1].type == ARM_OP_REG && arm->operands[1].reg == ARM_REG_SP) {
                argspdelta = 0;
            }
            if (insn[i].id == ARM_INS_ADD && arm->op_count == 3 && arm->operands[1].type == ARM_OP_REG && arm->operands[1].reg == ARM_REG_SP) {
                argspdelta = arm->operands[2].imm;
            }
        }
        uint32_t argstack = taskstackinit + spd1 + spd2 + spd3 + argspdelta;
        printf("argstack: %08X original LR:%08X usbrxbuf: %08X\n", argstack, dispatchlr, usbrxbuf);
        // 求得cmd_e0_fine_write_read最后的发送
        uint32_t usbtxbuf = insn[calls.back()].detail->arm.operands[0].imm;
        printf("usbtxbuf: %08X\n", usbtxbuf);
        cs_free(insn, count);
        // version, sp, lr, uxbrx, false
        //printf("\"%s\", 0x%08X, 0x%08X, 0x%08X, false\n", reader.get_banner(), currstack, dispatchlr, usbrxbuf);
        patcher_config item = {argstack, dispatchlr, usbrxbuf, false};
        add_user_config(reader.get_banner(), &item);
    }
    cs_close(&handle);

    return find_patcher_config(reader.get_banner());
}

void calc_stack(cs_insn *insn, size_t count, IntVec& vec)
{
    int spd = 0;
    vec.resize(count);
    for (size_t i = 0; i < count; i++) {
        vec[i] = spd;
        if (insn[i].id == ARM_INS_PUSH) {
            spd -= insn[i].detail->arm.op_count * 4;
        }
        if (insn[i].id == ARM_INS_POP) {
            spd += insn[i].detail->arm.op_count * 4;
        }
        // ldr lr,[sp],#4
        if (insn[i].id == ARM_INS_LDR && insn[i].detail->arm.op_count == 3 && 
            insn[i].detail->arm.operands[1].type == ARM_OP_MEM && insn[i].detail->arm.operands[1].mem.base == ARM_REG_SP && 
            insn[i].detail->arm.operands[2].type == ARM_OP_IMM) {
                spd += insn[i].detail->arm.operands[2].imm;
        }
        if ((insn[i].id == ARM_INS_SUB || insn[i].id == ARM_INS_ADD) && insn[i].detail->arm.op_count == 2 && insn[i].detail->arm.operands[0].type == ARM_OP_REG && insn[i].detail->arm.operands[0].reg == ARM_REG_SP) {
            if (insn[i].id == ARM_INS_SUB) {
                spd -= insn[i].detail->arm.operands[1].imm;
            } else {
                spd += insn[i].detail->arm.operands[1].imm;
            }
        }
    }
}

void calc_call(cs_insn *insn, size_t count, IntVec& calls)
{
    for (size_t i = 0; i < count; i++) {
        if (insn[i].id == ARM_INS_BL && insn[i].detail->arm.operands[0].type == ARM_OP_IMM) {
            calls.push_back(i);
        }
        if (insn[i].id == ARM_INS_BLX && insn[i].detail->arm.operands[0].type == ARM_OP_REG) {
            calls.push_back(i);
        }
        // tail jumpout
        if (insn[i].id == ARM_INS_B && i == count - 1) {
            calls.push_back(i);
        }
    }
}

size_t guess_func_size(csh handle, const uint8_t *code, size_t maxsize, uint32_t address)
{
    size_t funcsize = 0;
    cs_insn* insn;
    size_t count = cs_disasm(handle, code, maxsize, address, 0, &insn);
    bool endonpop = false;
    for (size_t i = 0; i < count; i++) {
        if (insn[i].id == ARM_INS_PUSH && endonpop == false) {
            for (uint8_t j = 0; j < insn[i].detail->arm.op_count; j++) {
                if (insn[i].detail->arm.operands[j].reg == ARM_REG_LR) {
                    endonpop = true;
                    break;
                }
            }
        }
        bool hitted = false;
        // quick n dirty
        if (insn[i].id == ARM_INS_POP && endonpop) {
            for (uint8_t j = 0; j < insn[i].detail->arm.op_count; j++) {
                if (insn[i].detail->arm.operands[j].reg == ARM_REG_PC) {
                    hitted = true;
                    break;
                }
            }
        }
        if (insn[i].id == ARM_INS_BLX && endonpop == false && insn[i].detail->arm.operands[0].reg == ARM_REG_LR) {
            hitted = true;
        }
        if (hitted) {
            funcsize = (uint32_t)insn[i].address + insn[i].size - address;
            break;
        }
    }
    cs_free(insn, count);
    return funcsize;
}
