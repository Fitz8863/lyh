# RK3588 边缘视频采集与推流服务

基于 **OpenCV + GStreamer** 的多路摄像头视频采集与 RTSP 推流程序，专为 **RK3588 开发板**设计，利用 MPP 硬件 H.264 编码实现高效视频推流。

> **配套组件**：本程序是「校园安防智能监测系统」的边缘端组件，需搭配 [`web/`](../web) Flask 管理端使用。本端负责采集和推流，Web 端负责 AI 推理、告警管理与设备控制。

---

## 🌟 功能特性

- **📷 多路摄像头并发采集** — 基于 V4L2 的 MJPEG 格式视频捕获，支持同时接入多个 USB/CSI 摄像头
- **🎬 GStreamer 硬件编码推流** — 利用 RK3588 MPP 硬件 H.264 编码器 (`mpph264enc`)，通过 `rtspclientsink` 推送至 MediaMTX 服务器
- **📡 MQTT 状态上报** — 周期性向 Broker 发布设备与各摄像头运行状态
- **📖 YAML 配置驱动** — 所有摄像头参数（设备路径、分辨率、帧率、推流地址等）通过 `config.yaml` 配置，无需重新编译
- **🔧 摄像头管理器架构** — CameraManager 统一管理多个摄像头的采集线程与状态，支持优雅启停

---

## 🛠️ 技术栈

| 组件 | 技术 |
|------|------|
| **语言/标准** | C++17 |
| **构建系统** | CMake 3.4.1+ |
| **视频采集** | OpenCV (V4L2 VideoCapture) |
| **视频编解码/推流** | GStreamer 1.0 (v4l2src, mpph264enc, rtspclientsink) |
| **硬件加速** | RK3588 MPP H.264 编码 |
| **消息队列** | Eclipse Paho MQTT (C/C++ 库) |
| **配置文件** | yaml-cpp |
| **线程模型** | std::thread + std::atomic 信号控制 |

---

## 📁 项目结构

```
rk3588/
├── CMakeLists.txt              # CMake 构建配置（C++17, pthread, OpenCV, GStreamer, MQTT, yaml-cpp）
├── config.yaml                 # 设备与摄像头 YAML 配置
├── README.md                   # 本文档
│
├── src/
│   ├── main.cc                 # 主程序入口（加载配置、初始化摄像头、启动 MQTT 发布器）
│   ├── camera_status.h / .cc   # 摄像头运行状态结构体
│   ├── capture_thread.h / .cc  # 视频采集线程（V4L2 → OpenCV → GStreamer 推流）
│   ├── publisher_thread.h / .cc# MQTT 状态发布线程（周期性上报摄像头状态）
│   ├── global_running.h        # 全局信号控制（SIGINT/SIGTERM 优雅退出）
│   └── before.cc               # （备用测试代码）
│
└── build/                      # CMake 编译输出目录
```

---

## 🚀 快速开始

### 1. 系统依赖

确保 RK3588 开发板已安装以下依赖：

```bash
# OpenCV
sudo apt install libopencv-dev

# GStreamer 1.0 + 插件（含 RK MPP 编码器）
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
                 gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
                 gstreamer1.0-plugins-bad gstreamer1.0-tools

# yaml-cpp
sudo apt install libyaml-cpp-dev

# Paho MQTT C/C++ 库
git clone https://github.com/eclipse/paho.mqtt.c.git && cd paho.mqtt.c && make && sudo make install
git clone https://github.com/eclipse/paho.mqtt.cpp.git && cd paho.mqtt.cpp && mkdir build && cd build && cmake .. && make && sudo make install
sudo ldconfig
```

### 2. 编译

```bash
cd rk3588/build
cmake ..
make
```

### 3. 配置

编辑 `config.yaml`：

```yaml
device:
  device_id: "RK3588"          # 设备唯一标识（需与 Web 端 REGISTERED_DEVICES 列表一致）

mqtt:
  server: "192.168.1.100:1883" # MQTT Broker 地址
  topic: "RK3588/info"         # 状态上报主题
  publish_interval: 1          # 上报间隔（秒）

camera:
  - id: "001"                  # 摄像头编号
    location: "校园大门"        # 安装位置
    rtsp_url: "rtsp://server:8554/rk3588"          # RTSP 推流地址（MediaMTX）
    http_url: "http://server:8889/rk3588"          # HTTP/WHIP 推流地址
    device: "/dev/video0"      # V4L2 设备路径
    width: 1920                # 分辨率宽
    height: 1080               # 分辨率高
    fps: 30                    # 帧率
```

> **提示**：可取消注释 `config.yaml` 中第二个摄像头的配置段，即可启用双路并发。

### 4. 运行

```bash
./build/main
```

按 `Ctrl+C` 优雅退出。

---

## 🔄 数据流

```
[ USB/CSI 摄像头 ]
       │
       ▼ V4L2 采集 (MJPEG)
[ OpenCV VideoCapture ]
       │
       ▼ jpegdec → videoconvert → video/x-raw,BGR
[ GStreamer Pipeline (读取) ]
       │
       ▼ appsrc → videoconvert → NV12
[ mpph264enc (RK3588 硬件编码) ]
       │
       ▼ h264parse → rtspclientsink
[ MediaMTX 服务器 ]
       │
       ▼ RTSP/WebRTC
[ Web 管理端播放 ]
```

### GStreamer Pipeline 详解

**读取摄像头 (MJPEG)**:
```
v4l2src device=/dev/video0 !
image/jpeg, width=1920, height=1080, framerate=30/1 !
jpegdec !
videoconvert ! video/x-raw, format=BGR !
appsink
```

**推流 (RK3588 硬件编码)**:
```
appsrc !
videoconvert !
video/x-raw,format=NV12,width=1920,height=1080,framerate=30/1 !
mpph266enc bps=8000000 !
h264parse !
rtspclientsink location=rtsp://server:8554/rk3588
```

---

## ⚙️ 配置参数

### 设备配置 (`device:`)

| 参数 | 说明 |
|------|------|
| `device_id` | 设备唯一标识，必须与 Web 端 `config.py` 中 `REGISTERED_DEVICES` 列表中的一致 |

### MQTT 配置 (`mqtt:`)

| 参数 | 说明 |
|------|------|
| `server` | MQTT Broker 地址与端口（如 `192.168.1.100:1883`） |
| `topic` | 设备状态上报主题 |
| `publish_interval` | 状态发布间隔（秒） |

### 摄像头配置 (`camera[]:`)

| 参数 | 说明 |
|------|------|
| `id` | 摄像头编号（如 `"001"`） |
| `location` | 安装位置描述 |
| `device` | V4L2 设备路径（如 `/dev/video0`） |
| `width` / `height` | 采集分辨率 |
| `fps` | 采集帧率 |
| `rtsp_url` | 视频 RTSP 推流目标地址 |
| `http_url` | 视频 HTTP/WHIP 推流目标地址 |


---

## 🏗️ 架构说明

### 线程模型

```
main()
├── CaptureThread (视频采集 + GStreamer 推流)
└── PublisherThread (MQTT 状态上报)
```

- **主线程**：加载配置、创建摄像头状态和采集线程、启动发布器、等待退出信号
- **CaptureThread**：独立线程，OpenCV 读取摄像头 → GStreamer 硬件编码 → RTSP 推流
- **PublisherThread**：独立线程，按 `publish_interval` 周期性发布摄像头状态到 MQTT Broker
- **信号处理**：通过 `SIGINT`/`SIGTERM` 设置 `g_running = false`，优雅停止所有线程

### CameraManager

统一管理多个摄像头实例的生命周期：
- `AddCamera()` — 注册新摄像头，自动创建采集线程
- `GetAllStatuses()` — 获取所有摄像头的运行状态引用
- `StopAll()` — 优雅停止所有采集线程

---

## 🔧 常见问题

**Q: 编译时提示找不到 MQTT 库？**  
A: 确保已编译安装 paho-mqtt3a/paho-mqtt3c 和 paho-mqttpp3 库，并运行 `sudo ldconfig` 刷新动态链接缓存。

**Q: 编译时提示找不到 GStreamer 头文件？**  
A: 安装 `libgstreamer1.0-dev` 和 `libgstreamer-plugins-base1.0-dev` 包。

**Q: 运行时提示 `/dev/video0` 不存在？**  
A: 检查摄像头是否正确连接，执行 `ls -la /dev/video*` 确认可用设备节点。可能需要调整权限：`sudo chmod 666 /dev/video0`。

**Q: RTSP 推流连接失败？**  
A: 确保 MediaMTX 服务已在目标服务器上运行，且 `rtsp_url` 中的地址和端口正确。默认 MediaMTX RTSP 端口为 `8554`。

**Q: 摄像头画面卡顿或掉帧？**  
A: 可适当降低 `fps` 或 `width/height`；确认 MPP 硬件编码器已正确加载（`lsmod | grep mpp`）。

---

## 📄 许可证

本项目仅供学习与研究使用。
