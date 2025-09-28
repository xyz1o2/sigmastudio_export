#!/usr/bin/env python3
"""
SigmaDSP Parameter File Generator Script (Python Edition)
Created by MCUdude - https://github.com/MCUdude/SigmaDSP
 
This script generates SigmaDSP_parameters.h from SigmaStudio export files.
Based on the PowerShell implementation with improved Python features.
"""
 
import os
import re
import sys
import argparse
from datetime import datetime
from pathlib import Path
from typing import List, Optional, Tuple
 
 
class SigmaDSPGenerator:
    def __init__(self, project_dir: str = ".", output_dir: Optional[str] = None):
        self.project_dir = Path(project_dir)
        self.output_dir = Path(output_dir) if output_dir else self.project_dir
        self.output_file = self.output_dir / "SigmaDSP_parameters.h"
        self.errors = []
        self.warnings = []
        
        # 确保输出目录存在
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
    def find_file(self, pattern: str, required: bool = True) -> Optional[Path]:
        """查找匹配模式的文件，优先搜索export目录，然后搜索主项目目录，排除examples目录"""
        files = []
        
        # 首先在export目录中搜索
        export_dirs = list(self.project_dir.rglob("export"))
        for export_dir in export_dirs:
            if 'examples' not in str(export_dir):
                for path in export_dir.glob(pattern):
                    files.append(path)
        
        # 如果在export目录中没找到，搜索整个项目目录（排除examples）
        if not files:
            for path in self.project_dir.rglob(pattern):
                if 'examples' not in str(path):
                    files.append(path)
        
        if not files:
            msg = f"{pattern} not found in project directory or export folders"
            if required:
                self.errors.append(msg)
                print(f"❌ {msg}")
            else:
                self.warnings.append(msg)
                print(f"⚠️  WARNING! {msg}")
            return None
            
        if len(files) > 1:
            # 优先选择export目录中的文件
            export_files = [f for f in files if 'export' in str(f)]
            if export_files:
                if len(export_files) == 1:
                    return export_files[0]
                else:
                    self.warnings.append(f"Multiple files found in export directories for {pattern}, using first: {export_files[0]}")
                    print(f"⚠️  Multiple export files found for {pattern}, using: {export_files[0]}")
                    return export_files[0]
            else:
                self.errors.append(f"Multiple files found for {pattern}: {files}")
                print(f"❌ Multiple files found for {pattern}")
                return None
            
        return files[0]
    
    def get_project_name(self) -> str:
        """获取SigmaStudio项目名称"""
        dspproj_files = list(self.project_dir.rglob("*.dspproj"))
        if dspproj_files:
            # Filter out examples
            main_project = [f for f in dspproj_files if 'examples' not in str(f)]
            if main_project:
                return main_project[0].stem
            return dspproj_files[0].stem # fallback to first found
        return "Unknown"
    
    def detect_chip_type(self, param_content: str, program_content: str = "") -> Tuple[str, str]:
        """检测芯片类型 - 改进的多源检测"""
        # 合并内容进行检测
        combined_content = param_content + "\n" + program_content
        
        # 芯片检测优先级和模式
        chip_patterns = [
            ("ADAU1452", ["ADAU1452", "1452"], "✅ ADAU1452 chip detected"),
            ("ADAU1401", ["ADAU1401", "1401"], "✅ ADAU1401 chip detected"), 
            ("ADAU1702", ["ADAU1702", "1702"], "✅ ADAU1702 chip detected"),
            ("ADAU1701", ["ADAU1701", "1701"], "✅ ADAU1701 chip detected")
        ]
        
        detected_chips = []
        
        for chip_name, patterns, message in chip_patterns:
            for pattern in patterns:
                if pattern in combined_content:
                    detected_chips.append((chip_name, message))
                    break
        
        if not detected_chips:
            print("⚠️  No specific chip detected, defaulting to ADAU1701")
            return "ADAU1701", "// ADAU1701 (default - no chip detected)"
        
        if len(detected_chips) > 1:
            chip_names = [chip[0] for chip in detected_chips]
            print(f"⚠️  Multiple chips detected: {', '.join(chip_names)}")
            print(f"   Using first detected: {detected_chips[0][0]}")
        
        selected_chip = detected_chips[0]
        print(selected_chip[1])
        return selected_chip[0], f"// {selected_chip[0]} detected"
    
    def extract_i2c_addresses(self, dsp_content: str, eeprom_content: str, chip_type: str) -> Tuple[str, str]:
        """提取I2C地址并进行芯片特定处理"""
        dsp_addr_line = ""
        eeprom_addr_line = ""
        
        # 提取DSP地址
        match = re.search(r'#define\s+DEVICE_ADDR_IC_1\s+(0x[0-9A-Fa-f]+)', dsp_content)
        if match:
            addr_hex = match.group(1)
            addr_int = int(addr_hex, 16)
            
            # 芯片特定的I2C地址处理
            if chip_type == "ADAU1452":
                # ADAU1452使用7位地址0x3B，SigmaStudio导出的是8位地址0x76
                if addr_int == 0x76:
                    final_addr = "0x3B"
                    print(f"✅ ADAU1452 I2C address: {final_addr} (from SigmaStudio 8-bit: {addr_hex})")
                else:
                    final_addr = f"0x{(addr_int >> 1):02X}"
                    print(f"⚠️  Converted ADAU1452 I2C address: {final_addr} (from {addr_hex})")
                dsp_addr_line = f"#define DSP_I2C_ADDRESS {final_addr}"
            else:
                # 其他芯片使用标准转换
                final_addr = f"0x{(addr_int >> 1):02X}"
                dsp_addr_line = f"#define DSP_I2C_ADDRESS {final_addr}"
                print(f"✅ DSP I2C address: {final_addr} (from 8-bit: {addr_hex})")
        
        # 提取EEPROM地址
        if eeprom_content:
            match = re.search(r'#define\s+DEVICE_ADDR_IC_2\s+(0x[0-9A-Fa-f]+)', eeprom_content)
            if match:
                addr_hex = match.group(1)
                addr_int = int(addr_hex, 16)
                final_addr = f"0x{(addr_int >> 1):02X}"
                eeprom_addr_line = f"#define EEPROM_I2C_ADDRESS {final_addr}"
                print(f"✅ EEPROM I2C address: {final_addr} (from 8-bit: {addr_hex})")
        
        return dsp_addr_line, eeprom_addr_line
    
    def extract_parameters(self, param_content: str) -> str:
        """提取DSP参数定义 - 按照PowerShell脚本逻辑"""
        lines = param_content.split('\n')
        result = []
        
        for line in lines:
            line = line.strip()
            if not line:
                continue
                
            # 模块注释
            if line.startswith('/* Module'):
                result.append(line)
                continue
            
            # 参数定义 - 匹配PowerShell脚本的条件
            if line.startswith('#define'):
                parts = line.split()
                if len(parts) >= 2:
                    param_name = parts[1]
                    if (param_name.endswith('_COUNT') or 
                        param_name.endswith('_FIXPT') or 
                        param_name.endswith('_VALUES') or 
                        param_name.endswith('_ADDR')):
                        result.append(line)
        
        return '\n'.join(result)
    
    def process_hex_file(self, hex_content: str) -> str:
        """处理EEPROM hex文件"""
        if not hex_content:
            return ""
        
        # 计算数组长度
        hex_values = [x.strip() for x in hex_content.split(',') if x.strip()]
        array_size = len(hex_values)
        
        # 格式化为C数组
        result = f"""/* This array contains the entire DSP program,
    and should be loaded into the external i2c EEPROM */
const uint8_t PROGMEM DSP_eeprom_firmware[{array_size}] = {{
{hex_content}
}};"""
        
        return result
    
    def extract_ram_data(self, program_content: str) -> str:
        """提取RAM数据数组 - 处理DSP Ram Data部分"""
        lines = program_content.split('\n')
        result = []
        in_ram_section = False
        ram_data_lines = []
        
        for line in lines:
            line = line.strip()
            
            # 检测DSP Ram Data部分的开始
            if '/* DSP Ram Data */' in line:
                in_ram_section = True
                result.append(line)  # 添加注释
                continue
            
            if in_ram_section:
                if line.startswith('0x'):
                    # 收集RAM数据
                    ram_data_lines.append(line)
                elif line.startswith('};'):
                    # RAM数据部分结束
                    if ram_data_lines:
                        result.append("const uint8_t PROGMEM DSP_ram_data[] = {")
                        result.extend(ram_data_lines)
                        result.append("};")
                        result.append("")
                    in_ram_section = False
                    ram_data_lines = []
                elif line and not line.startswith('//'):
                    # 其他非注释行，可能是寄存器默认值
                    if ram_data_lines:
                        # 如果已经有数据，先结束当前数组
                        result.append("const uint8_t PROGMEM DSP_ram_data[] = {")
                        result.extend(ram_data_lines)
                        result.append("};")
                        result.append("")
                        ram_data_lines = []
                    
                    # 处理寄存器默认值
                    if 'R0' in line or 'R1' in line or 'R2' in line or 'R3' in line or 'R4' in line:
                        result.append(f"/* Register defaults */")
                        result.append("const uint8_t PROGMEM register_defaults[] = {")
                        result.append(line)
                        result.append("};")
                        result.append("")
        
        # 处理最后收集的RAM数据
        if in_ram_section and ram_data_lines:
            result.append("const uint8_t PROGMEM DSP_ram_data[] = {")
            result.extend(ram_data_lines)
            result.append("};")
            result.append("")
        
        return '\n'.join(result) if result else ""
    
    def extract_program_data(self, program_content: str, chip_type: str) -> str:
        """提取程序数据 - 改进的解析逻辑，确保正确的PROGMEM数组声明"""
        lines = program_content.split('\n')
        result = []
        
        # 更智能的行范围检测
        start_line = 0
        end_line = len(lines)
        
        # 找到实际内容开始和结束位置
        for i, line in enumerate(lines):
            if '#define' in line and '_IC_1' in line:
                start_line = max(0, i - 5)  # 从定义前几行开始
                break
        
        # 从后往前找结束位置
        for i in range(len(lines) - 1, -1, -1):
            if lines[i].strip() and not lines[i].strip().startswith('//'):
                end_line = i + 1
                break
        
        program_lines = lines[start_line:end_line]
        
        # 状态变量
        in_data_array = False
        data_lines = []
        
        for line in program_lines:
            line = line.strip()
            if not line or line.startswith('//'):
                continue
                
            parts = line.split()
            last_part = parts[-1] if parts else ""
            
            # 跳过寄存器默认值注释和数据
            if '/* Register Default' in line:
                continue
            
            # 注释行
            if line.startswith('/*'):
                result.append(line)
                continue
            
            # 程序大小和地址定义
            if '#define PROGRAM_SIZE_IC_1' in line:
                result.append(f"#define PROGRAM_SIZE {last_part}")
            elif '#define PROGRAM_ADDR_IC_1' in line:
                result.append(f"#define PROGRAM_ADDR {last_part}")
                # 芯片特定的寄存器大小
                if chip_type == "ADAU1452":
                    result.append("#define PROGRAM_REGSIZE 4")  # ADAU1452使用4字节寄存器
                else:
                    result.append("#define PROGRAM_REGSIZE 5")  # ADAU1701等使用5字节
            elif '#define PARAM_SIZE_IC_1' in line:
                result.append(f"#define PARAMETER_SIZE {last_part}")
            elif '#define PARAM_ADDR_IC_1' in line:
                result.append(f"#define PARAMETER_ADDR {last_part}")
                result.append("#define PARAMETER_REGSIZE 4")
            
            # 硬件配置
            elif '#define R3_HWCONFIGURATION_IC_1_SIZE' in line:
                result.extend([
                    f"#define HARDWARE_CONF_SIZE {last_part}",
                    self._get_hardware_conf_addr(chip_type),
                    "#define HARDWARE_CONF_REGSIZE 1",
                    ""
                ])
            
            # 数据数组定义 - 开始收集数据
            elif 'ADI_REG_TYPE Program_Data' in line:
                if in_data_array and data_lines:
                    # 结束之前的数据数组
                    result.append("const uint8_t PROGMEM DSP_program_data[PROGRAM_SIZE] = {")
                    result.extend(data_lines)
                    result.append("};")
                    result.append("")
                    data_lines = []
                in_data_array = True
                result.append("const uint8_t PROGMEM DSP_program_data[PROGRAM_SIZE] = {")
            elif 'ADI_REG_TYPE Param_Data' in line:
                if in_data_array and data_lines:
                    # 结束之前的数据数组
                    result.append("const uint8_t PROGMEM DSP_parameter_data[PARAMETER_SIZE] = {")
                    result.extend(data_lines)
                    result.append("};")
                    result.append("")
                    data_lines = []
                in_data_array = True
                result.append("const uint8_t PROGMEM DSP_parameter_data[PARAMETER_SIZE] = {")
            elif 'ADI_REG_TYPE R0_COREREGISTER' in line:
                if in_data_array and data_lines:
                    result.extend(data_lines)
                    result.append("};")
                    result.append("")
                    data_lines = []
                    in_data_array = False
                result.extend([
                    "#define CORE_REGISTER_R0_SIZE 2",
                    self._get_core_register_r0_addr(chip_type),
                    "#define CORE_REGISTER_R0_REGSIZE 2",
                    "const uint8_t PROGMEM DSP_core_register_R0_data[CORE_REGISTER_R0_SIZE] = {"
                ])
                in_data_array = True
            elif 'ADI_REG_TYPE R3_HWCONFIGURATION' in line:
                if in_data_array and data_lines:
                    result.extend(data_lines)
                    result.append("};")
                    result.append("")
                    data_lines = []
                    in_data_array = False
                result.append("const uint8_t PROGMEM DSP_hardware_conf_data[HARDWARE_CONF_SIZE] = {")
                in_data_array = True
            elif 'ADI_REG_TYPE R4_COREREGISTER' in line:
                if in_data_array and data_lines:
                    result.extend(data_lines)
                    result.append("};")
                    result.append("")
                    data_lines = []
                    in_data_array = False
                result.extend([
                    "#define CORE_REGISTER_R4_SIZE 2",
                    self._get_core_register_r4_addr(chip_type),
                    "#define CORE_REGISTER_R4_REGSIZE 2", 
                    "const uint8_t PROGMEM DSP_core_register_R4_data[CORE_REGISTER_R4_SIZE] = {"
                ])
                in_data_array = True
            
            # 数据行 - 过滤掉孤立的寄存器默认值数据
            elif line.startswith('0x'):
                if in_data_array:
                    data_lines.append(line)
                # 跳过不在数据数组中的孤立数据行（通常是寄存器默认值）
            elif line.startswith('};'):
                if in_data_array:
                    if data_lines:
                        result.extend(data_lines)
                    result.append("};")
                    result.append("")
                    data_lines = []
                    in_data_array = False
        
        # 处理最后的数据数组
        if in_data_array and data_lines:
            result.extend(data_lines)
            result.append("};")
            result.append("")
        
        return '\n'.join(result)
    
    def _get_hardware_conf_addr(self, chip_type: str) -> str:
        """获取芯片特定的硬件配置地址"""
        if chip_type == "ADAU1452":
            return "#define HARDWARE_CONF_ADDR 0xF890"
        else:
            return "#define HARDWARE_CONF_ADDR 0x081C"
    
    def _get_core_register_r0_addr(self, chip_type: str) -> str:
        """获取芯片特定的R0寄存器地址"""
        if chip_type == "ADAU1452":
            return "#define CORE_REGISTER_R0_ADDR 0xF400"
        else:
            return "#define CORE_REGISTER_R0_ADDR 0x081C"
    
    def _get_core_register_r4_addr(self, chip_type: str) -> str:
        """获取芯片特定的R4寄存器地址"""
        if chip_type == "ADAU1452":
            return "#define CORE_REGISTER_R4_ADDR 0xF402"
        else:
            return "#define CORE_REGISTER_R4_ADDR 0x081C"
    
    def _validate_file_content(self, content: str, file_type: str) -> bool:
        """验证文件内容是否有效"""
        if not content or len(content.strip()) < 10:
            self.errors.append(f"Invalid or empty {file_type}")
            print(f"❌ Invalid or empty {file_type}")
            return False
        
        # 检查是否包含必要的SigmaStudio标识
        if "#define" not in content:
            self.errors.append(f"{file_type} doesn't contain expected #define statements")
            print(f"❌ {file_type} doesn't appear to be a valid SigmaStudio export file")
            return False
        
        return True
    
    def _validate_generated_content(self, content: str) -> bool:
        """验证生成的内容"""
        required_sections = [
            "#ifndef SIGMADSP_PARAMETERS_",
            "#define SIGMADSP_PARAMETERS_", 
            "#include <SigmaDSP.h>"
        ]
        
        for section in required_sections:
            if section not in content:
                self.warnings.append(f"Generated file missing expected section: {section}")
                print(f"⚠️  Generated file missing: {section}")
        
        return True
    
    def generate_header(self, project_name: str, chip_type: str) -> str:
        """生成文件头部"""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
        # 生成唯一的头文件保护符
        guard_name = f"SIGMADSP_PARAMETERS_{project_name.upper().replace('-', '_').replace(' ', '_')}_H"
        
        return f"""/*
 * SigmaDSP Parameter File
 * Generated by SigmaDSP Parameter Generator (Python Edition)
 * 
 * Project: {project_name}
 * Chip: {chip_type}
 * Generated: {timestamp}
 * 
 * This file contains DSP program data, parameters, and initialization functions
 * for use with the SigmaDSP Arduino library.
 */

#ifndef {guard_name}
#define {guard_name}

#include <SigmaDSP.h>

"""
    
    def generate_footer(self) -> str:
        """生成文件结尾"""
        return "#endif"
    
    def generate(self) -> bool:
        """主生成函数"""
        print("\n" + "="*50)
        print("   SIGMADSP PYTHON PARAMETER GENERATOR    ")
        print("            Created by MCUdude              ")
        print("    https://github.com/MCUdude/SigmaDSP     ")
        print("="*50 + "\n")
        
        # 获取项目名称
        project_name = self.get_project_name()
        print(f"Project name: {project_name}")
        
        # 查找必需文件
        dsp_param_file = self.find_file(f"*_IC_1_PARAM.h", required=True)
        dsp_program_file = self.find_file(f"*_IC_1.h", required=True)
        
        # 查找可选文件
        eeprom_program_file = self.find_file(f"*_IC_2.h", required=False)
        eeprom_hex_file = self.find_file("*rom.hex", required=False)
        
        # 检查错误
        if self.errors:
            print("\n❌ 错误:")
            for error in self.errors:
                print(f"  - {error}")
            return False
        
        # 显示警告
        if self.warnings:
            print("\n⚠️  警告:")
            for warning in self.warnings:
                print(f"  - {warning}")
            print("  EEPROM功能将不可用")
        
        print(f"\n📝 生成文件: {self.output_file}")
        
        # 读取文件内容
        if not dsp_param_file or not dsp_program_file:
            print("❌ 缺少必需的文件")
            return False
        
        try:
            dsp_param_content = dsp_param_file.read_text(encoding='utf-8', errors='ignore')
            dsp_program_content = dsp_program_file.read_text(encoding='utf-8', errors='ignore')
            
            # 验证文件内容
            if not self._validate_file_content(dsp_param_content, "parameter file"):
                return False
            if not self._validate_file_content(dsp_program_content, "program file"):
                return False
                
        except Exception as e:
            print(f"❌ 读取文件失败: {e}")
            return False
        
        eeprom_program_content = ""
        if eeprom_program_file:
            try:
                eeprom_program_content = eeprom_program_file.read_text(encoding='utf-8', errors='ignore')
            except Exception as e:
                print(f"⚠️  无法读取EEPROM程序文件: {e}")
        
        eeprom_hex_content = ""
        if eeprom_hex_file:
            try:
                eeprom_hex_content = eeprom_hex_file.read_text(encoding='utf-8', errors='ignore').strip()
            except Exception as e:
                print(f"⚠️  无法读取EEPROM hex文件: {e}")
        
        # 检测芯片类型
        chip_type, chip_comment = self.detect_chip_type(dsp_param_content, dsp_program_content)
        
        # 生成文件内容
        sections = []
        
        # 文件头
        sections.append(self.generate_header(project_name, chip_type))
        
        # I2C地址
        dsp_addr, eeprom_addr = self.extract_i2c_addresses(dsp_program_content, eeprom_program_content, chip_type)
        sections.append("/* 7-bit i2c addresses */")
        if dsp_addr:
            sections.append(dsp_addr)
        if eeprom_addr:
            sections.append(eeprom_addr)
        sections.append("")
        
        # 添加readout宏定义
        sections.append("// Define readout macro as empty")
        sections.append("#define SIGMASTUDIOTYPE_SPECIAL(x) (x)")
        sections.append("")
        
        # DSP参数
        parameters = self.extract_parameters(dsp_param_content)
        if parameters:
            sections.append(parameters)
            sections.append("")
        
        # EEPROM固件数组
        if eeprom_hex_content:
            hex_data = self.process_hex_file(eeprom_hex_content)
            if hex_data:
                sections.append(hex_data)
                sections.append("")
        
        # DSP程序数据
        program_data = self.extract_program_data(dsp_program_content, chip_type)
        if program_data:
            sections.append(program_data)
        
        # RAM数据
        ram_data = self.extract_ram_data(dsp_program_content)
        if ram_data:
            sections.append(ram_data)
        
        # 文件结尾
        sections.append(self.generate_footer())
        
        # 写入文件，处理格式问题
        try:
            content = '\n'.join(sections)
            
            # 清理和格式化内容
            content = self._clean_generated_content(content)
            
            # 验证生成的内容
            if not self._validate_generated_content(content):
                print("⚠️  生成的文件可能不完整，但仍会保存")
            
            # 确保输出目录存在且可写
            self.output_file.parent.mkdir(parents=True, exist_ok=True)
            
            self.output_file.write_text(content, encoding='utf-8')
            print("✅ 生成成功!")
            print(f"✅ 输出文件: {self.output_file}")
            print(f"✅ 文件大小: {len(content)} 字符")
            return True
        except PermissionError:
            print(f"❌ 权限错误: 无法写入文件 {self.output_file}")
            print("   请检查文件权限或以管理员身份运行")
            return False
        except Exception as e:
            print(f"❌ 写入文件失败: {e}")
            return False
    
    def _clean_generated_content(self, content: str) -> str:
        """清理和格式化生成的内容"""
        # 修复常见的格式问题
        content = content.replace(",}", "}")
        content = content.replace(",,", ",")
        
        # 移除多余的空行
        lines = content.split('\n')
        cleaned_lines = []
        prev_empty = False
        
        for line in lines:
            is_empty = not line.strip()
            if is_empty and prev_empty:
                continue  # 跳过连续的空行
            cleaned_lines.append(line)
            prev_empty = is_empty
        
        return '\n'.join(cleaned_lines)
 
 
def main():
    parser = argparse.ArgumentParser(
        description="SigmaDSP Parameter File Generator (Python Edition)"
    )
    parser.add_argument(
        "-i", "--input", 
        default=".", 
        help="SigmaStudio项目输入目录路径 (默认: 当前目录)"
    )
    parser.add_argument(
        "-o", "--output", 
        help="输出目录路径 (默认: 与输入目录相同)"
    )
    parser.add_argument(
        "-v", "--verbose", 
        action="store_true", 
        help="详细输出"
    )
    
    # 保持向后兼容性
    parser.add_argument(
        "-d", "--directory", 
        help="项目目录路径 (已弃用，请使用 -i)"
    )
    
    args = parser.parse_args()
    
    # 处理向后兼容性
    input_dir = args.directory if args.directory else args.input
    output_dir = args.output
    
    if not os.path.exists(input_dir):
        print(f"❌ 输入目录不存在: {input_dir}")
        sys.exit(1)
    
    if args.verbose:
        print(f"📁 输入目录: {os.path.abspath(input_dir)}")
        print(f"📁 输出目录: {os.path.abspath(output_dir) if output_dir else '与输入目录相同'}")
    
    generator = SigmaDSPGenerator(input_dir, output_dir)
    success = generator.generate()
    
    if not success:
        sys.exit(1)
 
 
if __name__ == "__main__":
    main()