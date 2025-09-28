#ifndef __SIGMASTUDIOFW_H__
#define __SIGMASTUDIOFW_H__
#include <Arduino.h>
#include "USER_SETTINGS.h"

#if USE_SPI
#include <SPI.h>
#else
#include <Wire.h>
#endif

#define ADI_DATA_U16 const PROGMEM uint16_t
#define ADI_REG_TYPE const PROGMEM uint8_t

// ========== 优化功能：错误处理系统 ==========
// 错误代码定义
#define SIGMA_SUCCESS           0
#define SIGMA_ERROR_I2C_TIMEOUT 1
#define SIGMA_ERROR_I2C_NACK    2
#define SIGMA_ERROR_I2C_DATA    3
#define SIGMA_ERROR_BUFFER_SIZE 4
#define SIGMA_ERROR_INVALID_PARAM 5

// 配置参数
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
  #define SIGMA_DEBUG_PRINT(x) Serial0.print(x)
  #define SIGMA_DEBUG_PRINT_HEX(x) Serial0.print(x, HEX)
  #define SIGMA_DEBUG_PRINTLN(x) Serial0.println(x)
#else
  #define SIGMA_DEBUG_PRINT(x)
  #define SIGMA_DEBUG_PRINT_HEX(x)
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

// 打印错误信息
void SIGMA_PRINT_ERROR() {
    Serial0.print("SIGMA Error: ");
    switch(g_sigma_last_error) {
        case SIGMA_SUCCESS: Serial0.println("SUCCESS"); break;
        case SIGMA_ERROR_I2C_TIMEOUT: Serial0.println("I2C_TIMEOUT"); break;
        case SIGMA_ERROR_I2C_NACK: Serial0.println("I2C_NACK"); break;
        case SIGMA_ERROR_I2C_DATA: Serial0.println("I2C_DATA"); break;
        case SIGMA_ERROR_BUFFER_SIZE: Serial0.println("BUFFER_SIZE"); break;
        case SIGMA_ERROR_INVALID_PARAM: Serial0.println("INVALID_PARAM"); break;
        default: Serial0.println("UNKNOWN"); break;
    }
}
// ========== 优化功能结束 ==========

/*
 * 参数数据格式
 */
#define SIGMASTUDIOTYPE_FIXPOINT 0
#define SIGMASTUDIOTYPE_INTEGER  1

 /*
  * 将浮点值转换为SigmaDSP (5.23或8.24) 定点格式
  */
#if DSP_TYPE == DSP_TYPE_SIGMA300_350
int32_t SIGMASTUDIOTYPE_FIXPOINT_CONVERT(double value) { return int32_t(value * (0x01 << 23)); }
#else
int32_t SIGMASTUDIOTYPE_FIXPOINT_CONVERT(double value) { return int32_t(value * (0x01 << 23)) & 0xFFFFFFF; }
#endif

// 为了与某些导出文件兼容，将SIGMASTUDIOTYPE_8_24_CONVERT重定向到
// SIGMASTUDIOTYPE_FIXPOINT_CONVERT
#define SIGMASTUDIOTYPE_8_24_CONVERT(x) SIGMASTUDIOTYPE_FIXPOINT_CONVERT(x)

// 将32位浮点值分离为四个字节
void SIGMASTUDIOTYPE_REGISTER_CONVERT(int32_t fixpt_val, byte dest[4]) {
    dest[0] = (fixpt_val >> 24) & 0xFF;
    dest[1] = (fixpt_val >> 16) & 0xFF;
    dest[2] = (fixpt_val >> 8) & 0xFF;
    dest[3] = (fixpt_val) & 0xFF;
}

// Arduino声明的I2C缓冲区默认长度为32字节。请根据您的处理器进行调整。
// 更长的缓冲区会使用更多的微控制器RAM，但允许更快的编程
// 因为I2C开销更低。
// 两个地址字节将数据突发大小缩短2字节。
const int MAX_I2C_DATA_LENGTH = 30;

/** 返回某个DSP内存位置的深度（以字节为单位）。
 * 目前此函数仅针对数据内存和程序内存实现。
 * 不包括控制寄存器。
 * 此函数仅在I2C时需要；它的存在是因为Teensy I2C库中的缓冲区大小限制。
 */


#if USE_SPI == false
byte getMemoryDepth(uint32_t address) {
#if DSP_TYPE == DSP_TYPE_SIGMA100
    if (address < 0x0400)
        return 4;    // 参数RAM深度为4字节
    else {
        return 5;    // 程序RAM深度为5字节
    }
#elif DSP_TYPE == DSP_TYPE_SIGMA200
    // Based on ADAU1761
    if (address < 0x0800) {
        return 4;    // 参数RAM深度为4字节
    }
    else {
        return 5;
    }
#elif (DSP_TYPE == DSP_TYPE_SIGMA300_350)
    if (address < 0xF000) {
        return 4;    // 程序内存、DM0和DM1都存储4字节（ADAU1463数据手册
                     // 第90页）
    }
    else {
        return 2;    // 控制寄存器都存储2字节（ADAU1463数据手册第93页）
    }
#else
    return 0;    // 我们永远不应该到达这个返回值
#endif
}
#endif


// ========== 优化的SIGMA_WRITE_REGISTER_BLOCK函数 ==========
uint8_t SIGMA_WRITE_REGISTER_BLOCK(byte devAddress, int address, int length, byte pData[]) {
    // 参数验证
    if (length <= 0 || !pData) {
        g_sigma_last_error = SIGMA_ERROR_INVALID_PARAM;
        return g_sigma_last_error;
    }
    
    g_sigma_last_error = SIGMA_SUCCESS;
    
#if USE_SPI    // SPI写入函数开始
    digitalWrite(DSP_SS_PIN, LOW);
    SPI.beginTransaction(settingsA);
    SPI.transfer(0x0);
    SPI.transfer(address >> 8);
    SPI.transfer(address & 0xff);
    for (int i = 0; i < length; i++) {
        SPI.transfer(pData[i]);
    }
    SPI.endTransaction();
    digitalWrite(DSP_SS_PIN, HIGH);
    return SIGMA_SUCCESS;
    
#else     // I2C写入函数开始 - 优化版本
    // 如果数据可以放入一个I2C缓冲区，则一次性发送
    if (length < MAX_I2C_DATA_LENGTH) {
        Wire.beginTransmission(DSP_I2C_ADDR);
        Wire.write(address >> 8);
        Wire.write(address & 0xff);
        Wire.write(pData, length);
        uint8_t error = Wire.endTransmission();
        return checkI2CError(error);
    }
    else {
        // 分块传输 - 优化版本
        int currentByte = 0;
        int currentAddr = address;

        while (currentByte < length) {
            Wire.beginTransmission(DSP_I2C_ADDR);
            Wire.write(currentAddr >> 8);
            Wire.write(currentAddr & 0xff);

            int bytesTransmitted = 0;
            while ((bytesTransmitted + getMemoryDepth(uint32_t(currentAddr)) <= MAX_I2C_DATA_LENGTH) &&
                (currentByte < length)) {
                for (byte i = 0; i < getMemoryDepth(uint32_t(currentAddr)) && currentByte < length; i++) {
                    Wire.write(pData[currentByte++]);
                    bytesTransmitted++;
                }
                currentAddr++;
            }
            
            uint8_t error = Wire.endTransmission();
            if (checkI2CError(error) != SIGMA_SUCCESS) {
                return g_sigma_last_error;
            }
        }
        return SIGMA_SUCCESS;
    }
#endif
}


// 用于progmem类型
void SIGMA_WRITE_REGISTER_BLOCK(byte devAddress, int address, int length, const uint8_t pData[], int addrOffset) {
#if USE_SPI    // SPI写入函数开始
    // 断言SPI从设备选择线
    digitalWrite(DSP_SS_PIN, LOW);        // 断言SPI从设备选择线（低电平有效）
    SPI.beginTransaction(settingsA);      // 初始化SPI
    SPI.transfer(0x0);                    // SPI读取地址 + 读/!写位
    SPI.transfer(address >> 8);           // 地址高字节
    SPI.transfer(address & 0xff);         // 地址低字节
    for (int i = 0; i < length; i++) {    // 对于数据包中的每个数据字节...
        SPI.transfer(pgm_read_byte_near(pData+i+addrOffset));           // 将数据字节写入DSP
    }
    SPI.endTransaction();              // 释放SPI总线
    digitalWrite(DSP_SS_PIN, HIGH);    // 拉高从设备选择线
// SPI写入函数结束
#else     // I2C写入函数开始


     // 如果数据可以放入一个I2C缓冲区，则一次性发送。
    if (length < MAX_I2C_DATA_LENGTH) {
        Wire.beginTransmission(DSP_I2C_ADDR);    // 初始化新的I2C传输
        Wire.write(address >> 8);                // 将地址高字节添加到I2C缓冲区
        Wire.write(address & 0xff);              // 将地址低字节添加到I2C缓冲区
        for (int i=0;i<length;i++){
            Wire.write(pgm_read_byte_near(pData+i+addrOffset));               // 将整个数据包添加到I2C缓冲区
        }
        Wire.endTransmission();                  // 将整个I2C传输发送到DSP
    }
    else {
        // 逐字节进行传输。
        int currentByte = 0;

        while (currentByte < length) {
            // 开始新的I2C事务。
            Wire.beginTransmission(DSP_I2C_ADDR);
            Wire.write(address >> 8);
            Wire.write(address & 0xff);
            
            int bytesTransmitted = 0;
            // 对于每个字节，找出它是否适合当前事务，并
            // 确保仍有数据要发送。
            while ((bytesTransmitted + getMemoryDepth(uint32_t(address)) <= MAX_I2C_DATA_LENGTH) &&
                (currentByte < length)) {
                // 如果另一个寄存器适合当前I2C突发写入，
                // 逐个将每个字节添加到I2C缓冲区。
                for (byte i = 0; i < getMemoryDepth(uint32_t(address)); i++) {
                    Wire.write(pgm_read_byte_near(pData+currentByte+addrOffset)); 
                    currentByte++;
                    bytesTransmitted++;
                }
                // 每写入一个寄存器增加一次地址。
                address++;
            }
            Wire.endTransmission();    // 现在缓冲区已满，发送I2C突发。
            if (currentByte == length) {
                break;
            }
        }
    }
#endif
}

void SIGMA_WRITE_REGISTER_BLOCK(byte devAddress, int address, int length, const uint8_t pData[]) {
  SIGMA_WRITE_REGISTER_BLOCK(devAddress, address, length, pData, 0);
}

// 不带地址的替代函数调用（单DSP系统）
void SIGMA_WRITE_REGISTER_BLOCK(int address, int length, byte pData[]) {
    SIGMA_WRITE_REGISTER_BLOCK(DSP_I2C_ADDR, address, length, pData);
}



// ========== 优化的整数和浮点写入函数 ==========
uint8_t SIGMA_WRITE_REGISTER_INTEGER(int address, int32_t pData) {
    byte byte_data[4];
    SIGMASTUDIOTYPE_REGISTER_CONVERT(pData, byte_data);
    return SIGMA_WRITE_REGISTER_BLOCK(DSP_I2C_ADDR, address, 4, byte_data);
}

uint8_t SIGMA_WRITE_REGISTER_FLOAT(int address, double pData) {
    return SIGMA_WRITE_REGISTER_INTEGER(address, SIGMASTUDIOTYPE_FIXPOINT_CONVERT(pData));
}

// 便利函数：带错误检查的参数写入
bool SIGMA_WRITE_PARAM_SAFE(int address, double value) {
    uint8_t result = SIGMA_WRITE_REGISTER_FLOAT(address, value);
    if (result != SIGMA_SUCCESS) {
        SIGMA_DEBUG_PRINT("参数写入失败，地址: 0x");
        SIGMA_DEBUG_PRINT_HEX(address);
        SIGMA_DEBUG_PRINT(", 错误: ");
        SIGMA_DEBUG_PRINTLN(result);
        return false;
    }
    return true;
}

// 便利函数：批量参数写入
bool SIGMA_WRITE_PARAMS_SAFE(int startAddress, double values[], int count) {
    for (int i = 0; i < count; i++) {
        if (!SIGMA_WRITE_PARAM_SAFE(startAddress + i, values[i])) {
            return false;
        }
    }
    return true;
}

void SIGMA_WRITE_DELAY(byte devAddress, int length, byte pData[]) {
    int delay_length = 0;    // 初始化延迟长度变量
    for (byte i = length; i > 0; i--) {
        // 解包pData以计算延迟长度为整数
        delay_length = (delay_length << 8) + pData[i];
    }
    delay(delay_length);    // 延迟此处理器（不是DSP）适当的时间
}

void SIGMA_WRITE_DELAY(byte devAddress, int length, const uint8_t pData[]) {
    int delay_length = 0;    // 初始化延迟长度变量
    for (byte i = length; i > 0; i--) {
        // 解包pData以计算延迟长度为整数
        delay_length = (delay_length << 8) + pgm_read_byte_near(pData+i);
    }
    delay(delay_length);    // 延迟此处理器（不是DSP）适当的时间
}


// SAFELOAD 宏定义映射
#ifndef SAFELOAD_DATA_ADDR
  #ifdef MOD_SAFELOADMODULE_DATA_SAFELOAD0_ADDR
    #define SAFELOAD_DATA_ADDR MOD_SAFELOADMODULE_DATA_SAFELOAD0_ADDR
  #else
    #define SAFELOAD_DATA_ADDR 24576  // 默认值
  #endif
#endif

#ifndef SAFELOAD_ADDR_ADDR
  #ifdef MOD_SAFELOADMODULE_ADDRESS_SAFELOAD_ADDR
    #define SAFELOAD_ADDR_ADDR MOD_SAFELOADMODULE_ADDRESS_SAFELOAD_ADDR
  #else
    #define SAFELOAD_ADDR_ADDR 24581  // 默认值
  #endif
#endif

#ifndef SAFELOAD_SLOTS_ADDR
  #ifdef MOD_SAFELOADMODULE_NUM_SAFELOAD_ADDR
    #define SAFELOAD_SLOTS_ADDR MOD_SAFELOADMODULE_NUM_SAFELOAD_ADDR
  #else
    #define SAFELOAD_SLOTS_ADDR 24582  // 默认值
  #endif
#endif

// SAFELOAD slots 数据数组
#ifndef SAFELOAD_SLOTS_DATA_1
  static const uint8_t SAFELOAD_SLOTS_DATA_1[] = {1, 1, 1, 1, 1};  // 默认值数组
#endif

void SIGMA_WRITE_SAFELOAD_REGISTER_BLOCK(int address, int length, uint8_t pData[]){
  SIGMA_WRITE_REGISTER_BLOCK(DSP_I2C_ADDR, SAFELOAD_DATA_ADDR, length, pData);
  SIGMA_WRITE_REGISTER_INTEGER(SAFELOAD_ADDR_ADDR, address);
  SIGMA_WRITE_REGISTER_BLOCK(DSP_I2C_ADDR, SAFELOAD_SLOTS_ADDR, length, SAFELOAD_SLOTS_DATA_1);
}

void SIGMA_WRITE_SAFELOAD_REGISTER_BLOCK(int address, int length, const uint8_t pData[], int addrOffset){
  SIGMA_WRITE_REGISTER_BLOCK(DSP_I2C_ADDR, SAFELOAD_DATA_ADDR, length, pData, addrOffset);
  SIGMA_WRITE_REGISTER_INTEGER(SAFELOAD_ADDR_ADDR, address);
  SIGMA_WRITE_REGISTER_BLOCK(DSP_I2C_ADDR, SAFELOAD_SLOTS_ADDR, length, SAFELOAD_SLOTS_DATA_1);
}

// ========== 优化的读取函数 ==========
uint8_t SIGMA_READ_REGISTER_BYTES(int address, int length, byte* pData) {
    // 参数验证
    if (length <= 0 || !pData) {
        g_sigma_last_error = SIGMA_ERROR_INVALID_PARAM;
        return g_sigma_last_error;
    }
    
    g_sigma_last_error = SIGMA_SUCCESS;
    
#if USE_SPI
    digitalWrite(DSP_SS_PIN, LOW);
    SPI.beginTransaction(settingsA);
    SPI.transfer(0x1);
    SPI.transfer(address >> 8);
    SPI.transfer(address & 0xff);
    for (int i = 0; i < length; i++) {
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
    
    for (int i = 0; i < length; i++) {
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

int32_t SIGMA_READ_REGISTER_INTEGER(int address, int length) {
    if (length > 4) {
        g_sigma_last_error = SIGMA_ERROR_INVALID_PARAM;
        return 0;
    }
    
    byte register_value[4] = {0};
    if (SIGMA_READ_REGISTER_BYTES(address, length, register_value) != SIGMA_SUCCESS) {
        return 0;
    }
    
    int32_t result = 0;
    for (int i = 0; i < length; i++) {
        result = (result << 8) + register_value[i];
    }
    return result;
}

double SIGMA_READ_REGISTER_FLOAT(int address) {
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

// 便利函数：安全读取参数
bool SIGMA_READ_PARAM_SAFE(int address, double* value) {
    if (!value) {
        g_sigma_last_error = SIGMA_ERROR_INVALID_PARAM;
        return false;
    }
    
    *value = SIGMA_READ_REGISTER_FLOAT(address);
    if (g_sigma_last_error != SIGMA_SUCCESS) {
        SIGMA_DEBUG_PRINT("参数读取失败，地址: 0x");
        SIGMA_DEBUG_PRINT_HEX(address);
        SIGMA_DEBUG_PRINT(", 错误: ");
        SIGMA_DEBUG_PRINTLN(g_sigma_last_error);
        return false;
    }
    return true;
}

// 用于读取DSP寄存器并打印到串口的函数，不被
// SigmaStudio导出文件调用
// 由于必须为register_value字节分配内存，请将dataLength保持在较低值
void SIGMA_PRINT_REGISTER(int address, int dataLength) {
    Serial0.print("VALUE AT 0x");
    Serial0.print(address, HEX);
    Serial0.print(": 0x");
    byte register_value[dataLength] = {};
    SIGMA_READ_REGISTER_BYTES(address, dataLength, register_value);
    for (int i = 0; i < dataLength; i++) {
        if (register_value[i] < 16) {
            Serial0.print('0');
        }
        Serial0.print(register_value[i], HEX);
        Serial0.print(' ');
    }
    Serial0.println();
}

/* SIGMASTUDIOTYPE_INTEGER_CONVERT包含在导出文件中但通常不需要。
 * 这里它只是一个直通宏。
 */
#define SIGMASTUDIOTYPE_INTEGER_CONVERT(_value) (_value)

// ========== 新增：便利函数和使用示例 ==========

// 优化的调试打印函数
void SIGMA_PRINT_REGISTER_ENHANCED(int address, int dataLength) {
    if (dataLength > 16) dataLength = 16; // 限制最大长度
    
    Serial.print("REG[0x");
    Serial.print(address, HEX);
    Serial.print("]: ");
    
    byte register_value[16] = {0};
    if (SIGMA_READ_REGISTER_BYTES(address, dataLength, register_value) == SIGMA_SUCCESS) {
        Serial.print("0x");
        for (int i = 0; i < dataLength; i++) {
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

// 打印浮点参数值
void SIGMA_PRINT_PARAM(int address, const char* name = nullptr) {
    if (name) {
        Serial.print(name);
        Serial.print(" [0x");
        Serial.print(address, HEX);
        Serial.print("]: ");
    } else {
        Serial.print("PARAM[0x");
        Serial.print(address, HEX);
        Serial.print("]: ");
    }
    
    double value;
    if (SIGMA_READ_PARAM_SAFE(address, &value)) {
        Serial.println(value, 6);
    } else {
        Serial.println("READ_ERROR");
    }
}

// DSP状态检查函数
bool SIGMA_CHECK_DSP_STATUS() {
    // 尝试读取一个已知寄存器来检查DSP是否响应
    byte testData[2];
    uint8_t result = SIGMA_READ_REGISTER_BYTES(0xF000, 2, testData); // 假设0xF000是状态寄存器
    
    if (result == SIGMA_SUCCESS) {
        SIGMA_DEBUG_PRINTLN("DSP响应正常");
        return true;
    } else {
        SIGMA_DEBUG_PRINT("DSP无响应，错误: ");
        SIGMA_DEBUG_PRINTLN(result);
        return false;
    }
}

// 系统初始化函数
bool SIGMA_SYSTEM_INIT() {
    Serial.println("=== SigmaStudioFW 优化版本初始化 ===");
    
    // 1. 初始化I2C
    SIGMA_I2C_INIT();
    Serial.println("✓ I2C初始化完成");
    
    // 2. 延迟让DSP稳定
    delay(100);
    
    // 3. 检查DSP状态
    if (SIGMA_CHECK_DSP_STATUS()) {
        Serial.println("✓ DSP状态正常");
        return true;
    } else {
        Serial.println("✗ DSP状态异常");
        SIGMA_PRINT_ERROR();
        return false;
    }
}

/*
 * ========== 使用示例 ==========
 * 
 * 1. 在setup()函数中初始化：
 *    void setup() {
 *        Serial.begin(115200);
 *        if (SIGMA_SYSTEM_INIT()) {
 *            Serial.println("系统初始化成功");
 *        }
 *    }
 * 
 * 2. 安全写入参数：
 *    if (SIGMA_WRITE_PARAM_SAFE(0x2000, 0.5)) {
 *        Serial.println("参数写入成功");
 *    }
 * 
 * 3. 批量写入参数：
 *    double params[] = {0.1, 0.2, 0.3, 0.4};
 *    if (SIGMA_WRITE_PARAMS_SAFE(0x2000, params, 4)) {
 *        Serial.println("批量参数写入成功");
 *    }
 * 
 * 4. 安全读取参数：
 *    double value;
 *    if (SIGMA_READ_PARAM_SAFE(0x2000, &value)) {
 *        Serial.print("参数值: ");
 *        Serial.println(value, 6);
 *    }
 * 
 * 5. 调试寄存器内容：
 *    SIGMA_PRINT_REGISTER_ENHANCED(0x1000, 4);
 *    SIGMA_PRINT_PARAM(0x2000, "Volume");
 * 
 * 6. 错误处理：
 *    uint8_t result = SIGMA_WRITE_REGISTER_BLOCK(addr, len, data);
 *    if (result != SIGMA_SUCCESS) {
 *        SIGMA_PRINT_ERROR();
 *    }
 * 
 * ========== 优化功能说明 ==========
 * 
 * 1. 错误处理系统：
 *    - 详细的错误代码定义
 *    - I2C超时检测
 *    - 传输状态验证
 *    - 错误追踪和清除
 * 
 * 2. 性能优化：
 *    - 可配置的I2C时钟频率
 *    - 优化的分块传输算法
 *    - 参数验证
 *    - 减少代码重复
 * 
 * 3. 调试功能：
 *    - 可开关的调试输出
 *    - 增强的寄存器打印
 *    - 参数值显示
 *    - 系统状态检查
 * 
 * 4. 便利函数：
 *    - 安全的参数读写
 *    - 批量操作支持
 *    - 系统初始化
 *    - 状态检查
 * 
 * ========== 配置选项 ==========
 * 
 * 在包含此头文件前定义以下宏来配置行为：
 * 
 * #define SIGMA_DEBUG 1              // 开启调试输出
 * #define I2C_TIMEOUT_MS 1000        // I2C超时时间
 * #define I2C_CLOCK_SPEED 400000     // I2C时钟频率
 * #define MAX_I2C_DATA_LENGTH 30     // I2C缓冲区大小
 */

#endif
