# RK3588 边缘智能视频监测服务

基于 **OpenCV + GStreamer + RKNN** 的单路摄像头视频采集、YOLOv8 目标检测、RTSP 推流与事件上报程序，运行于 **RK3588 开发板**，利用 MPP 硬件 H.264 编码实现高效推流，利用 RKNPU2 进行 AI 推理。

---

## 功能特性

- **MJPEG 摄像头采集** — 基于 V4L2 + GStreamer 的 MJPEG 格式视频捕获，解决 OpenCV 直读 Bus error 问题
- **YOLOv8 目标检测** — 基于 RKNPU2 的 RKNN 模型推理，支持多线程异步检测（submit/retrieve 模式），画面实时绘制检测框与标签
- **RTSP 硬件编码推流** — 利用 RK3588 MPP 硬件 H.264 编码器（`mpph264enc`），8Mbps 码率推送至 MediaMTX 服务器
- **FPS 实时叠加** — 画面右上角绿色显示当前帧率
- **事件触发判定机制** — 基于滑动窗口的目标检测触发：首次检测到目标启动窗口，窗口内累计帧数达标后 success_count+1，达到 trigger_count 后触发上报
- **MQTT 状态上报** — 周期性心跳上报设备状态 + 检测触发时即时上报检测结果
- **HTTP 图片上传** — 触发上报时同时将带检测框的抓拍图片通过 multipart/form-data POST 到后端服务
- **YAML 配置驱动** — 所有参数（摄像头、模型、MQTT、触发规则、HTTP 地址等）通过 `config.yaml` 配置，无需重新编译

---

## 技术栈

| 组件 | 技术 |
|------|------|
| **语言/标准** | C++17 |
| **构建系统** | CMake 3.4.1+ |
| **视频采集** | OpenCV 4.x + GStreamer (v4l2src, jpegdec, appsink) |
| **视频编码/推流** | GStreamer (appsrc, mpph264enc, rtspclientsink) |
| **硬件加速** | RK3588 MPP H.264 编码 + RKNPU2 AI 推理 |
| **AI 推理** | RKNN Toolkit (YOLOv8 RKNN 模型) |
| **消息队列** | Eclipse Paho MQTT C/C++ |
| **HTTP 上传** | libcurl (multipart/form-data) |
| **配置文件** | yaml-cpp |
| **线程模型** | std::thread + std::atomic + mutex |

---

## 项目结构

```
rk3588lyh/
├── CMakeLists.txt                  # CMake 构建配置
├── config.yaml                     # 运行时配置文件
├── README.md                       # 本文档
│
├── model/
│   ├── yolov8s.rknn                # YOLOv8 RKNN 模型文件
│   └── coco_80_labels_list.txt     # 类别标签文件（每行一个类别名）
│
├── install/
│   └── lib/
│       ├── librknnrt.so            # RKNPU2 运行时库
│       └── librga.so               # RGA 图形加速库
│
├── 3rdparty/                       # 第三方依赖源码
│   ├── thread_pool/                # 线程池
│   ├── opencl/                     # OpenCL stub
│   └── librga/                     # RGA 库源码
│
├── src/
│   ├── main.cc                     # 主程序入口（加载配置、初始化各模块、启动线程）
│   ├── camera_status.h / .cc       # CameraStatus 结构体（摄像头状态、检测报告队列、线程间通信）
│   ├── capture_thread.h / .cc      # CaptureThread 视频采集线程（采集→推理→绘制→推流→触发判定）
│   ├── publisher_thread.h / .cc    # PublisherThread 发布线程（MQTT 心跳、MQTT 检测上报、HTTP 图片上传）
│   ├── yolo_detector.h / .cc       # YoloDetector 封装类（RKNN 模型初始化、异步 submit/retrieve）
│   ├── global_running.h            # 全局信号控制（SIGINT/SIGTERM 优雅退出）
│   ├── postprocess.h / .cc         # YOLOv8 后处理（NMS、坐标还原、绘制检测框、类别标签映射）
│   ├── common_types.h              # 通用类型定义（rknn_app_context_t、object_detect_result 等）
│   └── rknpu2/
│       └── yolov8.h / .cc          # RKNN YOLOv8 推理核心实现
│
├── utils/
│   ├── image_convert.c / .h        # RGA 图像格式转换 + letterbox 预处理
│   ├── file_utils.c / .h           # 文件读取工具
│   └── CMakeLists.txt              # utils 子目录构建
│
└── build/                          # 编译输出目录
```

---

## 快速开始

### 1. 系统依赖

确保 RK3588 开发板已安装以下依赖：

```bash
# OpenCV (需支持 GStreamer 后端)
sudo apt install libopencv-dev

# GStreamer 1.0 + 插件（含 RK MPP 编码器）
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
                 gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
                 gstreamer1.0-plugins-bad gstreamer1.0-tools

# yaml-cpp
sudo apt install libyaml-cpp-dev

# libcurl
sudo apt install libcurl4-openssl-dev

# Paho MQTT C/C++ 库
git clone https://github.com/eclipse/paho.mqtt.c.git && cd paho.mqtt.c && make && sudo make install
git clone https://github.com/eclipse/paho.mqtt.cpp.git && cd paho.mqtt.cpp && mkdir build && cd build && cmake .. && make && sudo make install
sudo ldconfig
```

### 2. 编译

```bash
cd rk3588lyh/build
cmake ..
make
```

### 3. 配置

编辑 `~/lyh/rk3588lyh/config.yaml`（程序通过 `$HOME/lyh/rk3588lyh/config.yaml` 路径加载配置）：

```yaml
device:
  device_id: "rk3588lyh"

mqtt:
  server: "10.60.83.159:1883"
  user_name: "user"
  password: "520heweijie"
  heart_topic: "rk3588lyh/info"
  heart_rate: 1000

camera:
  id: "001"
  location: "前门大院"
  rtsp_url: "rtsp://10.60.83.159:8554/rk3588lyh"
  http_url: "http://10.60.83.159:8889/rk3588lyh"
  device: "/dev/video0"
  width: 1920
  height: 1080
  fps: 25
  use_yolo: true
  detector:
    window_seconds: 8
    target_class_id: 0
    frame_threshold: 20
    trigger_count: 3
    model_path: "/home/cat/lyh/rk3588lyh/model/yolov8s.rknn"
    mqtt_report_topic: "rk3588lyh/001/report"
    http_report_url: "http://10.60.83.159:5000/capture/upload"
    confidence_threshold: 0.50
    nms_threshold: 0.45
    thread_num: 3
```

### 4. 运行

```bash
./build/main
```

按 `Ctrl+C` 优雅退出。

---

## 数据流

```
[ USB/CSI 摄像头 ]
       │
       ▼ V4L2 MJPEG 采集
[ OpenCV VideoCapture (GStreamer Pipeline) ]
       │
       ▼ cv::Mat (BGR)
[ YoloDetector::submit() ]  ──异步──▶  [ RKNPU2 推理线程池 ]
       │                                      │
       │         ◀──异步──  YoloDetector::retrieve(annotated_frame, results)
       │
       ├──▶ draw_results() + DrawFpsOverlay()
       │         │
       │         ├──▶ cv::VideoWriter (GStreamer Pipeline) ──▶ RTSP 推流
       │         │
       │         └──▶ 触发判定逻辑
       │                  │
       │                  ├──▶ CameraStatus::QueueDetectionReport()
       │                  │         │
       │                  │         ▼
       │                  │    [ PublisherThread ]
       │                  │         ├──▶ MQTT 检测上报
       │                  │         └──▶ HTTP POST 图片上传
       │                  │
       │                  └──▶ 窗口重置
       │
       └──▶ MQTT 心跳 (周期性)
```

### GStreamer Pipeline

**读取摄像头 (MJPEG)**:
```
v4l2src device=/dev/video0 !
image/jpeg, width=1920, height=1080, framerate=25/1 !
jpegdec !
videoconvert ! video/x-raw, format=BGR !
appsink drop=1 max-buffers=1 sync=false
```

**推流 (RK3588 硬件编码)**:
```
appsrc !
videoconvert !
video/x-raw,format=NV12,width=1920,height=1080,framerate=25/1 !
mpph264enc bps=8000000 !
h264parse !
rtspclientsink location=rtsp://10.60.83.159:8554/rk3588lyh
```

---

## 配置参数说明

### 设备配置 (`device:`)

| 参数 | 类型 | 说明 |
|------|------|------|
| `device_id` | string | 设备唯一标识，用于 MQTT 消息中的设备标识 |

### MQTT 配置 (`mqtt:`)

| 参数 | 类型 | 说明 |
|------|------|------|
| `server` | string | MQTT Broker 地址与端口（如 `10.60.83.159:1883`） |
| `user_name` | string | MQTT 认证用户名 |
| `password` | string | MQTT 认证密码 |
| `heart_topic` | string | 设备心跳上报主题 |
| `heart_rate` | int | 心跳间隔（毫秒） |

### 摄像头配置 (`camera:`)

| 参数 | 类型 | 说明 |
|------|------|------|
| `id` | string | 摄像头编号（如 `"001"`），用于 MQTT 和 HTTP 上报 |
| `location` | string | 安装位置描述（如 `"前门大院"`），用于 HTTP 上报 |
| `device` | string | V4L2 设备路径（如 `/dev/video0`） |
| `width` / `height` | int | 采集分辨率 |
| `fps` | int | 采集帧率 |
| `rtsp_url` | string | RTSP 推流目标地址 |
| `http_url` | string | HTTP 视频流地址 |
| `use_yolo` | bool | 是否启用 YOLOv8 推理 |

### 检测器配置 (`camera.detector:`)

| 参数 | 类型 | 说明 |
|------|------|------|
| `window_seconds` | int | 触发判定窗口时长（秒），超时后窗口内计数清零 |
| `target_class_id` | int | 目标检测类别 ID，对应 `coco_80_labels_list.txt` 中的行号（0 起始） |
| `frame_threshold` | int | 窗口内连续检测到目标的帧数阈值，达到后 success_count +1 |
| `trigger_count` | int | success_count 达到此值时触发上报 |
| `model_path` | string | RKNN 模型文件绝对路径 |
| `mqtt_report_topic` | string | 检测结果 MQTT 上报主题 |
| `http_report_url` | string | 检测结果 HTTP 图片上传 URL（可选，不配置则不上传） |
| `confidence_threshold` | float | 检测置信度阈值 |
| `nms_threshold` | float | NMS 非极大值抑制阈值 |
| `thread_num` | int | RKNN 推理线程数 |

---

## 事件触发判定机制

核心逻辑在 `capture_thread.cc` 中，基于滑动窗口实现：

```
检测到目标帧
      │
      ▼
[窗口未激活?] ──是──▶ 启动窗口，match_frames=1, success_count=0
      │否
      ▼
[窗口内继续检测] ── match_frames++
      │
      ▼
[match_frames >= frame_threshold?] ──是──▶ success_count++, stage_match_frames 清零
      │
      ▼
[success_count >= trigger_count?] ──是──▶ 触发上报！关闭窗口，重置所有计数
      │
      ▼
[窗口超时 (window_seconds)?] ──是──▶ 重置所有计数
```

**示例**（默认配置 `window_seconds=8, frame_threshold=20, trigger_count=3`）：

1. 检测到 `target_class_id=0`（falldown）→ 启动 8 秒窗口
2. 窗口内累计 20 帧检测到目标 → `success_count=1`
3. 再累计 20 帧 → `success_count=2`
4. 再累计 20 帧 → `success_count=3` → **触发上报**
5. 若 8 秒内未达到 `trigger_count=3` → 窗口过期，计数清零

---

## 上报消息格式

### MQTT 心跳消息

主题：`rk3588lyh/info`（由 `heart_topic` 配置）

```json
{
  "device_id": "rk3588lyh",
  "timestamp_ns": 1713849600000000000,
  "camera": {
    "id": "001",
    "location": "前门大院",
    "http_url": "http://10.60.83.159:8889/rk3588lyh",
    "rtsp_url": "rtsp://10.60.83.159:8554/rk3588lyh",
    "resolution": {
      "width": 1920,
      "height": 1080,
      "fps": 25.0
    }
  }
}
```

### MQTT 检测上报消息

主题：`rk3588lyh/001/report`（由 `mqtt_report_topic` 配置）

```json
{
  "device_id": "rk3588lyh",
  "camera_id": "001",
  "timestamp_ns": 1713849600000000000,
  "detection": "falldown",
  "count": 2,
  "window_match_frames": 60,
  "trigger_success_count": 3
}
```

### HTTP 图片上传

URL：`http://10.60.83.159:5000/capture/upload`（由 `http_report_url` 配置）

请求方式：`POST multipart/form-data`

| 字段 | 类型 | 说明 |
|------|------|------|
| `file` | File | 抓拍图片（JPEG 格式，带检测框标注） |
| `camera_id` | String | 摄像头 ID（如 `"001"`） |
| `location` | String | 安装位置（如 `"前门大院"`） |
| `violation_type` | String | 告警类型，由 `target_class_id` 映射到 `coco_80_labels_list.txt` 中的类别名（如 `"falldown"`） |

HTTP 请求超时时间：5 秒。超时或失败会在终端打印日志。

---

## 架构说明

### 线程模型

```
main()
├── CaptureThread     (视频采集 + YOLO 推理 + 绘制 + 推流 + 触发判定)
└── PublisherThread   (MQTT 心跳 + MQTT 检测上报 + HTTP 图片上传)
```

- **主线程**：加载配置、初始化 YOLO 模型、创建线程、等待退出信号
- **CaptureThread**：循环采集帧 → submit 到推理线程池 → retrieve 结果 → 绘制检测框/FPS → 推流 → 触发判定逻辑
- **PublisherThread**：按心跳间隔周期性发布状态，同时消费 `CameraStatus` 中的待上报队列，执行 MQTT 发布和 HTTP 上传
- **信号处理**：`SIGINT`/`SIGTERM` → `g_running = false` → 优雅停止所有线程

### 线程间通信

```
CaptureThread                          PublisherThread
      │                                      │
      │  status_.QueueDetectionReport()      │
      │─────────────────────────────────────▶│
      │  (results, class_id, jpeg_data)      │
      │                                      │
      │                                      │  status_.ConsumeDetectionReport()
      │                                      │──────────────▶ MQTT publish
      │                                      │──────────────▶ HTTP POST
```

通过 `CameraStatus` 中的 mutex 保护的生产者-消费者模型：
- `QueueDetectionReport()` — CaptureThread 写入检测结果 + JPEG 数据，设置 `has_pending_report` 标志
- `ConsumeDetectionReport()` — PublisherThread 消费并清空，返回是否有待处理数据

### YOLOv8 异步推理

`YoloDetector` 使用 submit/retrieve 模式：
- `submit(frame)` — 将帧提交到内部线程池，不阻塞
- `retrieve(annotated_frame, results)` — 非阻塞取回最新推理结果
- 推理线程数由 `thread_num` 配置，利用 RKNN 的多核并行能力

---

## 终端日志说明

程序运行时终端会输出以下关键日志：

| 前缀 | 说明 | 示例 |
|------|------|------|
| `[Trigger] window started` | 触发窗口启动 | `target_class_id=0 window=8s elapsed=0s total_match_frames=1` |
| `[Trigger] window expired` | 窗口超时重置 | `elapsed=8s success_count=2/3 => reset` |
| `[Trigger] target_class_id=...` | frame_threshold 达标 | `stage_match_frames=20 >= threshold=20 => success_count=1/3` |
| `[Trigger] MQTT report queued` | 触发上报入队 | `jpeg_size=84532 bytes => reached trigger_count=3` |
| `[HTTP] Report sent` | HTTP 上传成功 | `violation_type=falldown camera_id=001 response_code=200` |
| `[HTTP] Request timeout` | HTTP 上传超时 | `Request timeout (5s) to http://...` |
| `[HTTP] Request failed` | HTTP 上传其他错误 | `Request failed: Couldn't resolve host` |
| `MQTT 连接成功` | MQTT 连接建立 | `心跳主题: rk3588lyh/info` |
| `YOLOv8 模型加载成功` | 模型初始化完成 | `yolov8s.rknn, 3 线程` |

---

## 类别标签文件

`model/coco_80_labels_list.txt` 定义了检测类别，每行一个类别名，行号即为 `target_class_id`：

```
falldown
```

当前模型为跌倒检测专用模型，仅包含 1 个类别。如需更换模型，替换 `.rknn` 文件和对应的标签文件即可。

---

## 常见问题

**Q: 编译时提示找不到 MQTT 库？**
A: 确保已编译安装 paho-mqtt3a/paho-mqtt3c 和 paho-mqttpp3 库，并运行 `sudo ldconfig` 刷新动态链接缓存。

**Q: 编译时提示 `libcurl not found`？**
A: 安装开发包：`sudo apt install libcurl4-openssl-dev`

**Q: 编译时提示找不到 GStreamer 头文件？**
A: 安装 `libgstreamer1.0-dev` 和 `libgstreamer-plugins-base1.0-dev` 包。

**Q: 运行时提示 `/dev/video0` 不存在？**
A: 检查摄像头是否正确连接，执行 `ls -la /dev/video*` 确认可用设备节点。可能需要调整权限：`sudo chmod 666 /dev/video0`。

**Q: 运行时 Bus error (core dumped)？**
A: 这是 OpenCV GStreamer 后端与 `libturbojpeg` 静态库冲突导致。本项目已通过将 JPEG 编解码代码从主程序中剥离来规避此问题，确保不链接 `libturbojpeg.a`。

**Q: RTSP 推流连接失败？**
A: 确保 MediaMTX 服务已在目标服务器上运行，且 `rtsp_url` 中的地址和端口正确。默认 MediaMTX RTSP 端口为 `8554`。

**Q: MQTT 上报主题收不到消息？**
A: 检查：(1) `mqtt_report_topic` 配置是否正确；(2) 触发条件是否满足（`frame_threshold` 和 `trigger_count` 不要设置过高）；(3) MQTT Broker 是否可达。

**Q: HTTP 上传图片没有检测框？**
A: 已修复。图片在 `draw_results()` 绘制检测框之后才编码为 JPEG 上传。

**Q: 摄像头画面卡顿或掉帧？**
A: 可适当降低 `fps` 或 `width/height`；确认 MPP 硬件编码器已正确加载（`lsmod | grep mpp`）。

---

## 许可证

本项目仅供学习与研究使用。
