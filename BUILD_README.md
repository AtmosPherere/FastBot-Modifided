# Fastbot 编译和部署脚本使用说明

## 概述

本项目提供了两个自动化脚本来简化 Fastbot 的编译和部署流程：

1. **`build_and_deploy.sh`** - 完整的编译和部署脚本
2. **`quick_deploy.sh`** - 快速部署脚本（仅推送文件）

## 脚本功能

### 1. build_and_deploy.sh（完整编译部署）

**功能：** 执行完整的编译和部署流程

**包含步骤：**
1. Gradle 构建 (`./gradlew clean makeJar`)
2. DEX 转换 (`dx --dex --output=monkeyq.jar`)
3. 原生库编译 (`sh ./build_native.sh`)
4. 推送 monkeyq.jar 到设备
5. 推送 fastbot-thirdpart.jar 到设备
6. 推送原生库到设备
7. 推送 framework.jar 到设备

**使用方法：**
```bash
./build_and_deploy.sh
```

### 2. quick_deploy.sh（快速部署）

**功能：** 仅推送已编译的文件到设备，不重新编译

**包含步骤：**
1. 推送 monkeyq.jar 到设备
2. 推送 fastbot-thirdpart.jar 到设备
3. 推送原生库到设备
4. 推送 framework.jar 到设备

**使用方法：**
```bash
./quick_deploy.sh
```

## 使用场景

### 首次编译或代码有重大更改
使用完整编译脚本：
```bash
./build_and_deploy.sh
```

### 仅需要重新部署已编译的文件
使用快速部署脚本：
```bash
./quick_deploy.sh
```

## 先决条件

### 系统要求
- macOS 或 Linux 系统
- Bash shell 支持

### 必需工具
1. **Android SDK** - 确保 `adb` 命令可用
2. **Android Build Tools** - 需要 `dx` 工具（路径：`/Users/atmo/Library/Android/sdk/build-tools/30.0.2/dx`）
3. **Java Development Kit (JDK)**
4. **Android NDK** - 用于原生库编译

### 设备要求
- Android 设备已连接并启用 USB 调试
- 设备可通过 `adb devices` 检测到

## 脚本特性

### 错误处理
- 自动检查先决条件
- 每个步骤都有错误检测
- 遇到错误立即停止执行

### 日志输出
- 彩色日志输出，便于识别
- 详细的步骤信息
- 执行时间统计

### 文件检查
- 自动检查必需文件是否存在
- 验证设备连接状态
- 确保工具可用性

## 故障排除

### 常见问题

1. **"adb 未找到"**
   - 确保 Android SDK 已安装
   - 将 SDK 的 platform-tools 目录添加到 PATH

2. **"未检测到 Android 设备"**
   - 检查 USB 连接
   - 确保设备已启用 USB 调试
   - 运行 `adb devices` 确认设备状态

3. **"dx 工具未找到"**
   - 检查 Android SDK build-tools 是否已安装
   - 确认路径 `/Users/atmo/Library/Android/sdk/build-tools/30.0.2/dx` 是否正确

4. **"gradlew 未找到"**
   - 确保在项目根目录执行脚本
   - 检查项目完整性

### 调试模式

如需查看详细的执行过程，可以使用：
```bash
bash -x ./build_and_deploy.sh
```

## 文件结构

执行脚本后，文件将被推送到设备的以下位置：

```
设备文件系统：
├── /sdcard/
│   ├── monkeyq.jar
│   ├── fastbot-thirdpart.jar
│   └── framework.jar
└── /data/local/tmp/
    └── [原生库文件]
```

## 性能优化

- **首次运行**：使用 `build_and_deploy.sh`
- **后续部署**：如果只是重新部署，使用 `quick_deploy.sh` 可节省大量时间
- **并行处理**：脚本已优化文件推送顺序

## 更新日志

- **2025-07-16**: 创建初始版本
  - 实现完整编译部署脚本
  - 实现快速部署脚本
  - 添加错误处理和日志功能

## 支持

如遇到问题，请检查：
1. 所有先决条件是否满足
2. 设备连接是否正常
3. 项目文件是否完整

---

**注意：** 请确保在项目根目录执行这些脚本。
