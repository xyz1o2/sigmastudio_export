#ifndef __SIGMASTUDIOFW_OPTIMIZED_H__
#define __SIGMASTUDIOFW_OPTIMIZED_H__

// 标准类型定义 - 确保兼容性
#include <stdint.h>

#ifdef ARDUINO
#include <Arduino.h>
#else
// 非Arduino环境的基本定义
typedef uint8_t byte;
#define PROGMEM
#define pgm_read_byte_near(addr) (*(const unsigned char *)(addr))
#endif

#include "USER_SETTINGS.h"

#if USE_SPI
#ifdef ARDUINO
#include <SPI.h>
#endif
#else
#ifdef ARDUINO
#include <Wire.h>
#endif
#endif

// 数据类型定义
#define ADI_DATA_U16 const PROGMEM uint16_t
#define ADI_REG_TYPE const PROGMEM uint8_t

// 错误代码定义
#define SIGMA_SUCCESS           0
#define SIGMA_ERROR_I2C_TIMEOUT 1
#define SIGMA_ERROR_I2C_NACK    2
#define SIGMA_ERROR_I2C_DATA    3
#define SIGMA_ERROR_BUFFER_SIZE 4
#define SIGMA_ERROR_INVALID_PARAM 5

// 参数数据格式
#define SIGMASTUDIOTYPE_FIXPOINT 0
#define SIGMASTUDIOTYPE_INTEGER  1

// 配置参数
#ifndef MAX_I2C_DATA_LENGTH
  #define MAX_I2C_DATA_LENGTH 30  // 可根据硬件调整
#endif

#ifndef I2C_TIMEOUT_MS
  #define I2C_TIMEOUT_MS 1000     // I2C超时时间(毫秒)
#endif

#ifndef I2C_CLOCK_SPEED
  #define I2C_CLOCK_SPEED 400000  // I2C时钟频率
#endif

// 调试开关
#ifndef SIGMA_DEBUG
  #define SIGMA_DEBUG 0
#endif

#if SIGMA_DEBUG
  #define SIGMA_DEBUG_PRINT(x) Serial.print(x)
  #define SIGMA_DEBUG_PRINTLN(x) Serial.println(x)
#else
  #define SIGMA_DEBUG_PRINT(x)
  #define SIGMA_DEBUG_PRINTLN(x)
#endif

// 全局错误状态
static uint8_t g_sigma_last_error = SIGMA_SUCCESS;

// 获取最后一次错误代码
uint8_t SIGMA_GET_LAST_ERROR() {
    return g_sigma_last_error;
}

// 清除错误状态
void SIGMA_CLEAR_ERROR() {
    g_sigma_last_error = SIGMA_SUCCESS;
}

// 浮点值转换为SigmaDSP定点格式
int32_t SIGMASTUDIOTYPE_FIXPOINT_CONVERT(double value) {
#if DSP_TYPE == DSP_TYPE_SIGMA300_350
    return int32_t(value * (0x01 << 23));
#else
    return int32_t(value * (0x01 << 23)) & 0xFFFFFFF;
#endif
}

// 兼容性宏
#define SIGMASTUDIOTYPE_8_24_CONVERT(x) SIGMASTUDIOTYPE_FIXPOINT_CONVERT(x)

// 将32位值分离为四个字节
void SIGMASTUDIOTYPE_REGISTER_CONVERT(int32_t fixpt_val, byte dest[4]) {
    dest[0] = (fixpt_val >> 24) & 0xFF;
    dest[1] = (fixpt_val >> 16) & 0xFF;
    dest[2] = (fixpt_val >> 8) & 0xFF;
    dest[3] = (fixpt_val) & 0xFF;
}

// 获取DSP内存位置深度
#if USE_SPI == false
byte getMemoryDepth(uint32_t address) {
#if DSP_TYPE == DSP_TYPE_SIGMA100
    return (address < 0x0400) ? 4 : 5;
#elif DSP_TYPE == DSP_TYPE_SIGMA200
    return (address < 0x0800) ? 4 : 5;
#elif (DSP_TYPE == DSP_TYPE_SIGMA300_350)
    return (address < 0xF000) ? 4 : 2;
#else
    return 4; // 默认值
#endif
}
#endif

// I2C初始化函数
void SIGMA_I2C_INIT() {
#if USE_SPI == false
    Wire.begin();
    Wire.setClock(I2C_CLOCK_SPEED);
    
    #if defined(WIRE_HAS_TIMEOUT)
    Wire.setWireTimeout(I2C_TIMEOUT_MS * 1000, true); // 转换为微秒
    #endif
    
    SIGMA_DEBUG_PRINTLN("I2C initialized");
#endif
}

// 增强的I2C错误检查
uint8_t checkI2CError(uint8_t error) {
    switch(error) {
        case 0: return SIGMA_SUCCESS;
        case 1: g_sigma_last_error = SIGMA_ERROR_BUFFER_SIZE; break;
        case 2: g_sigma_last_error = SIGMA_ERROR_I2C_NACK; break;
        case 3: g_sigma_last_error = SIGMA_ERROR_I2C_DATA; break;
        case 4: g_sigma_last_error = SIGMA_ERROR_I2C_DATA; break;
        case 5: g_sigma_last_error = SIGMA_ERROR_I2C_TIMEOUT; break;
        default: g_sigma_last_error = SIGMA_ERROR_I2C_DATA; break;
    }
    
    #if SIGMA_DEBUG
    SIGMA_DEBUG_PRINT("I2C Error: ");
    SIGMA_DEBUG_PRINTLN(error);
    #endif
    
    return g_sigma_last_error;
}

// 核心寄存器块写入函数 - 优化版本
uint8_t SIGMA_WRITE_REGISTER_BLOCK_CORE(byte devAddress, uint16_t address, uint16_t length, const uint8_t* pData, uint16_t dataOffset = 0) {
    if (length == 0) {
        g_sigma_last_error = SIGMA_ERROR_INVALID_PARAM;
        return g_sigma_last_error;
    }

#if USE_SPI
    // SPI实现
    digitalWrite(DSP_SS_PIN, LOW);
    SPI.beginTransaction(settingsA);
    SPI.transfer(0x0);
    SPI.transfer(address >> 8);
    SPI.transfer(address & 0xff);
    
    for (uint16_t i = 0; i < length; i++) {
        uint8_t data = (pData) ? pgm_read_byte_near(pData + i + dataOffset) : 0;
        SPI.transfer(data);
    }
    
    SPI.endTransaction();
    digitalWrite(DSP_SS_PIN, HIGH);
    return SIGMA_SUCCESS;

#else
    // I2C实现 - 优化版本
    g_sigma_last_error = SIGMA_SUCCESS;
    
    // 检查是否可以一次性发送
    if (length <= MAX_I2C_DATA_LENGTH - 2) { // -2 for address bytes
        Wire.beginTransmission(devAddress);
        Wire.write(address >> 8);
        Wire.write(address & 0xff);
        
        for (uint16_t i = 0; i < length; i++) {
            uint8_t data = (pData) ? pgm_read_byte_near(pData + i + dataOffset) : 0;
            Wire.write(data);
        }
        
        uint8_t error = Wire.endTransmission();
        return checkI2CError(error);
    }
    
    // 分块传输 - 优化的算法
    uint16_t currentByte = 0;
    uint16_t currentAddr = address;
    
    while (currentByte < length) {
        Wire.beginTransmission(devAddress);
        Wire.write(currentAddr >> 8);
        Wire.write(currentAddr & 0xff);
        
        uint16_t bytesInThisTransaction = 0;
        uint8_t memDepth = getMemoryDepth(currentAddr);
        
        // 计算这次传输可以发送多少字节
        while ((bytesInThisTransaction + memDepth <= MAX_I2C_DATA_LENGTH) && 
               (currentByte < length)) {
            
            for (uint8_t i = 0; i < memDepth && currentByte < length; i++) {
                uint8_t data = (pData) ? pgm_read_byte_near(pData + currentByte + dataOffset) : 0;
                Wire.write(data);
                currentByte++;
                bytesInThisTransaction++;
            }
            currentAddr++;
        }
        
        uint8_t error = Wire.endTransmission();
        if (checkI2CError(error) != SIGMA_SUCCESS) {
            return g_sigma_last_error;
        }
    }
    
    return SIGMA_SUCCESS;
#endif
}

// 公共接口函数 - 简化重载
uint8_t SIGMA_WRITE_REGISTER_BLOCK(byte devAddress, uint16_t address, uint16_t length, byte pData[]) {
    return SIGMA_WRITE_REGISTER_BLOCK_CORE(devAddress, address, length, (const uint8_t*)pData);
}

uint8_t SIGMA_WRITE_REGISTER_BLOCK(byte devAddress, uint16_t address, uint16_t length, const uint8_t pData[], uint16_t addrOffset = 0) {
    return SIGMA_WRITE_REGISTER_BLOCK_CORE(devAddress, address, length, pData, addrOffset);
}

uint8_t SIGMA_WRITE_REGISTER_BLOCK(uint16_t address, uint16_t length, byte pData[]) {
    return SIGMA_WRITE_REGISTER_BLOCK(DSP_I2C_ADDR, address, length, pData);
}

// 整数和浮点数写入函数
uint8_t SIGMA_WRITE_REGISTER_INTEGER(uint16_t address, int32_t pData) {
    byte byte_data[4];
    SIGMASTUDIOTYPE_REGISTER_CONVERT(pData, byte_data);
    return SIGMA_WRITE_REGISTER_BLOCK(address, 4, byte_data);
}

uint8_t SIGMA_WRITE_REGISTER_FLOAT(uint16_t address, double pData) {
    return SIGMA_WRITE_REGISTER_INTEGER(address, SIGMASTUDIOTYPE_FIXPOINT_CONVERT(pData));
}

// 延迟函数 - 优化版本
uint8_t SIGMA_WRITE_DELAY(byte devAddress, uint16_t length, const uint8_t pData[]) {
    if (length > 4) {
        g_sigma_last_error = SIGMA_ERROR_INVALID_PARAM;
        return g_sigma_last_error;
    }
    
    uint32_t delay_length = 0;
    for (int8_t i = length - 1; i >= 0; i--) {
        delay_length = (delay_length << 8) + pgm_read_byte_near(pData + i);
    }
    
    SIGMA_DEBUG_PRINT("Delay: ");
    SIGMA_DEBUG_PRINTLN(delay_length);
    
    delay(delay_length);
    return SIGMA_SUCCESS;
}

uint8_t SIGMA_WRITE_DELAY(byte devAddress, uint16_t length, byte pData[]) {
    return SIGMA_WRITE_DELAY(devAddress, length, (const uint8_t*)pData);
}

// SAFELOAD 宏定义
#ifndef SAFELOAD_DATA_ADDR
  #ifdef MOD_SAFELOADMODULE_DATA_SAFELOAD0_ADDR
    #define SAFELOAD_DATA_ADDR MOD_SAFELOADMODULE_DATA_SAFELOAD0_ADDR
  #else
    #define SAFELOAD_DATA_ADDR 24576
  #endif
#endif

#ifndef SAFELOAD_ADDR_ADDR
  #ifdef MOD_SAFELOADMODULE_ADDRESS_SAFELOAD_ADDR
    #define SAFELOAD_ADDR_ADDR MOD_SAFELOADMODULE_ADDRESS_SAFELOAD_ADDR
  #else
    #define SAFELOAD_ADDR_ADDR 24581
  #endif
#endif

#ifndef SAFELOAD_SLOTS_ADDR
  #ifdef MOD_SAFELOADMODULE_NUM_SAFELOAD_ADDR
    #define SAFELOAD_SLOTS_ADDR MOD_SAFELOADMODULE_NUM_SAFELOAD_ADDR
  #else
    #define SAFELOAD_SLOTS_ADDR 24582
  #endif
#endif

#ifndef SAFELOAD_SLOTS_DATA_1
  static const uint8_t SAFELOAD_SLOTS_DATA_1[] PROGMEM = {1, 1, 1, 1, 1};
#endif

// SAFELOAD写入函数
uint8_t SIGMA_WRITE_SAFELOAD_REGISTER_BLOCK(uint16_t address, uint16_t length, uint8_t pData[]) {
    uint8_t result = SIGMA_WRITE_REGISTER_BLOCK(DSP_I2C_ADDR, SAFELOAD_DATA_ADDR, length, pData);
    if (result != SIGMA_SUCCESS) return result;
    
    result = SIGMA_WRITE_REGISTER_INTEGER(SAFELOAD_ADDR_ADDR, address);
    if (result != SIGMA_SUCCESS) return result;
    
    return SIGMA_WRITE_REGISTER_BLOCK(DSP_I2C_ADDR, SAFELOAD_SLOTS_ADDR, length, (uint8_t*)SAFELOAD_SLOTS_DATA_1);
}

uint8_t SIGMA_WRITE_SAFELOAD_REGISTER_BLOCK(uint16_t address, uint16_t length, const uint8_t pData[], uint16_t addrOffset = 0) {
    uint8_t result = SIGMA_WRITE_REGISTER_BLOCK(DSP_I2C_ADDR, SAFELOAD_DATA_ADDR, length, pData, addrOffset);
    if (result != SIGMA_SUCCESS) return result;
    
    result = SIGMA_WRITE_REGISTER_INTEGER(SAFELOAD_ADDR_ADDR, address);
    if (result != SIGMA_SUCCESS) return result;
    
    return SIGMA_WRITE_REGISTER_BLOCK(DSP_I2C_ADDR, SAFELOAD_SLOTS_ADDR, length, (uint8_t*)SAFELOAD_SLOTS_DATA_1);
}

// 读取函数 - 增强错误处理
uint8_t SIGMA_READ_REGISTER_BYTES(uint16_t address, uint16_t length, byte* pData) {
    if (!pData || length == 0) {
        g_sigma_last_error = SIGMA_ERROR_INVALID_PARAM;
        return g_sigma_last_error;
    }

#if USE_SPI
    digitalWrite(DSP_SS_PIN, LOW);
    SPI.beginTransaction(settingsA);
    SPI.transfer(0x1);
    SPI.transfer(address >> 8);
    SPI.transfer(address & 0xff);
    
    for (uint16_t i = 0; i < length; i++) {
        pData[i] = SPI.transfer(0);
    }
    
    SPI.endTransaction();
    digitalWrite(DSP_SS_PIN, HIGH);
    return SIGMA_SUCCESS;

#else
    Wire.beginTransmission(DSP_I2C_ADDR);
    Wire.write(address >> 8);
    Wire.write(address & 0xff);
    uint8_t error = Wire.endTransmission(false);
    
    if (checkI2CError(error) != SIGMA_SUCCESS) {
        return g_sigma_last_error;
    }
    
    uint8_t received = Wire.requestFrom(DSP_I2C_ADDR, (uint8_t)length);
    if (received != length) {
        g_sigma_last_error = SIGMA_ERROR_I2C_DATA;
        return g_sigma_last_error;
    }
    
    for (uint16_t i = 0; i < length; i++) {
        if (Wire.available()) {
            pData[i] = Wire.read();
        } else {
            g_sigma_last_error = SIGMA_ERROR_I2C_DATA;
            return g_sigma_last_error;
        }
    }
    
    return SIGMA_SUCCESS;
#endif
}

// 读取整数值
int32_t SIGMA_READ_REGISTER_INTEGER(uint16_t address, uint8_t length) {
    if (length > 4) {
        g_sigma_last_error = SIGMA_ERROR_INVALID_PARAM;
        return 0;
    }
    
    byte register_value[4] = {0};
    if (SIGMA_READ_REGISTER_BYTES(address, length, register_value) != SIGMA_SUCCESS) {
        return 0;
    }
    
    int32_t result = 0;
    for (uint8_t i = 0; i < length; i++) {
        result = (result << 8) + register_value[i];
    }
    return result;
}

// 读取浮点值
double SIGMA_READ_REGISTER_FLOAT(uint16_t address) {
    int32_t integer_val = SIGMA_READ_REGISTER_INTEGER(address, 4);
    if (g_sigma_last_error != SIGMA_SUCCESS) {
        return 0.0;
    }
    
#if DSP_TYPE == DSP_TYPE_SIGMA300_350
    return double(integer_val) / (1 << 23);
#else
    return double(integer_val) / (1 << 23);
#endif
}

// 增强的调试打印函数
void SIGMA_PRINT_REGISTER(uint16_t address, uint8_t dataLength) {
    if (dataLength > 16) dataLength = 16; // 限制最大长度
    
    Serial.print("REG[0x");
    Serial.print(address, HEX);
    Serial.print("]: ");
    
    byte register_value[16] = {0};
    if (SIGMA_READ_REGISTER_BYTES(address, dataLength, register_value) == SIGMA_SUCCESS) {
        Serial.print("0x");
        for (uint8_t i = 0; i < dataLength; i++) {
            if (register_value[i] < 16) Serial.print('0');
            Serial.print(register_value[i], HEX);
            if (i < dataLength - 1) Serial.print(" ");
        }
        Serial.println();
    } else {
        Serial.print("ERROR ");
        Serial.println(g_sigma_last_error);
    }
}

// 打印错误信息
void SIGMA_PRINT_ERROR() {
    Serial.print("SIGMA Error: ");
    switch(g_sigma_last_error) {
        case SIGMA_SUCCESS: Serial.println("SUCCESS"); break;
        case SIGMA_ERROR_I2C_TIMEOUT: Serial.println("I2C_TIMEOUT"); break;
        case SIGMA_ERROR_I2C_NACK: Serial.println("I2C_NACK"); break;
        case SIGMA_ERROR_I2C_DATA: Serial.println("I2C_DATA"); break;
        case SIGMA_ERROR_BUFFER_SIZE: Serial.println("BUFFER_SIZE"); break;
        case SIGMA_ERROR_INVALID_PARAM: Serial.println("INVALID_PARAM"); break;
        default: Serial.println("UNKNOWN"); break;
    }
}

// 兼容性宏
#define SIGMASTUDIOTYPE_INTEGER_CONVERT(_value) (_value)

// 向后兼容的宏定义
#define SIGMA_WRITE_REGISTER(devAddress, address, dataLength, data) \
    SIGMA_WRITE_REGISTER_BLOCK(devAddress, address, dataLength, data)

#define SIGMA_READ_REGISTER(devAddress, address, length, pData) \
    SIGMA_READ_REGISTER_BYTES(address, length, pData)

#endif // __SIGMASTUDIOFW_OPTIMIZED_H__