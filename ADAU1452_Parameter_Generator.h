/*
 * ADAU1452 Parameter Generator - 受MCUdude/SigmaDSP启发
 * 
 * 这个头文件提供了一个ADAU1452专用的参数管理系统，
 * 借鉴了MCUdude/SigmaDSP项目的优秀设计理念，
 * 但专门针对ADAU1452进行了适配。
 * 
 * 主要功能：
 * - 自动化SigmaStudio参数提取
 * - 统一的参数地址管理
 * - 结构化的EQ模块定义
 * - 兼容SigmaStudio导出格式
 * 
 * 作者: The Augster
 * 参考: https://github.com/MCUdude/SigmaDSP
 * 适配: ADAU1452 DSP处理器
 */

#ifndef ADAU1452_PARAMETER_GENERATOR_H
#define ADAU1452_PARAMETER_GENERATOR_H

#include <stdint.h>
#include <stddef.h>
#include "ADAU1452_EN_B_I2C_IC_1_PARAM.h"

// Arduino类型兼容性
#ifndef byte
#define byte uint8_t
#endif

// ===== ADAU1452参数生成器配置 =====

// SigmaStudio项目信息
#define SIGMASTUDIO_PROJECT_NAME "ADAU1452_EN_B_I2C"
#define DSP_IC_NAME "IC_1"
#define EEPROM_IC_NAME "IC_2"

// 支持的EQ模块数量
#define MAX_EQ_MODULES 1  // 只支持第一个EQ模块，因为只有ALG1的宏定义存在
#define EQ_BANDS_PER_MODULE 10
#define EQ_COEFF_SIZE 20

// ===== 参数类型定义 =====

// DSP参数类型枚举
typedef enum {
    PARAM_TYPE_VOLUME,
    PARAM_TYPE_MUTE,
    PARAM_TYPE_EQ_MAIN_SWITCH,
    PARAM_TYPE_EQ_BAND_COEFF,
    PARAM_TYPE_BALANCE,
    PARAM_TYPE_CROSSOVER,
    PARAM_TYPE_COMPRESSOR,
    PARAM_TYPE_UNKNOWN
} DSPParameterType;

// 参数信息结构体
typedef struct {
    const char* name;           // 参数名称
    uint16_t address;          // 参数地址
    uint8_t size;              // 参数大小（字节）
    DSPParameterType type;     // 参数类型
    const char* description;   // 参数描述
} DSPParameter;

// EQ频段信息结构体
typedef struct {
    uint8_t bandNumber;        // 频段编号 (1-10)
    uint16_t coeffAddress;     // 系数地址
    const char* frequency;     // 中心频率描述
    byte defaultCoeff[EQ_COEFF_SIZE]; // 默认系数
} EQBandInfo;

// EQ模块信息结构体
typedef struct {
    const char* moduleName;    // 模块名称
    uint8_t moduleIndex;       // 模块索引 (0-2)
    uint16_t mainSwitchAddr;   // 主开关地址
    EQBandInfo bands[EQ_BANDS_PER_MODULE]; // 频段信息
    bool isActive;             // 是否激活
} EQModuleInfo;

// ===== 自动生成的参数映射表 =====

// 基于SigmaStudio PARAM.h文件自动提取的参数定义
static const DSPParameter dspParameters[] = {
    // 音量控制参数
    {"MULTIPLE1_2_TARGET", MOD_MULTIPLE1_2_ALG0_TARGET_ADDR, 4, PARAM_TYPE_VOLUME, "Channel 1A Volume"},
    {"MULTIPLE1_3_TARGET", MOD_MULTIPLE1_3_ALG0_TARGET_ADDR, 4, PARAM_TYPE_VOLUME, "Channel 1B Volume"},
    {"MULTIPLE1_4_TARGET", MOD_MULTIPLE1_4_ALG0_TARGET_ADDR, 4, PARAM_TYPE_VOLUME, "Channel 1C Volume"},
    
    // EQ主开关参数
    {"EQ_ALG0_SLEWMODE", MOD_EQ_ALG0_SLEWMODE_ADDR, 4, PARAM_TYPE_EQ_MAIN_SWITCH, "EQ Module 1 Main Switch"},
    {"EQ_2_ALG0_SLEWMODE", MOD_EQ_2_ALG0_SLEWMODE_ADDR, 4, PARAM_TYPE_EQ_MAIN_SWITCH, "EQ Module 2 Main Switch"},
    {"EQ_3_ALG0_SLEWMODE", MOD_EQ_3_ALG0_SLEWMODE_ADDR, 4, PARAM_TYPE_EQ_MAIN_SWITCH, "EQ Module 3 Main Switch"},
    
    // EQ频段系数参数 (使用正确的宏名称)
    {"EQ_ALG1_B2_1", MOD_EQ_ALG0_EQS300MULTISPHWSLEWP1ALG1TARGB251_ADDR, 20, PARAM_TYPE_EQ_BAND_COEFF, "EQ Module 1 Band 1 Coefficients"},
    {"EQ_ALG1_B2_10", MOD_EQ_ALG0_EQS300MULTISPHWSLEWP1ALG1TARGB210_ADDR, 20, PARAM_TYPE_EQ_BAND_COEFF, "EQ Module 1 Band 10 Coefficients"},
    
    // 结束标记
    {nullptr, 0, 0, PARAM_TYPE_UNKNOWN, nullptr}
};

// ===== EQ模块配置（基于SigmaStudio项目自动生成）=====

static EQModuleInfo eqModules[MAX_EQ_MODULES] = {
    // EQ模块1 (ALG0) - 主要EQ模块
    {
        "EQ_ALG0",
        0,
        MOD_EQ_ALG0_SLEWMODE_ADDR,
        {
            {1, MOD_EQ_ALG0_EQS300MULTISPHWSLEWP1ALG1TARGB251_ADDR, "31Hz", {0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
            {2, MOD_EQ_ALG0_EQS300MULTISPHWSLEWP1ALG1TARGB252_ADDR, "62Hz", {0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
            {3, MOD_EQ_ALG0_EQS300MULTISPHWSLEWP1ALG1TARGB260_ADDR, "125Hz", {0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
            {4, MOD_EQ_ALG0_EQS300MULTISPHWSLEWP1ALG1TARGB270_ADDR, "250Hz", {0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
            {5, MOD_EQ_ALG0_EQS300MULTISPHWSLEWP1ALG1TARGB280_ADDR, "500Hz", {0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
            {6, MOD_EQ_ALG0_EQS300MULTISPHWSLEWP1ALG1TARGB290_ADDR, "1kHz", {0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
            {7, MOD_EQ_ALG0_EQS300MULTISPHWSLEWP1ALG1TARGB2100_ADDR, "2kHz", {0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
            {8, MOD_EQ_ALG0_EQS300MULTISPHWSLEWP1ALG1TARGB230_ADDR, "4kHz", {0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
            {9, MOD_EQ_ALG0_EQS300MULTISPHWSLEWP1ALG1TARGB240_ADDR, "8kHz", {0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
            {10, MOD_EQ_ALG0_EQS300MULTISPHWSLEWP1ALG1TARGB210_ADDR, "16kHz", {0x00, 0x3D, 0x8B, 0x64, 0x00, 0x9E, 0xC5, 0xB2, 0x01, 0x00, 0x00, 0x00, 0xFF, 0xC2, 0x74, 0x9C, 0xFF, 0x61, 0x3A, 0x4E}}
        },
        true
    }
};

// ===== 参数管理函数声明 =====

// 参数查找函数
const DSPParameter* findParameterByName(const char* name);
const DSPParameter* findParameterByAddress(uint16_t address);
const DSPParameter* findParametersByType(DSPParameterType type, uint8_t* count);

// EQ模块管理函数
EQModuleInfo* getEQModule(uint8_t moduleIndex);
EQBandInfo* getEQBand(uint8_t moduleIndex, uint8_t bandNumber);
uint8_t getActiveEQModuleCount();

// 参数验证函数
bool isValidParameterAddress(uint16_t address);
bool isValidEQModule(uint8_t moduleIndex);
bool isValidEQBand(uint8_t bandNumber);

// 调试和信息函数
void printParameterMap();
void printEQModuleInfo(uint8_t moduleIndex);
void printAllEQModules();

// ===== 内联辅助函数 =====

// 获取参数数量
inline uint16_t getParameterCount() {
    uint16_t count = 0;
    while (dspParameters[count].name != NULL) count++;
    return count;
}

// 获取EQ模块数量
inline uint8_t getEQModuleCount() {
    return MAX_EQ_MODULES;
}

// 检查EQ模块是否激活
inline bool isEQModuleActive(uint8_t moduleIndex) {
    return (moduleIndex < MAX_EQ_MODULES) ? eqModules[moduleIndex].isActive : false;
}

// ===== 兼容性宏定义 =====

// 为了保持与现有代码的兼容性
#define CURRENT_EQ_MODULE 0
#define getCurrentEQModule() (&eqModules[CURRENT_EQ_MODULE])
#define getCurrentEQBandAddress(band) (eqModules[CURRENT_EQ_MODULE].bands[(band)-1].coeffAddress)
#define getCurrentEQMainSwitch() (eqModules[CURRENT_EQ_MODULE].mainSwitchAddr)

#endif // ADAU1452_PARAMETER_GENERATOR_H