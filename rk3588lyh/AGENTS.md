# AGENTS.md - RK3588 RTSP 推流程序

## 项目概述

基于 OpenCV + GStreamer 的摄像头视频流推送程序，运行于 RK3588 开发板。

---

## 1. 项目结构

```
rk3588/
├── CMakeLists.txt      # CMake 构建配置
├── src/
│   ├── main.cc        # 主程序
│   └── before.cc      # (备用/测试代码)
└── build/              # 编译输出目录
```

---

## 2. 构建/运行命令

### 编译
```bash
cd rk3588/build
cmake ..
make
```

### 运行
```bash
./build/main
```

### 清理构建
```bash
rm -rf build/* && cd build && cmake .. && make
```

---

## 3. 代码风格指南

### 编译标准
- C++17 (`-std=c++17`)
- 启用 pthread

### 命名规范
- 文件: `snake_case.cc`
- 类/函数: `PascalCase`
- 变量: `snake_case`
- 常量: `kPascalCase` (枚举) 或 `UPPER_SNAKE_CASE`

### 代码风格
- 大括号独立一行
- 使用 `std::` 而非 `using namespace std`
- 优先使用 `const` 引用传参
- 智能指针管理资源 (`std::unique_ptr`)

### 警告配置 (VSCode)
```json
"-Wall", "-Wextra", "-Wpedantic", "-Wshadow",
"-Wformat=2", "-Wcast-align", "-Wconversion",
"-Wsign-conversion", "-Wnull-dereference"
```

---

## 4. 技术栈

- OpenCV
- GStreamer (v4l2src, mpph264enc, rtspclientsink)
- CMake 3.4.1+

---

## 5. GStreamer Pipeline 说明

### 读取摄像头 (MJPEG)
```
v4l2src device=/dev/video0 ! 
image/jpeg, width=1920, height=1080, framerate=30/1 ! 
jpegdec ! 
videoconvert ! video/x-raw, format=BGR ! 
appsink
```

### 推流 (RK3588 硬件编码)
```
appsrc ! 
videoconvert ! 
video/x-raw,format=NV12,width=1920,height=1080,framerate=30/1 ! 
mpph264enc bps=8000000 ! 
h264parse ! 
rtspclientsink location=rtsp://fnas:8554/rk3588
```

---

## 6. 验证步骤

```bash
cd rk3588/build && cmake .. && make
```
