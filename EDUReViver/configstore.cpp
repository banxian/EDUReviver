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
                char truefalse[0x71];
                patcher_config item = {0,};
                //"J-Link V10 compiled Feb  2 2018 18:12:40", 0x100840A0, 0x1A010718, 0x1A010EDA, false
                int cnt = sscanf(line, "\"%[^\"]\", 0x%X, 0x%X, 0x%X, %[^,], %d, 0x%X, 0x%X, 0x%X", version, &item.sp, &item.lr, &item.usbrx, truefalse, &item.cmdReg, &item.R4, &item.R5, &item.R6);
                if (cnt >= 5) {
                    truefalse[0x70] = 0;
                    version[0x70] = 0;
                    item.isSES = _stricmp(truefalse, "true") == 0;
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
            linelen = sprintf_s(line, _countof(line), "\"%s\", 0x%08X, 0x%08X, 0x%08X, true, %d, 0x%X, 0x%X, 0x%X", version.c_str(), config->sp, config->lr, config->usbrx, config->cmdReg, config->R4, config->R5, config->R6);
        } else {
            linelen = sprintf_s(line, _countof(line), "\"%s\", 0x%08X, 0x%08X, 0x%08X, false", version.c_str(), config->sp, config->lr, config->usbrx);
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

#define FREE_AND_EXIT(ptr) cs_free(insn, count); cs_close(&handle); return ptr;

bool is_reg_add_imm(cs_insn * insn, int reg)
{
    return insn->id == ARM_INS_ADD && 
        insn->detail->arm.operands[0].type == ARM_OP_REG && insn->detail->arm.operands[0].reg == reg &&
        insn->detail->arm.operands[1].type == ARM_OP_IMM;
}

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
        int retry = 0;
        uint32_t mainaddr = 0;
        while (retry++ < 20 && mainaddr == 0) {
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
            //printf("%"PRIx64"\t%s\t\t%s\n", insn[j].address, insn[j].mnemonic, insn[j].op_str);
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
        //dispatchcmd是taskmain的一部分
        //1A0197AC 000 98 B0                       SUB     SP, SP, #0x60
        //1A01B25A 060 80 47                       BLX     R0
        count = cs_disasm(handle, reader.buf_at_addr(maintaskaddr & ~1), 0x2000, (maintaskaddr & ~1), 0, &insn);
        IntVec spdvec;
        calc_stack(insn, count, spdvec);
        uint32_t cmdtable = 0;
        int cmdcnt = 0;
        int r4val = -1, r5val = -1, r6val = -1;
        int spd1, spd2;
        uint32_t dispatchlr;
        uint32_t usbrxbuf = 0;
        int rawCmdRegIdx = -1;
        for (size_t j = 0; j < count; j++) {
            //printf("%"PRIx64" %03x \t%s\t\t%s\n", insn[j].address, -spdvec[j], insn[j].mnemonic, insn[j].op_str);
            cs_arm* arm = &insn[j].detail->arm;
            // BLX R0
            if (j > 5 && insn[j].id == ARM_INS_BLX && arm->operands[0].type == ARM_OP_REG) {
                // LDR.W   R0, [R1,R0,LSL#2]
                cs_arm* prevarm = &insn[j-1].detail->arm;
                // index uxtb cmd
                // base cmdtable
                // shift 2
                if (insn[j-1].id == ARM_INS_LDR && prevarm->operands[0].reg == arm->operands[0].reg && 
                    prevarm->operands[1].type == ARM_OP_MEM && prevarm->operands[1].shift.type == ARM_SFT_LSL && prevarm->operands[1].shift.value == 2) {
                        dispatchlr = insn[j+1].address;
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
                                is_reg_add_imm(&insn[j-1], arm->operands[1].reg)) {
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
                        if (cmdcnt && cmdtable) {
                            break;
                        }
                }
                // MOVS    R1, #1
                // BLX     R2
                // CMP     R0, #0
                if (j > 10 && insn[j-1].id == ARM_INS_MOV && insn[j+1].id == ARM_INS_CMP) {
                    cs_arm* prevarm = &insn[j-1].detail->arm;
                    cs_arm* nextarm = &insn[j+1].detail->arm;
                    if (prevarm->operands[1].type == ARM_OP_IMM && prevarm->operands[1].imm == 1 && nextarm->operands[1].type == ARM_OP_IMM && nextarm->operands[1].imm == 0) {
                        int step = 0;
                        int targetreg = arm->operands[0].reg;
                        uint32_t usbrxtable, disp;
                        for (size_t k = j - 1; k > j - 15; k--) {
                            cs_arm* arm = &insn[k].detail->arm;
                            // LDR     R2, [R0,#8]
                            if (step == 0 && insn[k].id == ARM_INS_LDR && arm->operands[0].reg == targetreg && arm->operands[1].type == ARM_OP_MEM && arm->operands[1].mem.disp) {
                                step++;
                                targetreg = arm->operands[1].mem.base;
                                disp = arm->operands[1].mem.disp;
                            }
                            // MOV     R0, R1
                            if (step >= 1 && insn[k].id == ARM_INS_MOV  && arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg == targetreg && arm->operands[1].type == ARM_OP_REG) {
                                targetreg = arm->operands[1].reg;
                                step = 1;
                            }
                            if (step == 1 && insn[k].id == ARM_INS_MOVT && arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg == targetreg) {
                                *((uint16_t*)&usbrxtable + 1) = arm->operands[1].imm;
                                step++;
                            }
                            if (step == 2 && (insn[k].id == ARM_INS_MOV || insn[k].id == ARM_INS_MOVW) && arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg == targetreg && arm->operands[1].type == ARM_OP_IMM) {
                                *(uint16_t*)&usbrxtable = arm->operands[1].imm;
                                usbrxbuf = reader.read_uint32_addr(usbrxtable + disp) &~1;
                                printf("usbrxbuf: %08X\n", usbrxbuf);
                                break;
                            }
                        }
                    }
                }
            }
            // 持续跟踪R4/R5/R6的MOVW/MOVT对值
            if ((insn[j].id == ARM_INS_MOV || insn[j].id == ARM_INS_MOVW) && arm->operands[0].type == ARM_OP_REG && arm->operands[1].type == ARM_OP_IMM) {
                switch(arm->operands[0].reg) {
                case ARM_REG_R4:
                    r4val = arm->operands[1].imm;
                    break;
                case ARM_REG_R5:
                    r5val = arm->operands[1].imm;
                    break;
                case ARM_REG_R6:
                    r6val = arm->operands[1].imm;
                    break;
                }
            }
            if (insn[j].id == ARM_INS_MOVT && arm->operands[0].type == ARM_OP_REG && arm->operands[1].type == ARM_OP_IMM) {
                switch(arm->operands[0].reg) {
                case ARM_REG_R4:
                    *((uint16_t*)&r4val + 1) = arm->operands[1].imm;
                    break;
                case ARM_REG_R5:
                    *((uint16_t*)&r5val + 1) = arm->operands[1].imm;
                    break;
                case ARM_REG_R6:
                    *((uint16_t*)&r6val + 1) = arm->operands[1].imm;
                    break;
                }
            }
        }
        cs_free(insn, count);
        printf("R4: %08X R5:%08X R6:%08X\n", r4val, r5val, r6val);
        if (cmdcnt == 0) {
            errprintf("Analyst failed on command table!\n");
            cs_close(&handle);
            return NULL;
        }
        uint32_t cmde0func = reader.read_uint32_addr(cmdtable + (cmdcnt - 1 - (0xFF - 0xE0)) * 4);
        printf("cmd_e0_fine_write_read: %08X\n", cmde0func);
        // 寻找usbrxbuf崩溃点, 5C
        count = cs_disasm(handle, reader.buf_at_addr(cmde0func & ~1), 0x70, (cmde0func & ~1), 0, &insn);
        calc_stack(insn, count, spdvec);
        int callcount = 0;
        size_t firstBL = -1, firstBLX = -1, secondBLX = -1;
        int funcsreg = ARM_REG_INVALID;
        uint32_t currstack;
        for (size_t j = 0; j < count; j++) {
            //printf("%"PRIx64" %03x \t%s\t\t%s\n", insn[j].address, -spdvec[j], insn[j].mnemonic, insn[j].op_str);
            cs_arm* arm = &insn[j].detail->arm;
            // BLX R2
            if (insn[j].id == ARM_INS_BL && arm->operands[0].type == ARM_OP_IMM) {
                firstBL = j;
            }
            if (firstBL != -1 && insn[j].id == ARM_INS_MOVT) {
                funcsreg = arm->operands[0].reg;
            }
            if (insn[j].id == ARM_INS_BLX && arm->operands[0].type == ARM_OP_REG) {
                if (callcount) {
                    spd2 = spdvec[j];
                    secondBLX = j;
                    currstack = taskstackinit + spd1 + spd2;
                    printf("sp: %08X dispatch LR:%08X\n", currstack, dispatchlr);
                    break;
                } else {
                    firstBLX = j;
                    callcount++;
                }
            }
        }
        // 寻找指针源
        uint32_t usbdfuncs;
        for (size_t j = firstBL+1; j < firstBLX; j++) {
            cs_arm* arm = &insn[j].detail->arm;
            if ((insn[j].id == ARM_INS_MOV || insn[j].id == ARM_INS_MOVW) && arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg == funcsreg && arm->operands[1].type == ARM_OP_IMM) {
                usbdfuncs = arm->operands[1].imm;
            }
            if (insn[j].id == ARM_INS_MOVT && arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg == funcsreg && arm->operands[1].type == ARM_OP_IMM) {
                *((uint16_t*)&usbdfuncs + 1) = arm->operands[1].imm;
                printf("g_usbd_funcs: %08X\n", usbdfuncs);
                break;
            }
        }
        // 寻找最后的发送
        uint32_t usbtxbuf;
        for (size_t j = secondBLX+1; j < count; j++) {
            //printf("%"PRIx64" %03x \t%s\t\t%s\n", insn[j].address, -spdvec[j], insn[j].mnemonic, insn[j].op_str);
            if (insn[j].id == ARM_INS_BL && insn[j].detail->arm.operands[0].type == ARM_OP_IMM) {
                usbtxbuf = insn[j].detail->arm.operands[0].imm;
            }
            if (spdvec[j] == 0) {
                printf("usbtxbuf: %08X\n", usbtxbuf);
                break;
            }
        }
        cs_free(insn, count);
        // version, sp, lr, uxbrx, true, cmdReg, R4, R5, R6
        if (rawCmdRegIdx == 4) r4val = cmdcnt + 0xE0;
        if (rawCmdRegIdx == 5) r5val = cmdcnt + 0xE0;
        if (rawCmdRegIdx == 6) r6val = cmdcnt + 0xE0;
        //printf("\"%s\", 0x%08X, 0x%08X, 0x%08X, true, %d, 0x%X, 0x%X, 0x%X\n", reader.get_banner(), currstack, dispatchlr, usbrxbuf, rawCmdRegIdx, r4val, r5val, r6val);
        patcher_config item = {currstack, dispatchlr, usbrxbuf, true, rawCmdRegIdx, r4val, r5val, r6val};
        add_user_config(reader.get_banner(), &item);
    }
    // IAR固件?
    if (count == 4) {
        if (insn[2].id == ARM_INS_LDR && insn[2].detail->arm.operands[1].reg == ARM_REG_PC) {
            startaddr = extract_ldr_pool(&insn[2]);
            startaddr = reader.read_uint32_addr(startaddr);
            printf("__iar_program_start: %08X\n", startaddr);
        } else {
            printf("Firmware not build by EWARM\n");
            FREE_AND_EXIT(NULL);
        }
        cs_free(insn, count);
        count = cs_disasm(handle, reader.buf_at_addr((startaddr & ~1) + 8), 4, (startaddr & ~1) + 8, 0, &insn);
        if (count <= 0 || insn[0].id != ARM_INS_BL) {
            printf("Firmware not build by EWARM.\n");
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
        size_t seekend = 0;
        uint32_t maintaskaddr = -1, taskstackinit = -1;
        int step = 0;
        for (size_t j = 0; j < count; j++) {
            //printf("%"PRIx64" %03x \t%s\t\t%s\n", insn[j].address, -spdvec[j], insn[j].mnemonic, insn[j].op_str);
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
                    //printf("%"PRIx64" %03x \t%s\t\t%s\n", insn[j].address, -spdvec[j], insn[j].mnemonic, insn[j].op_str);
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
        uint32_t target;
        int spd1, spd2, spd3;
        for (size_t j = 0; j < count; j++) {
            //printf("%"PRIx64" %03x \t%s\t\t%s\n", insn[j].address, -spdvec[j], insn[j].mnemonic, insn[j].op_str);
            cs_arm* arm = &insn[j].detail->arm;
            // LDRB.W  R0, [SP,#8+cmd]
            if (step == 0 && insn[j].id == ARM_INS_LDRB && 
                arm->op_count == 2 && arm->operands[0].type == ARM_OP_REG && arm->operands[0].reg == ARM_REG_R0 &&
                arm->operands[1].type == ARM_OP_MEM && arm->operands[1].mem.base == ARM_REG_SP) {
                    seekend = j;
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
        // 寻找usbcmdtable
        count = cs_disasm(handle, reader.buf_at_addr(target & ~1), 0x120, (target & ~1), 0, &insn);
        calc_stack(insn, count, spdvec);
        uint32_t dispatchlr, cmdtable;
        int cmdcnt = 0;
        for (size_t j = 0; j < count; j++) {
            //printf("%"PRIx64" %03x \t%s\t\t%s\n", insn[j].address, -spdvec[j], insn[j].mnemonic, insn[j].op_str);
            cs_arm* arm = &insn[j].detail->arm;
            // BLX     R0
            if (insn[j].id == ARM_INS_BLX && arm->operands[0].type == ARM_OP_REG) {
                spd2 = spdvec[j];
                dispatchlr = insn[j].address + insn[j].size;
                step = 0;
                int reg = ARM_REG_INVALID;
                // 往回寻找ADD/LDR
                while (j--) {
                    //printf("%"PRIx64" %03x \t%s\t\t%s\n", insn[j].address, -spdvec[j], insn[j].mnemonic, insn[j].op_str);
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
        uint32_t cmde0func = reader.read_uint32_addr(cmdtable + (cmdcnt - 1 - (0xFF - 0xE0)) * 4);
        printf("cmd_e0_fine_write_read: %08X\n", cmde0func);
        // 寻找usbrxbuf崩溃点, 4c
        count = cs_disasm(handle, reader.buf_at_addr(cmde0func & ~1), 0x60, (cmde0func & ~1), 0, &insn);
        calc_stack(insn, count, spdvec);
        uint32_t usbrxbuf = -1;
        size_t prevj = -1;
        uint32_t currstack;
        for (size_t j = 0; j < count; j++) {
            //printf("%"PRIx64" %03x \t%s\t\t%s\n", insn[j].address, -spdvec[j], insn[j].mnemonic, insn[j].op_str);
            cs_arm* arm = &insn[j].detail->arm;
            // BL      app_usbrxbuf
            if (insn[j].id == ARM_INS_BL && arm->operands[0].type == ARM_OP_IMM) {
                if (arm->operands[0].imm == usbrxbuf) {
                    spd3 = spdvec[j];
                    currstack = taskstackinit + spd1 + spd2 + spd3;
                    printf("sp: %08X dispatch LR:%08X usbrxbuf: %08X\n", currstack, dispatchlr, usbrxbuf);
                    prevj = j;
                    break;
                } else {
                    usbrxbuf = arm->operands[0].imm;
                }
            }
        }
        // 寻找最后的发送
        uint32_t usbtxbuf;
        for (size_t j = prevj + 1; j < count; j++) {
            //printf("%"PRIx64" %03x \t%s\t\t%s\n", insn[j].address, -spdvec[j], insn[j].mnemonic, insn[j].op_str);
            if (insn[j].id == ARM_INS_BL && insn[j].detail->arm.operands[0].type == ARM_OP_IMM) {
                usbtxbuf = insn[j].detail->arm.operands[0].imm;
            }
            if (spdvec[j] == 0) {
                printf("usbtxbuf: %08X\n", usbtxbuf);
                break;
            }
        }
        cs_free(insn, count);
        // version, sp, lr, uxbrx, false
        //printf("\"%s\", 0x%08X, 0x%08X, 0x%08X, false\n", reader.get_banner(), currstack, dispatchlr, usbrxbuf);
        patcher_config item = {currstack, dispatchlr, usbrxbuf, false};
        add_user_config(reader.get_banner(), &item);
    }
    cs_close(&handle);

    return find_patcher_config(reader.get_banner());
}

void calc_stack(cs_insn *insn, size_t count, IntVec& vec)
{
    int spd = 0;
    vec.resize(count);
    for (size_t j = 0; j < count; j++) {
        vec[j] = spd;
        if (insn[j].id == ARM_INS_PUSH) {
            spd -= insn[j].detail->arm.op_count * 4;
        }
        if (insn[j].id == ARM_INS_POP) {
            spd += insn[j].detail->arm.op_count * 4;
        }
        // ldr lr,[sp],#4
        if (insn[j].id == ARM_INS_LDR && insn[j].detail->arm.op_count == 3 && 
            insn[j].detail->arm.operands[1].type == ARM_OP_MEM && insn[j].detail->arm.operands[1].mem.base == ARM_REG_SP && 
            insn[j].detail->arm.operands[2].type == ARM_OP_IMM) {
                spd += insn[j].detail->arm.operands[2].imm;
        }
        if ((insn[j].id == ARM_INS_SUB || insn[j].id == ARM_INS_ADD) && insn[j].detail->arm.op_count == 2 && insn[j].detail->arm.operands[0].type == ARM_OP_REG && insn[j].detail->arm.operands[0].reg == ARM_REG_SP) {
            if (insn[j].id == ARM_INS_SUB) {
                spd -= insn[j].detail->arm.operands[1].imm;
            } else {
                spd += insn[j].detail->arm.operands[1].imm;
            }
        }
    }
}

