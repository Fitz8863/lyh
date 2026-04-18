# 设计文档：RK3588 摄像头推流与 MQTT 状态发布

## 1. 项目概述

将 RK3588 摄像头推流功能封装为独立线程，并实现 MQTT 定时发布线程推送实时状态信息。

## 2. 系统架构

```
┌─────────────────────────────────────────────────┐
│                   main.cc                        │
│  ┌──────────────────┐    ┌──────────────────┐   │
│  │ 摄像头推流线程   │    │ MQTT发布线程     │   │
│  │ (CaptureThread) │    │ (PublisherThread)│   │
│  │  - 读取摄像头   │───▶│  - 发送状态信息  │   │
│  │  - 硬件编码    │ FPS │  - 定时发送      │   │
│  │  - RTSP推流     │    │  - 从yaml读取    │   │
│  └──────────────────┘    └──────────────────┘   │
│           │                      ▲               │
│           ▼                      │               │
│  ┌──────────────────┐           │               │
│  │ 共享数据结构     │           │               │
│  │ (CameraStatus)   │───────────┘               │
│  │ - fps            │                           │
│  │ - resolution     │                           │
│  │ - timestamp_ns   │                           │
│  └──────────────────┘                           │
└─────────────────────────────────────────────────┘
```

## 3. YAML 配置文件 (config.yaml)

```yaml
mqtt:
  server: "fnas:1883"
  topic: "RK3588/info"
  publish_interval: 1

camera:
  device: "/dev/video0"
  width: 1920
  height: 1080
  fps: 30
  rtsp_url: "rtsp://fnas:8554/rk3588"

info:
  id: "001"
  location: "生产车间A区"
  http_url: "http://fnas:8889/jetson1"
```

## 4. MQTT 消息 JSON 格式

```json
{
  "timestamp_ns": 1773740540883479165,
  "cameras": [{
    "id": "001",
    "location": "生产车间A区",
    "http_url": "http://fnas:8889/jetson1",
    "resolution": {
      "width": 1920,
      "height": 1080,
      "fps": 25.2672
    }
  }]
}
```

## 5. 组件设计

### 5.1 CameraStatus (共享数据结构)

- `std::atomic<float> fps` - 实时帧率
- `int width` - 分辨率宽度
- `int height` - 分辨率高度
- `std::atomic<bool> running` - 运行状态标志
- `std::mutex mutex` - 互斥锁保护 timestamp

### 5.2 CaptureThread

- 读取摄像头 (v4l2src + jpegdec)
- 硬件编码 (mpph264enc)
- RTSP 推流 (rtspclientsink)
- 计算并更新实时 FPS 到共享结构

### 5.3 PublisherThread

- 定时从 YAML 读取配置
- 从共享结构获取实时 FPS
- 构建 JSON 消息
- 发布到 MQTT 主题

## 6. 依赖

- yaml-cpp: YAML 配置文件解析
- paho-mqtt: MQTT 客户端
- pthread: 线程支持

## 7. 错误处理

- 摄像头打开失败: 输出错误信息，退出线程
- MQTT 连接失败: 输出错误信息，继续重试
- YAML 解析失败: 使用默认值

## 8. 验证步骤

1. 编译: `cd build && cmake .. && make`
2. 运行: `./build/main`
3. 检查 MQTT 消息是否正常发送到 RK3588/info 主题