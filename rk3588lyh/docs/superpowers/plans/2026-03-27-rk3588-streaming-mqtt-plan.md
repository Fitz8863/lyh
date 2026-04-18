# RK3588 摄像头推流与 MQTT 状态发布实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将摄像头推流封装为独立线程，实现 MQTT 定时发布线程推送实时状态信息

**Architecture:** 使用 std::thread 创建两个独立线程：CaptureThread 负责视频采集和推流，PublisherThread 负责 MQTT 状态发布。通过 std::atomic 共享实时 FPS 数据。

**Tech Stack:** C++17, OpenCV, GStreamer, yaml-cpp, paho-mqtt, pthread

---

## 文件结构

```
rk3588/
├── CMakeLists.txt           # 添加 yaml-cpp 和 paho-mqtt 依赖
├── config.yaml              # 配置文件
├── src/
│   ├── main.cc              # 主程序入口
│   ├── capture_thread.{h,cc}  # 摄像头推流线程
│   ├── publisher_thread.{h,cc} # MQTT发布线程
│   └── camera_status.{h,cc}   # 共享状态数据结构
└── docs/superpowers/
    └── specs/                # 设计文档
```

---

## 实现步骤

### Task 1: 创建 YAML 配置文件

**Files:**
- Create: `config.yaml`

- [ ] **Step 1: 创建 config.yaml 配置文件**

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

- [ ] **Step 2: Commit**

```bash
git add config.yaml
git commit -m "feat: add yaml config file"
```

---

### Task 2: 创建共享状态数据结构

**Files:**
- Create: `src/camera_status.h`
- Create: `src/camera_status.cc`

- [ ] **Step 1: 创建 camera_status.h**

```cpp
#ifndef CAMERA_STATUS_H
#define CAMERA_STATUS_H

#include <atomic>
#include <mutex>
#include <string>
#include <cstdint>

struct CameraStatus {
    std::atomic<float> fps;
    int width;
    int height;
    std::atomic<bool> running;
    int64_t timestamp_ns;
    std::mutex timestamp_mutex;
    
    std::string camera_id;
    std::string location;
    std::string http_url;
    
    CameraStatus() : fps(0.0f), width(1920), height(1080), running(true), timestamp_ns(0) {}
    
    void UpdateFps(float new_fps) {
        fps.store(new_fps);
    }
    
    void UpdateTimestamp() {
        std::lock_guard<std::mutex> lock(timestamp_mutex);
        auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        timestamp_ns = now;
    }
    
    float GetFps() const {
        return fps.load();
    }
    
    int64_t GetTimestamp() const {
        std::lock_guard<std::mutex> lock(timestamp_mutex);
        return timestamp_ns;
    }
};

#endif
```

- [ ] **Step 2: 创建 camera_status.cc (实现如果需要)**

```cpp
#include "camera_status.h"
```

- [ ] **Step 3: Commit**

```bash
git add src/camera_status.h src/camera_status.cc
git commit -m "feat: add camera status data structure"
```

---

### Task 3: 创建摄像头推流线程

**Files:**
- Create: `src/capture_thread.h`
- Create: `src/capture_thread.cc`

- [ ] **Step 1: 创建 capture_thread.h**

```cpp
#ifndef CAPTURE_THREAD_H
#define CAPTURE_THREAD_H

#include <thread>
#include <string>
#include "camera_status.h"

class CaptureThread {
public:
    CaptureThread(CameraStatus& status, const std::string& device, 
                  int width, int height, int fps, const std::string& rtsp_url);
    ~CaptureThread();
    
    void Start();
    void Stop();
    bool IsRunning() const;
    
private:
    void Run();
    
    CameraStatus& status_;
    std::thread thread_;
    std::string device_;
    int width_;
    int height_;
    int fps_;
    std::string rtsp_url_;
    bool running_;
};

#endif
```

- [ ] **Step 2: 创建 capture_thread.cc**

```cpp
#include "capture_thread.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>

CaptureThread::CaptureThread(CameraStatus& status, const std::string& device,
                             int width, int height, int fps, const std::string& rtsp_url)
    : status_(status), device_(device), width_(width), height_(height), 
      fps_(fps), rtsp_url_(rtsp_url), running_(false) {}

CaptureThread::~CaptureThread() {
    Stop();
}

void CaptureThread::Start() {
    if (!running_) {
        running_ = true;
        thread_ = std::thread(&CaptureThread::Run, this);
    }
}

void CaptureThread::Stop() {
    if (running_) {
        running_ = false;
        status_.running.store(false);
        if (thread_.joinable()) {
            thread_.join();
        }
    }
}

bool CaptureThread::IsRunning() const {
    return running_ && status_.running.load();
}

void CaptureThread::Run() {
    std::string read_pipeline =
        "v4l2src device=" + device_ + " ! "
        "image/jpeg, width=(int)" + std::to_string(width_) +
        ", height=(int)" + std::to_string(height_) +
        ", framerate=" + std::to_string(fps_) + "/1 ! "
        "jpegdec ! "
        "videoconvert ! video/x-raw, format=BGR ! "
        "appsink drop=1 max-buffers=1";

    std::cout << "正在打开摄像头 (MJPEG " << width_ << "x" << height_ << "@" << fps_ << "fps)..." << std::endl;

    cv::VideoCapture cap(read_pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened()) {
        std::cerr << "错误：无法打开摄像头 pipeline！" << std::endl;
        running_ = false;
        status_.running.store(false);
        return;
    }

    std::string push_pipeline =
        "appsrc ! "
        "videoconvert ! "
        "video/x-raw,format=NV12,width=" + std::to_string(width_) +
        ",height=" + std::to_string(height_) +
        ",framerate=" + std::to_string(fps_) + "/1 ! "
        "mpph264enc bps=8000000 ! "
        "h264parse ! "
        "rtspclientsink location=" + rtsp_url_;

    cv::VideoWriter writer(push_pipeline, cv::CAP_GSTREAMER, 0, fps_, cv::Size(width_, height_), true);
    if (!writer.isOpened()) {
        std::cerr << "错误：无法打开推流 pipeline！" << std::endl;
        cap.release();
        running_ = false;
        status_.running.store(false);
        return;
    }

    std::cout << "RTSP 推流已启动 → " << rtsp_url_ << " (8Mbps)" << std::endl;

    auto fps_update_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    float current_fps = 0.0f;
    
    cv::Mat frame;
    while (running_ && status_.running.load()) {
        cap >> frame;
        
        if (frame.empty()) {
            std::cerr << "读取摄像头帧失败！" << std::endl;
            break;
        }
        
        auto now = std::chrono::high_resolution_clock::now();
        frame_count++;
        float elapsed = std::chrono::duration<float>(now - fps_update_time).count();
        
        if (elapsed >= 1.0f) {
            current_fps = frame_count / elapsed;
            status_.UpdateFps(current_fps);
            status_.UpdateTimestamp();
            frame_count = 0;
            fps_update_time = now;
        }
        
        std::string fps_text = "FPS: " + std::to_string(int(current_fps));
        cv::putText(frame, fps_text, cv::Point(frame.cols - 130, 40),
                    cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);
        
        writer << frame;
    }
    
    cap.release();
    writer.release();
    std::cout << "摄像头推流线程已退出" << std::endl;
}
```

- [ ] **Step 3: Commit**

```bash
git add src/capture_thread.h src/capture_thread.cc
git commit -m "feat: add capture thread implementation"
```

---

### Task 4: 创建 MQTT 发布线程

**Files:**
- Create: `src/publisher_thread.h`
- Create: `src/publisher_thread.cc`

- [ ] **Step 1: 创建 publisher_thread.h**

```cpp
#ifndef PUBLISHER_THREAD_H
#define PUBLISHER_THREAD_H

#include <thread>
#include <string>
#include <memory>
#include "camera_status.h"

namespace mqtt { class async_client; }

class PublisherThread {
public:
    PublisherThread(CameraStatus& status, const std::string& server, 
                    const std::string& topic, int interval,
                    const std::string& camera_id, const std::string& location,
                    const std::string& http_url, int width, int height);
    ~PublisherThread();
    
    void Start();
    void Stop();
    bool IsRunning() const;
    bool IsConnected() const;
    
private:
    void Run();
    std::string BuildJsonMessage();
    
    CameraStatus& status_;
    std::thread thread_;
    std::string server_;
    std::string topic_;
    int interval_;
    std::string camera_id_;
    std::string location_;
    std::string http_url_;
    int width_;
    int height_;
    bool running_;
    bool connected_;
    std::unique_ptr<mqtt::async_client> client_;
};

#endif
```

- [ ] **Step 2: 创建 publisher_thread.cc**

```cpp
#include "publisher_thread.h"
#include <mqtt/async_client.h>
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>

PublisherThread::PublisherThread(CameraStatus& status, const std::string& server,
                                 const std::string& topic, int interval,
                                 const std::string& camera_id, const std::string& location,
                                 const std::string& http_url, int width, int height)
    : status_(status), server_(server), topic_(topic), interval_(interval),
      camera_id_(camera_id), location_(location), http_url_(http_url),
      width_(width), height_(height), running_(false), connected_(false) {}

PublisherThread::~PublisherThread() {
    Stop();
}

void PublisherThread::Start() {
    if (!running_) {
        running_ = true;
        thread_ = std::thread(&PublisherThread::Run, this);
    }
}

void PublisherThread::Stop() {
    if (running_) {
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
        if (connected_ && client_) {
            client_->disconnect()->wait();
        }
    }
}

bool PublisherThread::IsRunning() const {
    return running_;
}

bool PublisherThread::IsConnected() const {
    return connected_;
}

std::string PublisherThread::BuildJsonMessage() {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    
    float fps = status_.GetFps();
    int64_t timestamp = status_.GetTimestamp();
    
    oss << "{"
        << "\"timestamp_ns\":" << timestamp << ","
        << "\"cameras\":[{"
        << "\"id\":\"" << camera_id_ << "\","
        << "\"location\":\"" << location_ << "\","
        << "\"http_url\":\"" << http_url_ << "\","
        << "\"resolution\":{"
        << "\"width\":" << width_ << ","
        << "\"height\":" << height_ << ","
        << "\"fps\":" << fps << "}"
        << "}]"
        << "}";
    
    return oss.str();
}

void PublisherThread::Run() {
    try {
        client_ = std::make_unique<mqtt::async_client>(server_, "rk3588_pub");
        
        mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(20);
        connOpts.set_clean_session(true);
        
        client_->connect(connOpts)->wait();
        connected_ = true;
        std::cout << "MQTT 连接成功，主题: " << topic_ << std::endl;
        
        while (running_) {
            auto start = std::chrono::steady_clock::now();
            
            std::string payload = BuildJsonMessage();
            auto msg = mqtt::make_message(topic_, payload, 1, false);
            client_->publish(msg)->wait();
            
            std::cout << "已发送状态信息 - FPS: " << std::fixed << std::setprecision(2) 
                      << status_.GetFps() << std::endl;
            
            auto elapsed = std::chrono::steady_clock::now() - start;
            auto sleep_time = std::chrono::seconds(interval_) - elapsed;
            if (sleep_time > std::chrono::seconds(0)) {
                std::this_thread::sleep_for(sleep_time);
            }
        }
        
        client_->disconnect()->wait();
        connected_ = false;
        std::cout << "MQTT 客户端已断开" << std::endl;
        
    } catch (const mqtt::exception& exc) {
        std::cerr << "MQTT 错误: " << exc.what() << std::endl;
        connected_ = false;
    }
}
```

- [ ] **Step 3: Commit**

```bash
git add src/publisher_thread.h src/publisher_thread.cc
git commit -m "feat: add MQTT publisher thread implementation"
```

---

### Task 5: 修改主程序 main.cc

**Files:**
- Modify: `src/main.cc`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 更新 CMakeLists.txt 添加依赖**

```cmake
cmake_minimum_required(VERSION 3.4.1)

project(main)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-pthread")

find_package(OpenCV REQUIRED)

# 添加 yaml-cpp
find_package(yaml-cpp REQUIRED)

# 添加 Paho MQTT
find_library(MQTT_LIBRARY mqtt REQUIRED)
if(NOT MQTT_LIBRARY)
    message(FATAL_ERROR "MQTT library not found")
endif()

add_executable(main
    src/main.cc
    src/camera_status.cc
    src/capture_thread.cc
    src/publisher_thread.cc
)

target_link_libraries(main
    ${OpenCV_LIBS}
    ${MQTT_LIBRARY}
    yaml-cpp
)
```

- [ ] **Step 2: 更新 main.cc**

```cpp
#include <iostream>
#include <memory>
#include "camera_status.h"
#include "capture_thread.h"
#include "publisher_thread.h"
#include <yaml-cpp/yaml.h>

int main() {
    // 读取配置文件
    YAML::Node config = YAML::LoadFile("config.yaml");
    
    // MQTT 配置
    std::string mqtt_server = "mqtt://" + config["mqtt"]["server"].as<std::string>();
    std::string mqtt_topic = config["mqtt"]["topic"].as<std::string>();
    int publish_interval = config["mqtt"]["publish_interval"].as<int>();
    
    // 摄像头配置
    std::string device = config["camera"]["device"].as<std::string>();
    int width = config["camera"]["width"].as<int>();
    int height = config["camera"]["height"].as<int>();
    int fps = config["camera"]["fps"].as<int>();
    std::string rtsp_url = config["camera"]["rtsp_url"].as<std::string>();
    
    // 信息配置
    std::string camera_id = config["info"]["id"].as<std::string>();
    std::string location = config["info"]["location"].as<std::string>();
    std::string http_url = config["info"]["http_url"].as<std::string>();
    
    // 创建共享状态
    CameraStatus status;
    status.width = width;
    status.height = height;
    status.camera_id = camera_id;
    status.location = location;
    status.http_url = http_url;
    
    // 启动摄像头推流线程
    CaptureThread capture(status, device, width, height, fps, rtsp_url);
    capture.Start();
    
    // 启动 MQTT 发布线程
    PublisherThread publisher(status, mqtt_server, mqtt_topic, publish_interval,
                             camera_id, location, http_url, width, height);
    publisher.Start();
    
    std::cout << "服务已启动，按 Enter 键退出..." << std::endl;
    std::cin.get();
    
    // 停止线程
    capture.Stop();
    publisher.Stop();
    
    std::cout << "服务已停止" << std::endl;
    return 0;
}
```

- [ ] **Step 3: Commit**

```bash
git add src/main.cc CMakeLists.txt
git commit -m "feat: integrate capture and publisher threads"
```

---

### Task 6: 编译测试

**Files:**
- Test: 编译和运行

- [ ] **Step 1: 编译项目**

```bash
cd build
cmake ..
make
```

- [ ] **Step 2: 运行测试**

```bash
./build/main
```

- [ ] **Step 3: 验证 MQTT 消息**

检查 MQTT 客户端是否收到 RK3588/info 主题的消息

- [ ] **Step 4: Commit**

```bash
git add .
git commit -m "test: verify build and runtime"
```