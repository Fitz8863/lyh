# 嵌入式智能跌倒检测与远程监控系统

基于 **RK3588 边缘 AI + Flask Web + MQTT 物联网** 的智能监控系统，实现摄像头实时监测、跌倒自动检测、抓拍告警上报与远程设备管理。

---

## 项目概述

本项目是一个完整的物联网智能监控系统，包含边缘端 AI 推理、云端 Web 管理平台、嵌入式硬件终端三大模块：

```
┌─────────────────┐      RTSP 视频流       ┌─────────────────┐
│   USB 摄像头    │ ◀───────────────────── │   RK3588 边缘端  │
└─────────────────┘                        │  (跌倒检测推理)  │
                                           └────────┬────────┘
                                                    │
                                           MQTT + HTTP 上报
                                                    │
                                                    ▼
┌─────────────────┐      WebRTC 播放       ┌─────────────────┐
│   用户浏览器    │ ◀───────────────────── │   Flask Web     │
│  (监控/告警)    │ ─────────────────────▶ │   管理平台      │
└─────────────────┘      远程配置下发      └────────┬────────┘
                                                           │
                                                    ┌──────┴──────┐
                                                    │ MQTT Broker │
                                                    └──────┬──────┘
                                                           │
                                                    ┌──────┴──────┐
                                                    │ 嵌入式终端   │
                                                    │ (STM32/ESP) │
                                                    └─────────────┘
```

---

## 目录结构

```
lyh/
├── rk3588lyh/          # RK3588 边缘端程序
│   ├── src/            # 源码（采集、推理、推流、上报）
│   ├── model/          # RKNN 模型 + 类别标签
│   ├── config.yaml     # 运行时配置
│   └── README.md       # 详细文档
│
├── web/                # Flask Web 管理平台
│   ├── app.py          # 应用入口
│   ├── blueprints/     # 功能模块（认证、视频、告警、MQTT）
│   ├── static/         # 静态资源
│   ├── templates/      # Jinja2 模板
│   └── README.md       # 详细文档
│
├── convert/            # YOLOv8 模型训练与转换
│   ├── convert.py      # ONNX → RKNN 转换脚本
│   ├── export.py       # PyTorch → ONNX 导出
│   └── yolov8s_falldown_75/  # 训练产物（权重、曲线图）
│
├── cpp/                # YOLOv8 推理验证代码
│   ├── main.cc         # 独立测试入口
│   └── build/          # 编译输出
│
├── stm32/              # STM32 嵌入式工程
│   └── STM32-BISHE/    # Keil 工程（FreeRTOS + 外设驱动）
│
├── esp8266/            # ESP8266 WiFi 模块代码
│   └── esp8266-oled-mqtt-uart/  # MQTT + OLED 显示
│
├── PCB/                # PCB 硬件设计文件
│   └── STM32主控板.eprj
│
└── README.md           # 本文档
```

---

## 核心模块说明

### 1. RK3588 边缘端 (`rk3588lyh/`)

基于 RK3588 开发板的边缘 AI 推理程序：

- **视频采集**：V4L2 + GStreamer 读取 MJPEG 摄像头
- **AI 推理**：RKNPU2 加速的 YOLOv8 跌倒检测
- **RTSP 推流**：MPP 硬件 H.264 编码，8Mbps 推流
- **事件触发**：基于滑动窗口的检测判定机制
- **上报通道**：MQTT 检测消息 + HTTP 抓拍图片上传

详见 [rk3588lyh/README.md](rk3588lyh/README.md)

### 2. Flask Web 平台 (`web/`)

基于 Flask 的管理平台：

- **用户认证**：注册、登录、会话管理
- **视频监控**：多路 RTSP/WebRTC 实时播放
- **告警管理**：抓拍记录查看、数据统计
- **MQTT 集成**：与边缘设备双向通信
- **远程配置**：动态下发检测参数

详见 [web/README.md](web/README.md)

### 3. 模型训练与转换 (`convert/`)

YOLOv8 跌倒检测模型的训练与部署：

- **训练框架**：Ultralytics YOLOv8
- **导出格式**：ONNX → RKNN（INT8 量化）
- **模型文件**：`yolov8s_falldown_75/weights/best_int8.rknn`

### 4. 嵌入式终端 (`stm32/` + `esp8266/`)

辅助硬件模块：

- **STM32**：FreeRTOS 任务框架，OLED 显示、舵机控制、串口通信
- **ESP8266**：WiFi 连接、MQTT 订阅、OLED 状态显示

---

## 快速开始

### 硬件要求

- RK3588 开发板（含 RKNPU2）
- USB 摄像头（支持 MJPEG 格式）
- MQTT Broker（如 Mosquitto）
- MediaMTX（RTSP 服务器）
- MySQL 数据库

### 运行流程

1. **启动 MQTT Broker 和 MediaMTX**
   ```bash
   mosquitto -c /etc/mosquitto/mosquitto.conf
   mediamtx
   ```

2. **启动 RK3588 边缘端**
   ```bash
   cd rk3588lyh/build && ./main
   ```

3. **启动 Flask Web 平台**
   ```bash
   cd web && python app.py
   ```

4. **访问 Web 界面**

   打开浏览器访问 `http://<服务器IP>:5000`

---

## 技术栈总览

| 层级 | 技术 |
|------|------|
| **边缘端 AI** | RKNN Toolkit, YOLOv8, OpenCV, GStreamer |
| **边缘端语言** | C++17, CMake |
| **视频流** | RTSP, WebRTC, MPP H.264 编码 |
| **消息队列** | Eclipse Paho MQTT |
| **Web 后端** | Flask, SQLAlchemy, Flask-Login |
| **Web 前端** | Bootstrap 5, Jinja2, Vanilla JS |
| **数据库** | MySQL |
| **嵌入式** | STM32 HAL, FreeRTOS, ESP8266 Arduino |

---

## 许可证

本项目仅供学习与研究使用。
