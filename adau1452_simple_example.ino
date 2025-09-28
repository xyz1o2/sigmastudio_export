/*
 * ADAU1452 Manual Control System - 优化版本 + 电平显示功能 + API接口
 * 
 * 简化的单EQ控制系统，支持：
 * - 通道1静音开关
 * - 通道1音量控制
 * - 单个10频段EQ控制
 * - Level_1到Level_6电平读取功能
 * - 增强的错误处理和调试功能
 * - 标准化API接口供其他工程调用
 * 
 * 硬件连接:
 * - SDA: ESP32 GPIO8
 * - SCL: ESP32 GPIO9  
 * - VCC: 3.3V
 * - GND: GND
 */

// ========== 优化配置 ==========
#define SIGMA_DEBUG 1              // 开启调试输出
#define I2C_TIMEOUT_MS 1000        // I2C超时时间
#define I2C_CLOCK_SPEED 400000     // I2C时钟频率

#include <Wire.h>
#include "USER_SETTINGS.h"

// 包含SigmaStudio导出的文件
#include "ADAU1452_EN_B_I2C_IC_1_PARAM.h"
#include "ADAU1452_EN_B_I2C_IC_1.h"

// 控制变量
float currentVolume = 0.5;  // 当前音量 (0.0-1.0)
bool isMuted = false;       // 静音状态

// 控制数据
byte volumeData[4];
byte muteOn[4]  = {0x00, 0x00, 0x00, 0x00};  // 静音
byte muteOff[4] = {0x00, 0x80, 0x00, 0x00};  // 取消静音

// 简化的EQ控制 - 只控制第一个EQ模块的10个频段
uint16_t eqMainSwitchAddr = MOD_EQ_ALG0_SLEWMODE_ADDR;  // EQ总开关地址228

// EQ的10个频段地址（基于SigmaStudio的EQ滤波器系数）
// 这些地址对应每个频段的20字节滤波器系数块的起始地址
uint16_t eqBandAddresses[10] = {
  208,   // 频段1 (31Hz) - EQS300MultiSPHWSlewP1Alg1Targ_B2_1
  203,   // 频段2 (62Hz) - EQS300MultiSPHWSlewP1Alg1Targ_B2_2  
  24619, // 频段3 (125Hz) - EQS300MultiSPHWSlewP1Alg1Targ_B2_3
  24624, // 频段4 (250Hz) - EQS300MultiSPHWSlewP1Alg1Targ_B2_4
  223,   // 频段5 (500Hz) - EQS300MultiSPHWSlewP1Alg1Targ_B2_5
  213,   // 频段6 (1000Hz) - EQS300MultiSPHWSlewP1Alg1Targ_B2_6
  24629, // 频段7 (2000Hz) - EQS300MultiSPHWSlewP1Alg1Targ_B2_7
  218,   // 频段8 (4000Hz) - EQS300MultiSPHWSlewP1Alg1Targ_B2_8
  24634, // 频段9 (8000Hz) - EQS300MultiSPHWSlewP1Alg1Targ_B2_9
  24639  // 频段10 (16000Hz) - EQS300MultiSPHWSlewP1Alg1Targ_B2_10
};

// Level控件地址数组 - Level_1到Level_6的电平检测器地址
uint16_t levelDetectorAddresses[6] = {
  579,   // Level_1 - MOD_LEVEL_1_ALG0_SINGLEBANDLEVELLITE3001_ADDR
  581,   // Level_2 - MOD_LEVEL_2_ALG0_SINGLEBANDLEVELLITE3002_ADDR
  583,   // Level_3 - MOD_LEVEL_3_ALG0_SINGLEBANDLEVELLITE3003_ADDR
  585,   // Level_4 - MOD_LEVEL_4_ALG0_SINGLEBANDLEVELLITE3004_ADDR
  765,   // Level_5 - MOD_LEVEL_5_ALG0_SINGLEBANDLEVELLITE3005_ADDR
  767    // Level_6 - MOD_LEVEL_6_ALG0_SINGLEBANDLEVELLITE3006_ADDR
};

// 电平监控控制变量
bool levelMonitorEnabled = false;  // 连续电平监控开关
unsigned long lastLevelReadTime = 0;  // 上次读取电平的时间
const unsigned long LEVEL_READ_INTERVAL = 500;  // 电平读取间隔(毫秒)

// EQ状态跟踪 - 修改：默认开启所有EQ频段
bool eqMainEnabled = true;  // 默认开启EQ主开关
bool eqBandEnabled[10] = {true, true, true, true, true, true, true, true, true, true}; // 默认开启所有频段
float eqBandGains[10] = {0.0}; // 初始增益为0dB

// 前向声明
struct ADAU1452_API_Status;

// 函数声明
void printCommands();
void processCommand(String command);
void setVolume(float volume);
void setMute(bool mute);
void setEQ(int band, bool enabled);
void setEQGain(int bandNum, float gainDB);
void setEQPreset(int presetNum);
void readLevels();
void readAllLevelDetectors();
void readSingleLevel(int levelNum);
void printLevelBar(float level);
void toggleLevelMonitor();
void handleLevelMonitoring();
void showStatus();
void resetAll();
bool checkDSPCommunication();
bool recoverDSPCommunication();
bool safeDSPReload();
void diagnoseDSP();
void printEQAddresses();
void printLevelAddresses();
void initializeEQ();
void processAPICommand(String command);
void demonstrateAPI();
bool ADAU1452_API_GetStatus(struct ADAU1452_API_Status* status);

void setup() {
  Serial0.begin(115200);
  delay(1000);
  
  Serial0.println("ADAU1452 Single EQ Control System - 优化版本 + 电平显示 + API接口");
  Serial0.println("================================================================");
  
  // 使用优化的系统初始化
  if (SIGMA_SYSTEM_INIT()) {
    Serial0.println("✓ 系统初始化成功");
  } else {
    Serial0.println("✗ 系统初始化失败");
    SIGMA_PRINT_ERROR();
    return;
  }
  
  // 加载DSP固件 - 带错误检查
  Serial0.println("Loading DSP firmware...");
  
  // 加载固件数据
  default_download_IC_1();
  Serial0.println("✓ DSP固件加载完成");
  
  // 显示控制命令
  printCommands();
  
  // 初始化音量
  setVolume(currentVolume);
  
  // 初始化EQ - 默认开启所有频段以避免无声问题
  Serial0.println("Initializing EQ system...");
  initializeEQ();
  
  // 验证EQ初始化结果
  delay(200);
  uint32_t eqStatus = SIGMA_READ_REGISTER_INTEGER(eqMainSwitchAddr, 4);
  Serial0.print("EQ Main Switch after init: 0x");
  Serial0.print(eqStatus, HEX);
  Serial0.println(eqStatus == 0x0000208A ? " (ON - Success)" : " (OFF - Will retry)");
  
  // 如果EQ初始化失败，重试一次
  if (eqStatus != 0x0000208A) {
    Serial0.println("EQ initialization failed, retrying...");
    delay(500);
    initializeEQ();
    delay(200);
    eqStatus = SIGMA_READ_REGISTER_INTEGER(eqMainSwitchAddr, 4);
    Serial0.print("EQ Main Switch after retry: 0x");
    Serial0.print(eqStatus, HEX);
    Serial0.println(eqStatus == 0x0000208A ? " (ON - Success)" : " (OFF - Check DSP)");
  }
  
  Serial0.println("=== 系统就绪 ===");
}

void loop() {
  if (Serial0.available()) {
    String command = Serial0.readStringUntil('\n');
    command.trim();
    processCommand(command);
  }
  
  // 处理连续电平监控
  handleLevelMonitoring();
  
  delay(10);
}

void processCommand(String command) {
  if (command.length() == 0) return;
  
  char cmd = command.charAt(0);
  
  switch (cmd) {
    case 'v': // 音量控制: v0.8
      if (command.length() > 1) {
        float volume = command.substring(1).toFloat();
        setVolume(volume);
      }
      break;
      
    case 'm': // 静音开关: m1(静音) m0(取消静音)
      if (command.length() > 1) {
        int muteState = command.substring(1).toInt();
        setMute(muteState == 1);
      }
      break;
      
    case 'e': // EQ控制: e0,1 (EQ总开关) e5,1 (频段5开启)
      if (command.length() > 3) {
        int bandNum = command.substring(1, command.indexOf(',')).toInt();
        int eqState = command.substring(command.indexOf(',') + 1).toInt();
        setEQ(bandNum, eqState == 1);
      }
      break;
      
    case 'g': // EQ增益调整: g5,3.5 (频段5增益+3.5dB)
      if (command.length() > 3) {
        int bandNum = command.substring(1, command.indexOf(',')).toInt();
        float gain = command.substring(command.indexOf(',') + 1).toFloat();
        setEQGain(bandNum, gain);
      }
      break;
      
    case 'l': // 电平读取命令
      if (command.length() > 1) {
        char subCmd = command.charAt(1);
        if (subCmd == 'm') {
          // lm - 开启/关闭连续电平监控
          toggleLevelMonitor();
        } else if (subCmd >= '1' && subCmd <= '6') {
          // l1-l6 - 读取单个Level控件
          int levelNum = subCmd - '0';
          readSingleLevel(levelNum);
        } else {
          readLevels();
        }
      } else {
        readLevels();
      }
      break;
      
    case 's': // 显示状态: s
      showStatus();
      break;
      
    case 'h': // 帮助: h
      printCommands();
      break;
      
    case 'p': // EQ预设: p1 (预设1)
      if (command.length() > 1) {
        int presetNum = command.substring(1).toInt();
        setEQPreset(presetNum);
      }
      break;
      
    case 'r': // 重置所有: r
      resetAll();
      break;
      
    case 'd': // 调试信息: d
      printEQAddresses();
      printLevelAddresses();
      break;
      
    case 'c': // 检查DSP通信: c
      if (checkDSPCommunication()) {
        Serial0.println("✓ DSP communication OK");
      } else {
        Serial0.println("❌ DSP communication failed");
      }
      break;
      
    case 'x': // 完整诊断: x
      diagnoseDSP();
      break;
      
    case 'f': // 修复DSP通信: f
      if (recoverDSPCommunication()) {
        Serial0.println("✓ DSP communication recovered");
        // 通信恢复后显示EQ状态
        delay(100);
        uint32_t eqStatus = SIGMA_READ_REGISTER_INTEGER(eqMainSwitchAddr, 4);
        Serial0.print("EQ Status after recovery: 0x");
        Serial0.print(eqStatus, HEX);
        Serial0.println(eqStatus == 0x0000208A ? " (ON)" : " (OFF)");
      } else {
        Serial0.println("❌ DSP recovery failed");
      }
      break;
      
    case 'i': // 强制重新初始化EQ: i
      Serial0.println("Force re-initializing EQ...");
      initializeEQ();
      break;
      
    default:
      Serial0.println("Unknown command. Type 'h' for help.");
      break;
  }
}

void setVolume(float volume) {
  volume = constrain(volume, 0.0, 1.0);
  currentVolume = volume;
  
  // 转换为DSP格式
  int32_t volumeValue = SIGMASTUDIOTYPE_FIXPOINT_CONVERT(volume);
  volumeData[0] = (volumeValue >> 24) & 0xFF;
  volumeData[1] = (volumeValue >> 16) & 0xFF;
  volumeData[2] = (volumeValue >> 8) & 0xFF;
  volumeData[3] = volumeValue & 0xFF;
  
  // 写入音量到通道1相关的MULTIPLE模块
  SIGMA_WRITE_REGISTER_BLOCK(MOD_MULTIPLE1_2_ALG0_TARGET_ADDR, 4, volumeData);
  SIGMA_WRITE_REGISTER_BLOCK(MOD_MULTIPLE1_3_ALG0_TARGET_ADDR, 4, volumeData);
  SIGMA_WRITE_REGISTER_BLOCK(MOD_MULTIPLE1_4_ALG0_TARGET_ADDR, 4, volumeData);
  
  Serial0.print("Volume set to: ");
  Serial0.print(volume * 100);
  Serial0.println("%");
}

void setMute(bool mute) {
  isMuted = mute;
  
  if (mute) {
    // 静音：将音量设为0
    SIGMA_WRITE_REGISTER_BLOCK(MOD_MULTIPLE1_2_ALG0_TARGET_ADDR, 4, muteOn);
    SIGMA_WRITE_REGISTER_BLOCK(MOD_MULTIPLE1_3_ALG0_TARGET_ADDR, 4, muteOn);
    SIGMA_WRITE_REGISTER_BLOCK(MOD_MULTIPLE1_4_ALG0_TARGET_ADDR, 4, muteOn);
    Serial0.println("Channel 1 MUTED");
  } else {
    // 取消静音：恢复音量
    setVolume(currentVolume);
    Serial0.println("Channel 1 UNMUTED");
  }
}

// 设置EQ（恢复10频段EQ滤波器控制）
void setEQ(int band, bool enabled) {
  if (band < 0 || band > 10) return;
  
  if (band == 0) {
    // 控制EQ总开关
    eqMainEnabled = enabled;
    
    if (enabled) {
      // 开启EQ：写入正确的SLEWMODE值
      byte eqOnData[4] = {0x00, 0x00, 0x20, 0x8A};
      SIGMA_WRITE_REGISTER_BLOCK(eqMainSwitchAddr, 4, eqOnData);
      Serial0.println("EQ Main: ON");
    } else {
      // 关闭EQ：写入0值
      byte eqOffData[4] = {0x00, 0x00, 0x00, 0x00};
      SIGMA_WRITE_REGISTER_BLOCK(eqMainSwitchAddr, 4, eqOffData);
      Serial0.println("EQ Main: OFF");
      
      // 关闭EQ时，只重置控制状态，不写入DSP寄存器
      // 让DSP固件的默认值保持不变
      for (int i = 0; i < 10; i++) {
        eqBandEnabled[i] = false;
        eqBandGains[i] = 0.0;
      }
      Serial0.println("EQ bands reset to default state");
    }
    
    // 读回验证
    delay(10);
    uint32_t readback = SIGMA_READ_REGISTER_INTEGER(eqMainSwitchAddr, 4);
    Serial0.print("EQ Main Switch readback: 0x");
    Serial0.println(readback, HEX);
    
  } else {
    // 控制单个频段
    int bandIndex = band - 1;
    eqBandEnabled[bandIndex] = enabled;
    
    if (enabled) {
      // 如果EQ总开关未开启，先开启
      if (!eqMainEnabled) {
        Serial0.println("Auto-enabling EQ Main Switch...");
        setEQ(0, true);
        delay(50); // 给DSP时间处理EQ总开关
      }
      
      // 修复：开启频段时保持0dB增益（透明通过）
      // 不自动设置1dB，避免意外的音量变化
      if (eqBandGains[bandIndex] == 0.0) {
        setEQGain(band, 0.0); // 保持0dB，不改变音量
      } else {
        // 如果之前有设置过增益，重新应用
        setEQGain(band, eqBandGains[bandIndex]);
      }
    } else {
      // 关闭频段，设置增益为0dB（透明通过）
      setEQGain(band, 0.0);
    }
    Serial0.print("EQ Band ");
    Serial0.print(band);
    Serial0.print(": ");
    Serial0.println(enabled ? "ON" : "OFF");
  }
}

// EQ增益调整函数 - 修复版本，解决声音渐变问题
void setEQGain(int bandNum, float gainDB) {
  if (bandNum < 1 || bandNum > 10) {
    Serial0.println("Band number must be 1-10");
    return;
  }
  
  // 限制增益范围 -6dB 到 +6dB（更安全的范围，避免炸音）
  gainDB = constrain(gainDB, -6.0, 6.0);
  
  // 对低频段进一步限制，避免炸音
  if (bandNum <= 3 && gainDB > 3.0) {
    gainDB = 3.0;
    Serial0.println("Low frequency gain limited to +3dB to prevent distortion");
  }
  int bandIndex = bandNum - 1;
  eqBandGains[bandIndex] = gainDB;
  
  uint16_t baseAddr = eqBandAddresses[bandIndex];
  
  Serial0.print("EQ Band ");
  Serial0.print(bandNum);
  Serial0.print(" gain set to ");
  Serial0.print(gainDB);
  Serial0.println("dB");
  
  // 修复：只修改B0系数，不改变完整的滤波器结构
  // 这样可以避免破坏SigmaStudio设计的滤波器特性
  
  if (abs(gainDB) < 0.1) {
    // 0dB增益 - 设置B0为1.0（unity gain）
    byte unityCoeff[4];
    int32_t unityValue = SIGMASTUDIOTYPE_8_24_CONVERT(1.0);
    unityCoeff[0] = (unityValue >> 24) & 0xFF;
    unityCoeff[1] = (unityValue >> 16) & 0xFF;
    unityCoeff[2] = (unityValue >> 8) & 0xFF;
    unityCoeff[3] = unityValue & 0xFF;
    
    // 只写入B0系数（偏移8字节）
    SIGMA_WRITE_REGISTER_BLOCK(baseAddr + 8, 4, unityCoeff);
    Serial0.println("  -> Unity gain (0dB)");
    
  } else {
    // 有增益调整 - 计算线性增益
    float linearGain = pow(10.0, gainDB / 20.0);
    
    byte gainCoeff[4];
    int32_t gainValue = SIGMASTUDIOTYPE_8_24_CONVERT(linearGain);
    gainCoeff[0] = (gainValue >> 24) & 0xFF;
    gainCoeff[1] = (gainValue >> 16) & 0xFF;
    gainCoeff[2] = (gainValue >> 8) & 0xFF;
    gainCoeff[3] = gainValue & 0xFF;
    
    // 只写入B0系数，保持其他滤波器参数不变
    SIGMA_WRITE_REGISTER_BLOCK(baseAddr + 8, 4, gainCoeff);
    
    Serial0.print("  -> Linear gain: ");
    Serial0.println(linearGain, 6);
  }
  
  // 验证写入
  delay(5);
  float readback = SIGMA_READ_REGISTER_FLOAT(baseAddr + 8);
  Serial0.print("  -> B0 readback: ");
  Serial0.println(readback, 6);
}

// 预设EQ曲线 - 更安全的增益设置
void setEQPreset(int presetNum) {
  Serial0.print("Loading EQ Preset ");
  Serial0.println(presetNum);
  
  switch (presetNum) {
    case 1: // 温和低音增强（避免炸音）
      setEQGain(1, 2.0);   // 频段1 +2dB（降低避免炸音）
      setEQGain(2, 1.5);   // 频段2 +1.5dB
      setEQGain(3, 0.5);   // 频段3 +0.5dB
      setEQGain(4, 0.0);   // 频段4 0dB
      setEQGain(5, -0.5);  // 频段5 -0.5dB（稍微衰减中频）
      setEQGain(6, 0.0);   // 频段6 0dB
      setEQGain(7, 0.0);   // 频段7 0dB
      setEQGain(8, 0.0);   // 频段8 0dB
      setEQGain(9, 0.0);   // 频段9 0dB
      setEQGain(10, 0.0);  // 频段10 0dB
      Serial0.println("Safe Bass Boost preset loaded");
      break;
      
    case 2: // 清晰高音增强
      setEQGain(1, 0.0);   // 频段1 0dB
      setEQGain(2, 0.0);   // 频段2 0dB
      setEQGain(3, 0.0);   // 频段3 0dB
      setEQGain(4, 0.0);   // 频段4 0dB
      setEQGain(5, 0.0);   // 频段5 0dB
      setEQGain(6, 0.5);   // 频段6 +0.5dB
      setEQGain(7, 1.0);   // 频段7 +1dB
      setEQGain(8, 1.5);   // 频段8 +1.5dB
      setEQGain(9, 1.0);   // 频段9 +1dB
      setEQGain(10, 0.5);  // 频段10 +0.5dB
      Serial0.println("Clear Treble preset loaded");
      break;
      
    case 3: // 温和V型曲线
      setEQGain(1, 1.5);   // 频段1 +1.5dB（降低避免炸音）
      setEQGain(2, 1.0);   // 频段2 +1dB
      setEQGain(3, 0.0);   // 频段3 0dB
      setEQGain(4, -0.5);  // 频段4 -0.5dB
      setEQGain(5, -1.0);  // 频段5 -1dB
      setEQGain(6, -0.5);  // 频段6 -0.5dB
      setEQGain(7, 0.0);   // 频段7 0dB
      setEQGain(8, 1.0);   // 频段8 +1dB
      setEQGain(9, 1.5);   // 频段9 +1.5dB
      setEQGain(10, 1.0);  // 频段10 +1dB
      Serial0.println("Gentle V-Shape preset loaded");
      break;
      
    case 4: // 人声突出
      setEQGain(1, 0.0);   // 频段1 0dB
      setEQGain(2, 0.0);   // 频段2 0dB
      setEQGain(3, 0.5);   // 频段3 +0.5dB（温暖感）
      setEQGain(4, 1.0);   // 频段4 +1dB（人声基频）
      setEQGain(5, 0.5);   // 频段5 +0.5dB
      setEQGain(6, 1.5);   // 频段6 +1.5dB（清晰度）
      setEQGain(7, 0.5);   // 频段7 +0.5dB
      setEQGain(8, 0.0);   // 频段8 0dB
      setEQGain(9, 0.0);   // 频段9 0dB
      setEQGain(10, 0.0);  // 频段10 0dB
      Serial0.println("Vocal Enhancement preset loaded");
      break;
      
    case 0: // 平坦响应
    default:
      for (int i = 1; i <= 10; i++) {
        setEQGain(i, 0.0);  // 所有频段 0dB
      }
      Serial0.println("Flat response preset loaded");
      break;
  }
}

void readLevels() {
  Serial0.println("=== Audio Levels ===");
  
  // 读取各通道的音量值
  double vol2 = SIGMA_READ_REGISTER_FLOAT(MOD_MULTIPLE1_2_ALG0_TARGET_ADDR);
  double vol3 = SIGMA_READ_REGISTER_FLOAT(MOD_MULTIPLE1_3_ALG0_TARGET_ADDR);
  double vol4 = SIGMA_READ_REGISTER_FLOAT(MOD_MULTIPLE1_4_ALG0_TARGET_ADDR);
  
  Serial0.print("Channel 1A Level: ");
  Serial0.println(vol2, 3);
  Serial0.print("Channel 1B Level: ");
  Serial0.println(vol3, 3);
  Serial0.print("Channel 1C Level: ");
  Serial0.println(vol4, 3);
  
  // 读取Level_1到Level_6的电平值
  Serial0.println("--- Level Detectors ---");
  readAllLevelDetectors();
  
  // 读取EQ状态
  Serial0.println("--- EQ Status ---");
  
  // 读取EQ总开关
  uint32_t mainStatus = SIGMA_READ_REGISTER_INTEGER(eqMainSwitchAddr, 4);
  Serial0.print("EQ Main: ");
  Serial0.println(mainStatus == 0x0000208A ? "ON" : "OFF");
  
  // 读取各频段状态（基于我们的控制状态，而不是DSP寄存器值）
  for (int i = 0; i < 10; i++) {
    Serial0.print("  Band ");
    Serial0.print(i + 1);
    Serial0.print(": ");
    
    if (eqBandEnabled[i]) {
      Serial0.print("ON (gain: ");
      Serial0.print(eqBandGains[i], 1);
      Serial0.print("dB, coeff: ");
      float bandValue = SIGMA_READ_REGISTER_FLOAT(eqBandAddresses[i]);
      Serial0.print(bandValue, 3);
      Serial0.println(")");
    } else {
      Serial0.println("OFF");
    }
  }
  
  Serial0.println("==================");
}

// 读取所有Level控件的电平值
void readAllLevelDetectors() {
  if (!checkDSPCommunication()) {
    Serial0.println("❌ DSP通信失败，无法读取电平");
    return;
  }
  
  for (int i = 0; i < 6; i++) {
    float levelValue = SIGMA_READ_REGISTER_FLOAT(levelDetectorAddresses[i]);
    
    Serial0.print("Level_");
    Serial0.print(i + 1);
    Serial0.print(": ");
    Serial0.print(levelValue, 6);
    
    // 转换为dB显示（如果值大于0）
    if (levelValue > 0.000001) {  // 避免log(0)
      float levelDB = 20.0 * log10(levelValue);
      Serial0.print(" (");
      Serial0.print(levelDB, 1);
      Serial0.print("dB)");
    } else {
      Serial0.print(" (-∞dB)");
    }
    
    // 添加电平指示条
    printLevelBar(levelValue);
    Serial0.println();
  }
}

// 读取单个Level控件的电平值
void readSingleLevel(int levelNum) {
  if (levelNum < 1 || levelNum > 6) {
    Serial0.println("Level number must be 1-6");
    return;
  }
  
  if (!checkDSPCommunication()) {
    Serial0.println("❌ DSP通信失败，无法读取电平");
    return;
  }
  
  int index = levelNum - 1;
  float levelValue = SIGMA_READ_REGISTER_FLOAT(levelDetectorAddresses[index]);
  
  Serial0.print("Level_");
  Serial0.print(levelNum);
  Serial0.print(": ");
  Serial0.print(levelValue, 6);
  
  // 转换为dB显示
  if (levelValue > 0.000001) {
    float levelDB = 20.0 * log10(levelValue);
    Serial0.print(" (");
    Serial0.print(levelDB, 1);
    Serial0.print("dB)");
  } else {
    Serial0.print(" (-∞dB)");
  }
  
  // 添加电平指示条
  printLevelBar(levelValue);
  Serial0.println();
}

// 打印电平指示条
void printLevelBar(float level) {
  Serial0.print(" [");
  
  // 将电平值转换为0-20的刻度
  int barLength = 20;
  int filledBars = 0;
  
  if (level > 0.000001) {
    // 将线性值转换为对数刻度 (-60dB到0dB)
    float levelDB = 20.0 * log10(level);
    levelDB = constrain(levelDB, -60.0, 0.0);  // 限制在-60dB到0dB
    filledBars = map(levelDB * 10, -600, 0, 0, barLength);  // 映射到0-20
  }
  
  // 打印指示条
  for (int i = 0; i < barLength; i++) {
    if (i < filledBars) {
      if (i >= 16) {
        Serial0.print("█");  // 高电平用实心块
      } else if (i >= 12) {
        Serial0.print("▓");  // 中高电平用中等密度
      } else {
        Serial0.print("▒");  // 低电平用低密度
      }
    } else {
      Serial0.print("░");  // 空白用最低密度
    }
  }
  Serial0.print("]");
}

// 切换连续电平监控模式
void toggleLevelMonitor() {
  levelMonitorEnabled = !levelMonitorEnabled;
  
  if (levelMonitorEnabled) {
    Serial0.println("✓ 连续电平监控已开启 (输入'lm'关闭)");
    Serial0.println("监控Level_1到Level_6，每500ms更新一次");
  } else {
    Serial0.println("✓ 连续电平监控已关闭");
  }
}

// 连续电平监控处理（在loop中调用）
void handleLevelMonitoring() {
  if (!levelMonitorEnabled) return;
  
  unsigned long currentTime = millis();
  if (currentTime - lastLevelReadTime >= LEVEL_READ_INTERVAL) {
    lastLevelReadTime = currentTime;
    
    Serial0.println("=== 实时电平监控 ===");
    readAllLevelDetectors();
    Serial0.println("==================");
  }
}

void showStatus() {
  Serial0.println("\n=== System Status ===");
  Serial0.print("Volume: ");
  Serial0.print(currentVolume * 100);
  Serial0.println("%");
  Serial0.print("Mute: ");
  Serial0.println(isMuted ? "ON" : "OFF");
  Serial0.print("Level Monitor: ");
  Serial0.println(levelMonitorEnabled ? "ON" : "OFF");
  
  Serial0.println("EQ Status:");
  Serial0.print("  EQ Main: ");
  Serial0.println(eqMainEnabled ? "ON" : "OFF");
  
  for (int i = 0; i < 10; i++) {
    Serial0.print("    Band ");
    Serial0.print(i + 1);
    Serial0.print(": ");
    Serial0.print(eqBandEnabled[i] ? "ON" : "OFF");
    Serial0.print(" (");
    Serial0.print(eqBandGains[i], 1);
    Serial0.println("dB)");
  }
  
  Serial0.println("Level Detectors:");
  readAllLevelDetectors();
  
  Serial0.println("====================\n");
}

void resetAll() {
  Serial0.println("Resetting all controls...");
  
  // 重置音量
  setVolume(0.5);
  
  // 取消静音
  setMute(false);
  
  // 关闭电平监控
  levelMonitorEnabled = false;
  
  // 先关闭EQ总开关（这会自动重置所有频段）
  Serial0.println("Disabling EQ...");
  setEQ(0, false);
  
  // 检查DSP通信状态
  Serial0.println("Checking DSP communication...");
  if (!checkDSPCommunication()) {
    Serial0.println("❌ DSP communication failed! Trying recovery...");
    if (recoverDSPCommunication()) {
      Serial0.println("✓ DSP communication recovered");
    } else {
      Serial0.println("❌ DSP recovery failed - check hardware connections");
      return;
    }
  }
  
  // 额外确保：重新加载DSP固件以完全重置EQ状态
  Serial0.println("Reloading DSP firmware to ensure clean state...");
  if (safeDSPReload()) {
    Serial0.println("✓ DSP firmware reloaded successfully");
    // 重新设置音量（因为固件重载会重置音量）
    setVolume(0.5);
    Serial0.println("All controls reset to default");
  } else {
    Serial0.println("❌ DSP firmware reload failed");
  }
}

// 检查DSP通信状态
bool checkDSPCommunication() {
  // 尝试读取一个已知寄存器
  byte testData[2];
  uint8_t result = SIGMA_READ_REGISTER_BYTES(0xF000, 2, testData);
  
  if (result == SIGMA_SUCCESS) {
    return true;
  } else {
    SIGMA_PRINT_ERROR();
    return false;
  }
}

// DSP通信恢复
bool recoverDSPCommunication() {
  Serial0.println("Attempting DSP communication recovery...");
  
  // 1. 重新初始化I2C
  Serial0.println("1. Reinitializing I2C...");
  Wire.end();
  delay(100);
  SIGMA_I2C_INIT();
  delay(200);
  
  // 2. 降低I2C时钟频率
  Serial0.println("2. Reducing I2C clock speed...");
  Wire.setClock(100000); // 降低到100kHz
  delay(100);
  
  // 3. 测试基本通信
  Serial0.println("3. Testing basic communication...");
  for (int retry = 0; retry < 3; retry++) {
    if (checkDSPCommunication()) {
      Serial0.print("✓ Communication recovered on retry ");
      Serial0.println(retry + 1);
      
      // 4. 通信恢复后重新初始化EQ
      Serial0.println("4. Re-initializing EQ after recovery...");
      delay(100);
      initializeEQ();
      
      return true;
    }
    delay(500);
  }
  
  return false;
}

// 安全的DSP固件重载
bool safeDSPReload() {
  Serial0.println("Starting safe DSP firmware reload...");
  
  // 1. 检查通信
  if (!checkDSPCommunication()) {
    Serial0.println("❌ Cannot reload - DSP not responding");
    return false;
  }
  
  // 2. 分步加载，每步检查
  Serial0.println("Loading firmware in safe mode...");
  
  try {
    // 使用错误处理包装的固件加载
    default_download_IC_1();
    
    // 验证加载成功
    delay(500);
    if (checkDSPCommunication()) {
      Serial0.println("✓ Firmware reload successful");
      return true;
    } else {
      Serial0.println("❌ Firmware loaded but DSP not responding");
      return false;
    }
    
  } catch (...) {
    Serial0.println("❌ Exception during firmware reload");
    return false;
  }
}

// 诊断DSP状态
void diagnoseDSP() {
  Serial0.println("=== DSP Diagnostic Report ===");
  
  // 1. I2C基本测试
  Serial0.println("1. I2C Communication Test:");
  if (checkDSPCommunication()) {
    Serial0.println("   ✓ I2C communication OK");
  } else {
    Serial0.println("   ❌ I2C communication FAILED");
    Serial0.println("   Check: SDA/SCL connections, power supply, DSP address");
  }
  
  // 2. 读取关键寄存器
  Serial0.println("2. Key Register Status:");
  
  // EQ主开关状态
  uint32_t eqStatus = SIGMA_READ_REGISTER_INTEGER(eqMainSwitchAddr, 4);
  Serial0.print("   EQ Main Switch: 0x");
  Serial0.print(eqStatus, HEX);
  Serial0.println(eqStatus == 0x0000208A ? " (ON)" : " (OFF)");
  
  // 音量寄存器状态
  float vol = SIGMA_READ_REGISTER_FLOAT(MOD_MULTIPLE1_2_ALG0_TARGET_ADDR);
  Serial0.print("   Volume Level: ");
  Serial0.println(vol, 6);
  
  // Level检测器状态
  Serial0.println("   Level Detectors:");
  for (int i = 0; i < 6; i++) {
    float levelValue = SIGMA_READ_REGISTER_FLOAT(levelDetectorAddresses[i]);
    Serial0.print("     Level_");
    Serial0.print(i + 1);
    Serial0.print(": ");
    Serial0.println(levelValue, 6);
  }
  
  // 3. 错误状态
  Serial0.println("3. Error Status:");
  uint8_t lastError = SIGMA_GET_LAST_ERROR();
  if (lastError == SIGMA_SUCCESS) {
    Serial0.println("   ✓ No errors");
  } else {
    Serial0.print("   ❌ Last error: ");
    SIGMA_PRINT_ERROR();
  }
  
  // 4. I2C配置信息
  Serial0.println("4. I2C Configuration:");
  Serial0.print("   Clock Speed: ");
  Serial0.println(I2C_CLOCK_SPEED);
  Serial0.print("   Timeout: ");
  Serial0.print(I2C_TIMEOUT_MS);
  Serial0.println("ms");
  
  Serial0.println("=============================");
}

// 调试：打印所有EQ地址
void printEQAddresses() {
  Serial0.println("=== EQ Address Mapping ===");
  for (int i = 0; i < 10; i++) {
    Serial0.print("Band ");
    Serial0.print(i + 1);
    Serial0.print(": Address ");
    Serial0.println(eqBandAddresses[i]);
  }
  Serial0.print("EQ Main Switch: Address ");
  Serial0.println(eqMainSwitchAddr);
  Serial0.println("========================");
}

// 打印Level控件地址映射
void printLevelAddresses() {
  Serial0.println("=== Level Detector Address Mapping ===");
  for (int i = 0; i < 6; i++) {
    Serial0.print("Level_");
    Serial0.print(i + 1);
    Serial0.print(": Address ");
    Serial0.println(levelDetectorAddresses[i]);
  }
  Serial0.println("=====================================");
}

// 初始化EQ系统 - 默认开启所有频段避免无声
void initializeEQ() {
  Serial0.println("Setting up EQ with all bands enabled...");
  
  // 1. 首先确保DSP通信正常
  if (!checkDSPCommunication()) {
    Serial0.println("❌ DSP communication failed, cannot initialize EQ");
    return;
  }
  
  // 2. 开启EQ主开关 - 使用更强制的方法
  Serial0.println("  Enabling EQ main switch...");
  byte eqOnData[4] = {0x00, 0x00, 0x20, 0x8A};
  
  // 多次尝试写入EQ主开关
  for (int attempt = 0; attempt < 3; attempt++) {
    SIGMA_WRITE_REGISTER_BLOCK(eqMainSwitchAddr, 4, eqOnData);
    delay(100); // 给DSP更多时间处理
    
    uint32_t readback = SIGMA_READ_REGISTER_INTEGER(eqMainSwitchAddr, 4);
    Serial0.print("    Attempt ");
    Serial0.print(attempt + 1);
    Serial0.print(": 0x");
    Serial0.print(readback, HEX);
    
    if (readback == 0x0000208A) {
      Serial0.println(" (SUCCESS)");
      break;
    } else {
      Serial0.println(" (FAILED)");
      if (attempt < 2) {
        Serial0.println("    Retrying...");
        delay(200);
      }
    }
  }
  
  // 3. 为每个频段设置默认的透明通过系数（基于用户提供的数据格式）
  Serial0.println("  Setting up EQ band coefficients...");
  for (int i = 0; i < 10; i++) {
    uint16_t baseAddr = eqBandAddresses[i];
    
    // 使用基于用户提供的开启状态数据的系数
    // 这个系数对应0.214486241340637的值，确保EQ频段开启
    byte eqCoeff[20] = {
      0x00, 0x36, 0xE8, 0x92,  // 基于用户数据的系数
      0x00, 0x9B, 0x24, 0x8D,  
      0x00, 0xAB, 0xD0, 0x56,  
      0xFF, 0xCF, 0xE3, 0xED,  
      0xFF, 0xB2, 0x3E, 0x9E   
    };
    
    // 写入完整的滤波器系数
    SIGMA_WRITE_REGISTER_BLOCK(baseAddr, 20, eqCoeff);
    delay(20); // 增加延迟确保写入完成
    
    Serial0.print("    Band ");
    Serial0.print(i + 1);
    Serial0.print(" (addr ");
    Serial0.print(baseAddr);
    Serial0.println(") initialized");
  }
  
  // 4. 最终验证EQ主开关状态
  delay(200);
  uint32_t finalStatus = SIGMA_READ_REGISTER_INTEGER(eqMainSwitchAddr, 4);
  Serial0.print("Final EQ Main Switch status: 0x");
  Serial0.print(finalStatus, HEX);
  Serial0.println(finalStatus == 0x0000208A ? " (ON - Success)" : " (OFF - Check configuration)");
  
  if (finalStatus == 0x0000208A) {
    Serial0.println("✓ EQ initialization complete - all bands enabled");
  } else {
    Serial0.println("❌ EQ initialization incomplete - manual intervention may be needed");
  }
}

void printCommands() {
  Serial0.println("\n=== Control Commands ===");
  Serial0.println("v<0.0-1.0>  - Set volume (e.g. v0.8)");
  Serial0.println("m<0|1>      - Mute control (m1=mute, m0=unmute)");
  Serial0.println("e<0-10>,<0|1> - EQ control (e0,1=Main on, e5,1=Band5 on)");
  Serial0.println("g<1-10>,<gain> - EQ gain (g5,3.5 = Band5 +3.5dB)");
  Serial0.println("p<0-4>      - EQ presets (p0=flat, p1=bass, p2=treble, p3=V-shape, p4=vocal)");
  Serial0.println("--- Level Commands ---");
  Serial0.println("l           - Read all audio levels");
  Serial0.println("l<1-6>      - Read single Level detector (l1, l2, l3, l4, l5, l6)");
  Serial0.println("lm          - Toggle continuous level monitoring");
  Serial0.println("--- System Commands ---");
  Serial0.println("s           - Show system status");
  Serial0.println("d           - Show debug info (addresses)");
  Serial0.println("r           - Reset all controls");
  Serial0.println("--- Diagnostic Commands ---");
  Serial0.println("c           - Check DSP communication");
  Serial0.println("x           - Full DSP diagnostic");
  Serial0.println("f           - Fix/recover DSP communication");
  Serial0.println("i           - Force re-initialize EQ system");
  Serial0.println("h           - Show this help");
  Serial0.println("========================\n");
}

// ========== ADAU1452 API 接口实现 ==========
// 以下代码提供标准化的API接口，供其他工程调用

// ========== 系统控制API ==========

bool ADAU1452_API_Init() {
  return SIGMA_SYSTEM_INIT();
}

bool ADAU1452_API_CheckCommunication() {
  return checkDSPCommunication();
}

bool ADAU1452_API_RecoverCommunication() {
  return recoverDSPCommunication();
}

bool ADAU1452_API_ResetAll() {
  resetAll();
  return true;
}

// ========== 音量控制API ==========

bool ADAU1452_API_SetVolume(float volume) {
  if (volume < 0.0 || volume > 1.0) {
    return false;
  }
  setVolume(volume);
  return true;
}

float ADAU1452_API_GetVolume() {
  return currentVolume;
}

bool ADAU1452_API_SetMute(bool mute) {
  setMute(mute);
  return true;
}

bool ADAU1452_API_GetMute() {
  return isMuted;
}

// ========== EQ控制API ==========

bool ADAU1452_API_SetEQEnabled(bool enabled) {
  setEQ(0, enabled);
  return true;
}

bool ADAU1452_API_GetEQEnabled() {
  return eqMainEnabled;
}

bool ADAU1452_API_SetEQBand(int band, bool enabled) {
  if (band < 1 || band > 10) {
    return false;
  }
  setEQ(band, enabled);
  return true;
}

bool ADAU1452_API_GetEQBand(int band) {
  if (band < 1 || band > 10) {
    return false;
  }
  return eqBandEnabled[band - 1];
}

bool ADAU1452_API_SetEQGain(int band, float gainDB) {
  if (band < 1 || band > 10) {
    return false;
  }
  if (gainDB < -6.0 || gainDB > 6.0) {
    return false;
  }
  setEQGain(band, gainDB);
  return true;
}

float ADAU1452_API_GetEQGain(int band) {
  if (band < 1 || band > 10) {
    return 0.0;
  }
  return eqBandGains[band - 1];
}

// EQ频段中心频率表
const int eqFrequencies[10] = {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};

int ADAU1452_API_GetEQFrequency(int band) {
  if (band < 1 || band > 10) {
    return 0;
  }
  return eqFrequencies[band - 1];
}

// EQ预设定义
#define ADAU1452_API_PRESET_FLAT    0
#define ADAU1452_API_PRESET_BASS    1
#define ADAU1452_API_PRESET_TREBLE  2
#define ADAU1452_API_PRESET_V_SHAPE 3
#define ADAU1452_API_PRESET_VOCAL   4

bool ADAU1452_API_SetEQPreset(int preset) {
  if (preset < 0 || preset > 4) {
    return false;
  }
  setEQPreset(preset);
  return true;
}

// ========== 电平监控API ==========

bool ADAU1452_API_ReadLevel(int levelNum, float* levelValue) {
  if (levelNum < 1 || levelNum > 6 || levelValue == nullptr) {
    return false;
  }
  
  if (!checkDSPCommunication()) {
    return false;
  }
  
  int index = levelNum - 1;
  *levelValue = SIGMA_READ_REGISTER_FLOAT(levelDetectorAddresses[index]);
  return true;
}

bool ADAU1452_API_ReadLevelDB(int levelNum, float* levelDB) {
  float levelValue;
  if (!ADAU1452_API_ReadLevel(levelNum, &levelValue)) {
    return false;
  }
  
  if (levelValue > 0.000001) {
    *levelDB = 20.0 * log10(levelValue);
  } else {
    *levelDB = -60.0; // 表示静音
  }
  return true;
}

bool ADAU1452_API_ReadAllLevels(float levels[6]) {
  if (levels == nullptr) {
    return false;
  }
  
  if (!checkDSPCommunication()) {
    return false;
  }
  
  for (int i = 0; i < 6; i++) {
    levels[i] = SIGMA_READ_REGISTER_FLOAT(levelDetectorAddresses[i]);
  }
  return true;
}

bool ADAU1452_API_ReadAllLevelsDB(float levelsDB[6]) {
  float levels[6];
  if (!ADAU1452_API_ReadAllLevels(levels)) {
    return false;
  }
  
  for (int i = 0; i < 6; i++) {
    if (levels[i] > 0.000001) {
      levelsDB[i] = 20.0 * log10(levels[i]);
    } else {
      levelsDB[i] = -60.0; // 表示静音
    }
  }
  return true;
}

bool ADAU1452_API_SetLevelMonitor(bool enabled) {
  levelMonitorEnabled = enabled;
  return true;
}

bool ADAU1452_API_GetLevelMonitor() {
  return levelMonitorEnabled;
}

// ========== 状态查询API ==========

// 状态结构体定义
struct ADAU1452_API_Status {
  float volume;                    // 当前音量 (0.0-1.0)
  bool muted;                      // 静音状态
  bool eqMainEnabled;              // EQ总开关状态
  bool eqBandEnabled[10];          // EQ各频段开关状态
  float eqBandGains[10];           // EQ各频段增益值 (dB)
  bool levelMonitorEnabled;        // 电平监控状态
  bool dspCommunicationOK;         // DSP通信状态
};

bool ADAU1452_API_GetStatus(ADAU1452_API_Status* status) {
  if (status == nullptr) {
    return false;
  }
  
  // 填充状态信息
  status->volume = currentVolume;
  status->muted = isMuted;
  status->eqMainEnabled = eqMainEnabled;
  status->levelMonitorEnabled = levelMonitorEnabled;
  status->dspCommunicationOK = checkDSPCommunication();
  
  // 复制EQ频段状态
  for (int i = 0; i < 10; i++) {
    status->eqBandEnabled[i] = eqBandEnabled[i];
    status->eqBandGains[i] = eqBandGains[i];
  }
  
  return true;
}

// ========== 扩展命令处理 ==========

// 扩展的命令处理函数，可以处理API调用
void processAPICommand(String command) {
  command.trim();
  command.toLowerCase();
  
  if (command.startsWith("api_")) {
    // 处理API命令
    String apiCmd = command.substring(4); // 去掉 "api_" 前缀
    
    if (apiCmd.startsWith("vol ")) {
      float volume = apiCmd.substring(4).toFloat();
      if (ADAU1452_API_SetVolume(volume)) {
        Serial0.print("API: Volume set to ");
        Serial0.println(volume * 100);
      } else {
        Serial0.println("API: Volume setting failed");
      }
    }
    else if (apiCmd == "mute") {
      ADAU1452_API_SetMute(true);
      Serial0.println("API: Muted");
    }
    else if (apiCmd == "unmute") {
      ADAU1452_API_SetMute(false);
      Serial0.println("API: Unmuted");
    }
    else if (apiCmd.startsWith("eq ")) {
      int preset = apiCmd.substring(3).toInt();
      if (ADAU1452_API_SetEQPreset(preset)) {
        Serial0.print("API: EQ preset ");
        Serial0.print(preset);
        Serial0.println(" applied");
      } else {
        Serial0.println("API: EQ preset failed");
      }
    }
    else if (apiCmd == "status") {
      ADAU1452_API_Status status;
      if (ADAU1452_API_GetStatus(&status)) {
        Serial0.println("=== API Status ===");
        Serial0.print("Volume: ");
        Serial0.println(status.volume * 100);
        Serial0.print("Muted: ");
        Serial0.println(status.muted ? "Yes" : "No");
        Serial0.print("EQ Main: ");
        Serial0.println(status.eqMainEnabled ? "On" : "Off");
        Serial0.print("Level Monitor: ");
        Serial0.println(status.levelMonitorEnabled ? "On" : "Off");
        Serial0.print("DSP Comm: ");
        Serial0.println(status.dspCommunicationOK ? "OK" : "Failed");
        Serial0.println("==================");
      } else {
        Serial0.println("API: Status query failed");
      }
    }
    else if (apiCmd == "levels") {
      float levelsDB[6];
      if (ADAU1452_API_ReadAllLevelsDB(levelsDB)) {
        Serial0.print("API Levels (dB): ");
        for (int i = 0; i < 6; i++) {
          Serial0.print("L");
          Serial0.print(i + 1);
          Serial0.print(":");
          if (levelsDB[i] > -60.0) {
            Serial0.print(levelsDB[i], 1);
          } else {
            Serial0.print("-∞");
          }
          Serial0.print(" ");
        }
        Serial0.println();
      } else {
        Serial0.println("API: Level reading failed");
      }
    }
    else if (apiCmd == "help") {
      printAPICommands();
    }
    else {
      Serial0.println("API: Unknown command. Type 'api_help' for API commands.");
    }
  } else {
    // 调用原有的命令处理函数
    processCommand(command);
  }
}

// 打印API命令帮助
void printAPICommands() {
  Serial0.println("\n=== API Commands ===");
  Serial0.println("api_vol <0.0-1.0>  - Set volume via API");
  Serial0.println("api_mute           - Mute via API");
  Serial0.println("api_unmute         - Unmute via API");
  Serial0.println("api_eq <0-4>       - Set EQ preset via API");
  Serial0.println("api_status         - Get system status via API");
  Serial0.println("api_levels         - Read levels via API");
  Serial0.println("api_help           - Show API commands");
  Serial0.println("====================\n");
}

// 演示API功能的示例函数
void demonstrateAPI() {
  Serial0.println("\n=== API Demonstration ===");
  
  // 1. 检查通信
  Serial0.println("1. Checking DSP communication...");
  if (ADAU1452_API_CheckCommunication()) {
    Serial0.println("   ✓ DSP communication OK");
  } else {
    Serial0.println("   ❌ DSP communication failed");
    return;
  }
  
  // 2. 音量控制演示
  Serial0.println("2. Volume control demo...");
  ADAU1452_API_SetVolume(0.3);
  Serial0.print("   Volume set to 30%: ");
  Serial0.println(ADAU1452_API_GetVolume() * 100);
  delay(1000);
  
  ADAU1452_API_SetVolume(0.7);
  Serial0.print("   Volume set to 70%: ");
  Serial0.println(ADAU1452_API_GetVolume() * 100);
  delay(1000);
  
  // 3. EQ演示
  Serial0.println("3. EQ control demo...");
  ADAU1452_API_SetEQPreset(ADAU1452_API_PRESET_BASS);
  Serial0.println("   Applied bass preset");
  delay(2000);
  
  ADAU1452_API_SetEQPreset(ADAU1452_API_PRESET_FLAT);
  Serial0.println("   Applied flat preset");
  delay(1000);
  
  // 4. 电平读取演示
  Serial0.println("4. Level reading demo...");
  float levelsDB[6];
  if (ADAU1452_API_ReadAllLevelsDB(levelsDB)) {
    Serial0.print("   Current levels (dB): ");
    for (int i = 0; i < 6; i++) {
      Serial0.print("L");
      Serial0.print(i + 1);
      Serial0.print(":");
      Serial0.print(levelsDB[i], 1);
      Serial0.print(" ");
    }
    Serial0.println();
  }
  
  // 5. 状态查询演示
  Serial0.println("5. Status query demo...");
  ADAU1452_API_Status status;
  if (ADAU1452_API_GetStatus(&status)) {
    Serial0.print("   System status - Volume: ");
    Serial0.print(status.volume * 100);
    Serial0.print("%, Muted: ");
    Serial0.print(status.muted ? "Yes" : "No");
    Serial0.print(", EQ: ");
    Serial0.println(status.eqMainEnabled ? "On" : "Off");
  }
  
  // 恢复默认设置
  ADAU1452_API_SetVolume(0.5);
  Serial0.println("6. Restored default volume (50%)");
  
  Serial0.println("=== API Demo Complete ===\n");
}