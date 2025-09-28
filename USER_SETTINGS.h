#ifndef __USER_SETTINGS_H__
#define __USER_SETTINGS_H__

#define DSP_TYPE_SIGMA100     1    // Sigma100: ADAU1701/ADAU1702/ADAU1401
#define DSP_TYPE_SIGMA200     2    // Sigma200: ADAU176x/ADAU178x/ADAU144x
#define DSP_TYPE_SIGMA300_350 3    // Sigma300/Sigma350: ADAU145x, ADAU146x

// 设置DSP类型为ADAU1452 (Sigma300/350系列)
#define DSP_TYPE DSP_TYPE_SIGMA300_350

// 通信接口选择
#define USE_SPI false

// 时钟速度设置
#define SPI_SPEED 1000000L
#define I2C_SPEED 100000L  // 降低到 100kHz

#if USE_SPI
#include <SPI.h>
const SPISettings settingsA(SPI_SPEED, MSBFIRST, SPI_MODE3);
#endif

// 引脚定义
#define DSP_RESET_PIN 9
#define DSP_SS_PIN    10

// I2C地址覆盖 (用于SigmaStudio导出的SPI项目通过I2C编程)
#define OVERRIDE_SIGMASTUDIO_DEVICE_ADDRESS true
#if OVERRIDE_SIGMASTUDIO_DEVICE_ADDRESS
#if DSP_TYPE == DSP_TYPE_SIGMA100
const int DSP_I2C_ADDR = 0b0110100;    // ADAU1701
#elif DSP_TYPE == DSP_TYPE_SIGMA200
const int DSP_I2C_ADDR = 0b0111000;    // ADAU1761
#elif DSP_TYPE == DSP_TYPE_SIGMA300_350
const int DSP_I2C_ADDR = 0x3B;    // ADAU1452 (实际扫描到的地址)
#endif
#endif

#endif // __USER_SETTINGS_H__