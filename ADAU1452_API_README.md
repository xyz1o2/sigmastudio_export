# ADAU1452 API 接口文档

## 概述

这个API接口库为ADAU1452 DSP芯片提供了简单易用的控制接口，基于 `adau1452_simple_example.ino` 中的功能实现。

## 文件说明

- `ADAU1452_API.h` - API头文件，包含所有函数声明和数据结构
- `ADAU1452_API.cpp` - API实现文件，包含所有函数的具体实现
- `ADAU1452_API_Example.ino` - 使用示例，展示如何在其他项目中使用这些API
- `ADAU1452_API_README.md` - 本文档

## 硬件要求

- ESP32 或其他兼容的微控制器
- ADAU1452 DSP芯片
- I2C连接：
  - SDA: GPIO8 (可配置)
  - SCL: GPIO9 (可配置)
  - VCC: 3.3V
  - GND: GND

## 软件依赖

- Arduino IDE 或 PlatformIO
- Wire库 (I2C通信)
- SigmaStudio生成的头文件：
  - `ADAU1452_EN_B_I2C_IC_1_PARAM.h`
  - `ADAU1452_EN_B_I2C_IC_1.h`
  - `USER_SETTINGS.h`

## 快速开始

### 1. 包含必要文件

```cpp
#include "ADAU1452_API.h"
```

### 2. 初始化系统

```cpp
void setup() {
  Serial.begin(115200);
  
  // 初始化ADAU1452系统
  if (ADAU1452_Init()) {
    Serial.println("DSP初始化成功");
  } else {
    Serial.println("DSP初始化失败");
    return;
  }
}
```

### 3. 基本控制

```cpp
// 音量控制
ADAU1452_SetVolume(0.5);        // 设置音量为50%
float volume = ADAU1452_GetVolume();  // 获取当前音量

// 静音控制
ADAU1452_SetMute(true);         // 静音
ADAU1452_SetMute(false);        // 取消静音
bool muted = ADAU1452_GetMute(); // 获取静音状态
```

## API 参考

### 系统控制函数

#### `bool ADAU1452_Init()`
初始化ADAU1452系统
- **返回值**: 成功返回true，失败返回false

#### `bool ADAU1452_CheckCommunication()`
检查DSP通信状态
- **返回值**: 通信正常返回true，异常返回false

#### `bool ADAU1452_RecoverCommunication()`
尝试恢复DSP通信
- **返回值**: 恢复成功返回true，失败返回false

#### `bool ADAU1452_ResetAll()`
重置所有控制参数到默认值
- **返回值**: 重置成功返回true，失败返回false

### 音量控制函数

#### `bool ADAU1452_SetVolume(float volume)`
设置音量
- **参数**: `volume` - 音量值 (0.0-1.0)
- **返回值**: 设置成功返回true，失败返回false

#### `float ADAU1452_GetVolume()`
获取当前音量
- **返回值**: 当前音量值 (0.0-1.0)

#### `bool ADAU1452_SetMute(bool mute)`
设置静音状态
- **参数**: `mute` - true为静音，false为取消静音
- **返回值**: 设置成功返回true，失败返回false

#### `bool ADAU1452_GetMute()`
获取静音状态
- **返回值**: true为静音，false为未静音

### EQ控制函数

#### `bool ADAU1452_SetEQEnabled(bool enabled)`
开启/关闭EQ总开关
- **参数**: `enabled` - true为开启，false为关闭
- **返回值**: 设置成功返回true，失败返回false

#### `bool ADAU1452_GetEQEnabled()`
获取EQ总开关状态
- **返回值**: true为开启，false为关闭

#### `bool ADAU1452_SetEQBand(int band, bool enabled)`
开启/关闭指定EQ频段
- **参数**: 
  - `band` - 频段编号 (1-10)
  - `enabled` - true为开启，false为关闭
- **返回值**: 设置成功返回true，失败返回false

#### `bool ADAU1452_GetEQBand(int band)`
获取指定EQ频段状态
- **参数**: `band` - 频段编号 (1-10)
- **返回值**: true为开启，false为关闭

#### `bool ADAU1452_SetEQGain(int band, float gainDB)`
设置EQ频段增益
- **参数**: 
  - `band` - 频段编号 (1-10)
  - `gainDB` - 增益值 (-6.0 到 +6.0 dB)
- **返回值**: 设置成功返回true，失败返回false

#### `float ADAU1452_GetEQGain(int band)`
获取EQ频段增益
- **参数**: `band` - 频段编号 (1-10)
- **返回值**: 增益值 (dB)

#### `int ADAU1452_GetEQFrequency(int band)`
获取EQ频段中心频率
- **参数**: `band` - 频段编号 (1-10)
- **返回值**: 中心频率 (Hz)

### EQ预设函数

#### `bool ADAU1452_SetEQPreset(int preset)`
应用EQ预设
- **参数**: `preset` - 预设编号
  - `ADAU1452_PRESET_FLAT` (0) - 平坦响应
  - `ADAU1452_PRESET_BASS` (1) - 低音增强
  - `ADAU1452_PRESET_TREBLE` (2) - 高音增强
  - `ADAU1452_PRESET_V_SHAPE` (3) - V型曲线
  - `ADAU1452_PRESET_VOCAL` (4) - 人声增强
- **返回值**: 设置成功返回true，失败返回false

### 电平监控函数

#### `bool ADAU1452_ReadLevel(int levelNum, float* levelValue)`
读取单个电平检测器的值
- **参数**: 
  - `levelNum` - 电平检测器编号 (1-6)
  - `levelValue` - 输出电平值的指针
- **返回值**: 读取成功返回true，失败返回false

#### `bool ADAU1452_ReadLevelDB(int levelNum, float* levelDB)`
读取单个电平检测器的dB值
- **参数**: 
  - `levelNum` - 电平检测器编号 (1-6)
  - `levelDB` - 输出dB值的指针
- **返回值**: 读取成功返回true，失败返回false

#### `bool ADAU1452_ReadAllLevels(float levels[6])`
读取所有电平检测器的值
- **参数**: `levels` - 输出电平值数组 (6个元素)
- **返回值**: 读取成功返回true，失败返回false

#### `bool ADAU1452_ReadAllLevelsDB(float levelsDB[6])`
读取所有电平检测器的dB值
- **参数**: `levelsDB` - 输出dB值数组 (6个元素)
- **返回值**: 读取成功返回true，失败返回false

#### `bool ADAU1452_SetLevelMonitor(bool enabled)`
开启/关闭连续电平监控
- **参数**: `enabled` - true为开启，false为关闭
- **返回值**: 设置成功返回true，失败返回false

#### `bool ADAU1452_GetLevelMonitor()`
获取连续电平监控状态
- **返回值**: true为开启，false为关闭

### 状态查询函数

#### `bool ADAU1452_GetStatus(ADAU1452_Status* status)`
获取完整的系统状态
- **参数**: `status` - 输出状态结构体的指针
- **返回值**: 获取成功返回true，失败返回false

## 数据结构

### `ADAU1452_Status`
系统状态结构体
```cpp
typedef struct {
  float volume;                    // 当前音量 (0.0-1.0)
  bool muted;                      // 静音状态
  bool eqMainEnabled;              // EQ总开关状态
  bool eqBandEnabled[10];          // EQ各频段开关状态
  float eqBandGains[10];           // EQ各频段增益值 (dB)
  bool levelMonitorEnabled;        // 电平监控状态
  bool dspCommunicationOK;         // DSP通信状态
} ADAU1452_Status;
```

## 使用示例

### 基本音量控制
```cpp
// 设置音量为70%
if (ADAU1452_SetVolume(0.7)) {
  Serial.println("音量设置成功");
} else {
  Serial.println("音量设置失败");
}

// 获取当前音量
float currentVolume = ADAU1452_GetVolume();
Serial.print("当前音量: ");
Serial.println(currentVolume * 100);
```

### EQ控制示例
```cpp
// 开启EQ系统
ADAU1452_SetEQEnabled(true);

// 设置频段1增益为+3dB
ADAU1452_SetEQGain(1, 3.0);

// 应用低音增强预设
ADAU1452_SetEQPreset(ADAU1452_PRESET_BASS);
```

### 电平监控示例
```cpp
// 读取所有电平值
float levels[6];
if (ADAU1452_ReadAllLevelsDB(levels)) {
  for (int i = 0; i < 6; i++) {
    Serial.print("Level ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(levels[i]);
    Serial.println(" dB");
  }
}

// 开启连续电平监控
ADAU1452_SetLevelMonitor(true);
```

### 完整状态查询
```cpp
ADAU1452_Status status;
if (ADAU1452_GetStatus(&status)) {
  Serial.print("音量: ");
  Serial.println(status.volume * 100);
  Serial.print("静音: ");
  Serial.println(status.muted ? "是" : "否");
  Serial.print("EQ开启: ");
  Serial.println(status.eqMainEnabled ? "是" : "否");
  
  // 显示EQ频段状态
  for (int i = 0; i < 10; i++) {
    if (status.eqBandEnabled[i]) {
      Serial.print("频段 ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(status.eqBandGains[i]);
      Serial.println(" dB");
    }
  }
}
```

## 错误处理

所有API函数都返回布尔值来指示操作是否成功。建议在调用API函数时检查返回值：

```cpp
if (!ADAU1452_SetVolume(0.5)) {
  Serial.println("音量设置失败，请检查DSP连接");
  // 可以尝试恢复通信
  if (ADAU1452_RecoverCommunication()) {
    Serial.println("通信已恢复，重试音量设置");
    ADAU1452_SetVolume(0.5);
  }
}
```

## 注意事项

1. **初始化顺序**: 必须先调用 `ADAU1452_Init()` 才能使用其他API函数
2. **参数范围**: 注意各函数的参数范围，超出范围的参数会被自动限制
3. **通信检查**: 如果遇到通信问题，可以使用 `ADAU1452_CheckCommunication()` 和 `ADAU1452_RecoverCommunication()` 函数
4. **EQ增益限制**: EQ增益被限制在 -6dB 到 +6dB 范围内，以防止音频失真
5. **低频段保护**: 低频段 (1-3) 的增益被进一步限制在 +3dB 以内，以防止低频炸音

## 故障排除

### DSP不响应
1. 检查I2C连接 (SDA, SCL)
2. 检查电源连接 (3.3V, GND)
3. 调用 `ADAU1452_CheckCommunication()` 检查通信状态
4. 尝试 `ADAU1452_RecoverCommunication()` 恢复通信

### 音频无输出
1. 检查音量设置 (`ADAU1452_GetVolume()`)
2. 检查静音状态 (`ADAU1452_GetMute()`)
3. 检查EQ设置 (`ADAU1452_GetEQEnabled()`)
4. 尝试 `ADAU1452_ResetAll()` 重置所有设置

### EQ不工作
1. 确保EQ总开关已开启 (`ADAU1452_SetEQEnabled(true)`)
2. 检查各频段是否已开启 (`ADAU1452_GetEQBand()`)
3. 尝试应用预设 (`ADAU1452_SetEQPreset()`)

## 版本信息

- 版本: 1.0.0
- 基于: adau1452_simple_example.ino
- 兼容: Arduino IDE, PlatformIO
- 测试平台: ESP32

## 许可证

本API库基于原始的 adau1452_simple_example.ino 代码开发，遵循相同的许可证条款。