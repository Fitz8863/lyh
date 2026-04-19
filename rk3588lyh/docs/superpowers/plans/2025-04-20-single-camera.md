# 单摄像头架构改造实施计划

> **For agentic workers:** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 按任务执行。步骤使用复选框（`- [ ]`）语法跟踪。

**目标：** 将多摄像头架构简化为单摄像头架构，删除 CameraManager，直接在 main 中管理单个摄像头

**架构：** 
- 删除 CameraManager 类，main 直接创建 CaptureThread 和 CameraStatus 实例
- PublisherThread 从发布摄像头数组改为发布单个摄像头对象
- 保留配置文件中的 camera 数组格式，但只使用第一个配置项
- 简化线程模型：main → CaptureThread + PublisherThread（双线程）

**技术栈：** C++17, OpenCV, GStreamer, MQTT, yaml-cpp

---

### 任务 1: 删除 CameraManager 相关文件

**文件:**
- 删除: `src/camera_manager.h`
- 删除: `src/camera_manager.cc`

- [ ] **步骤 1: 确认文件可删除**

检查这两个文件是否被其他源文件包含。CameraManager 只在 main.cc 中使用。

```bash
grep -r "camera_manager" /home/cat/lyh/rk3588lyh/src/
```

预期：只在 main.cc 中找到引用。

- [ ] **步骤 2: 删除文件**

```bash
rm /home/cat/lyh/rk3588lyh/src/camera_manager.h
rm /home/cat/lyh/rk3588lyh/src/camera_manager.cc
```

- [ ] **步骤 3: 验证删除成功**

```bash
ls /home/cat/lyh/rk3588lyh/src/ | grep -i camera_manager
```

预期：无输出。

---

### 任务 2: 修改 main.cc 为单摄像头架构

**文件:**
- 修改: `src/main.cc`

- [ ] **步骤 1: 读取第一个摄像头配置**

替换相机循环逻辑，只取第一个摄像头配置。修改 lines 31-50。

**当前代码:**
```cpp
const auto& cameras = config["camera"];
for (std::size_t i = 0; i < cameras.size(); ++i) {
    const auto& cam = cameras[i];
    
    std::string camera_id = cam["id"].as<std::string>();
    std::string location = cam["location"].as<std::string>();
    std::string http_url = cam["http_url"].as<std::string>();
    std::string rtsp_url = cam["rtsp_url"].as<std::string>();
    std::string voice_rtsp_url = cam["voice_rtsp_url"].as<std::string>();
    std::string voice_http_url = cam["voice_http_url"].as<std::string>();
    std::string device = cam["device"].as<std::string>();
    int width = cam["width"].as<int>();
    int height = cam["height"].as<int>();
    int fps = cam["fps"].as<int>();
    
    camera_manager.AddCamera(camera_id, location, http_url, rtsp_url, 
                              voice_rtsp_url, voice_http_url, device, 
                              width, height, fps);
    std::cout << "已启动摄像头: " << camera_id << " (" << location << ")" << std::endl;
}
```

**新代码:**
```cpp
const auto& cameras = config["camera"];
if (cameras.empty()) {
    std::cerr << "错误：配置文件中没有摄像头配置！" << std::endl;
    return 1;
}

const auto& cam = cameras[0];  // 只使用第一个摄像头

std::string camera_id = cam["id"].as<std::string>();
std::string location = cam["location"].as<std::string>();
std::string http_url = cam["http_url"].as<std::string>();
std::string rtsp_url = cam["rtsp_url"].as<std::string>();
std::string device = cam["device"].as<std::string>();
int width = cam["width"].as<int>();
int height = cam["height"].as<int>();
int fps = cam["fps"].as<int>();

std::cout << "已启动摄像头: " << camera_id << " (" << location << ")" << std::endl;
```

- [ ] **步骤 2: 删除 CameraManager 相关代码**

移除 lines 29 和 52-65 的 CameraManager 相关代码。

**删除:**
- `CameraManager camera_manager;` (line 29)
- `auto statuses = camera_manager.GetAllStatuses();` (line 52)
- `camera_manager.StopAll();` (line 64)

**保留:**
- MQTT PublisherThread 初始化（line 53）
- 主循环（lines 58-60）
- 停止 publisher（line 65）

- [ ] **步骤 3: 创建单个 CameraStatus 和 CaptureThread**

在 main 函数中添加：

```cpp
// 创建单个摄像头状态和采集线程
CameraStatus camera_status;
CaptureThread capture_thread(camera_status, device, width, height, fps, rtsp_url);
capture_thread.Start();

std::cout << "RTSP 推流已启动 → " << rtsp_url << std::endl;
```

- [ ] **步骤 4: 修改 PublisherThread 初始化**

原代码（line 53）：
```cpp
auto statuses = camera_manager.GetAllStatuses();
PublisherThread publisher(statuses, device_id, mqtt_server, mqtt_topic, publish_interval);
```

改为单个 CameraStatus 引用：
```cpp
PublisherThread publisher(camera_status, device_id, mqtt_server, mqtt_topic, publish_interval);
```

- [ ] **步骤 5: 修改停止逻辑**

原代码（line 64）：
```cpp
camera_manager.StopAll();
```

改为：
```cpp
capture_thread.Stop();
```

- [ ] **步骤 6: 包含必要头文件**

确保 main.cc 包含：
```cpp
#include "camera_status.h"
#include "capture_thread.h"
```

移除：
```cpp
#include "camera_manager.h"
```

---

### 任务 3: 修改 PublisherThread 为单摄像头模式

**文件:**
- 修改: `src/publisher_thread.h`
- 修改: `src/publisher_thread.cc`

- [ ] **步骤 1: 修改 PublisherThread 头文件签名**

**当前:**
```cpp
class PublisherThread {
public:
    PublisherThread(const std::vector<std::reference_wrapper<CameraStatus>>& statuses,
                    const std::string& device_id,
                    const std::string& server, const std::string& topic, int interval);
    // ...
private:
    std::vector<std::reference_wrapper<CameraStatus>> statuses_;
    std::mutex statuses_mutex_;
};
```

**改为:**
```cpp
class PublisherThread {
public:
    PublisherThread(CameraStatus& status,
                    const std::string& device_id,
                    const std::string& server, const std::string& topic, int interval);
    // ...
private:
    CameraStatus& status_;
    std::mutex status_mutex_;
};
```

- [ ] **步骤 2: 修改构造函数实现**

**publisher_thread.cc line 8-12:**
```cpp
PublisherThread::PublisherThread(const std::vector<std::reference_wrapper<CameraStatus>>& statuses,
                                 const std::string& device_id,
                                 const std::string& server, const std::string& topic, int interval)
    : statuses_(statuses), device_id_(device_id), server_(server), topic_(topic), interval_(interval),
      running_(false), connected_(false) {}
```

改为：
```cpp
PublisherThread::PublisherThread(CameraStatus& status,
                                 const std::string& device_id,
                                 const std::string& server, const std::string& topic, int interval)
    : status_(status), device_id_(device_id), server_(server), topic_(topic), interval_(interval),
      running_(false), connected_(false) {}
```

- [ ] **步骤 3: 删除 UpdateStatuses 方法**

移除整个 `UpdateStatuses` 方法（lines 45-48），因为不再需要更新多个摄像头状态。

- [ ] **步骤 4: 重写 BuildJsonMessage 方法**

**当前:**
```cpp
std::string PublisherThread::BuildJsonMessage() {
    std::lock_guard<std::mutex> lock(statuses_mutex_);
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    
    int64_t timestamp = 0;
    if (!statuses_.empty()) {
        timestamp = statuses_[0].get().GetTimestamp();
    }
    
    oss << "{\"device_id\":\"" << device_id_ << "\",\"timestamp_ns\":" << timestamp << ",\"cameras\":[";
    
    for (size_t i = 0; i < statuses_.size(); ++i) {
        const auto& status = statuses_[i].get();
        if (i > 0) oss << ",";
        oss << "{"
            << "\"id\":\"" << status.camera_id << "\","
            << "\"location\":\"" << status.location << "\","
            << "\"http_url\":\"" << status.http_url << "\","
            << "\"rtsp_url\":\"" << status.rtsp_url << "\","
            << "\"voice_rtsp_url\":\"" << status.voice_rtsp_url << "\","
            << "\"voice_http_url\":\"" << status.voice_http_url << "\","
            << "\"voice_running\":" << (status.voice_running.load() ? "true" : "false") << ","
            << "\"resolution\":{"
            << "\"width\":" << status.width << ","
            << "\"height\":" << status.height << ","
            << "\"fps\":" << status.GetFps() << "}"
            << "}";
    }
    
    oss << "]}";
    return oss.str();
}
```

**改为:**
```cpp
std::string PublisherThread::BuildJsonMessage() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    
    int64_t timestamp = status_.GetTimestamp();
    
    oss << "{"
        << "\"device_id\":\"" << device_id_ << "\","
        << "\"timestamp_ns\":" << timestamp << ","
        << "\"camera\":{"
        << "\"id\":\"" << status_.camera_id << "\","
        << "\"location\":\"" << status_.location << "\","
        << "\"http_url\":\"" << status_.http_url << "\","
        << "\"rtsp_url\":\"" << status_.rtsp_url << "\","
        << "\"resolution\":{"
        << "\"width\":" << status_.width << ","
        << "\"height\":" << status_.height << ","
        << "\"fps\":" << status_.GetFps() << "}"
        << "}"
        << "}";
    
    return oss.str();
}
```

注意：已移除 voice_rtsp_url、voice_http_url、voice_running 字段（已在之前删除）。

- [ ] **步骤 5: 修改 Run 方法中的状态引用**

将 `statuses_.size()` 改为不再依赖（单摄像头不需要统计数量）。但保留循环逻辑（interval 控制），不需要修改。

---

### 任务 4: 更新 CMakeLists.txt

**文件:**
- 修改: `CMakeLists.txt`

- [ ] **步骤 1: 移除 camera_manager.cc 源文件**

当前：
```cmake
add_executable(main
    src/main.cc
    src/camera_status.cc
    src/capture_thread.cc
    src/publisher_thread.cc
    src/camera_manager.cc
)
```

改为：
```cmake
add_executable(main
    src/main.cc
    src/camera_status.cc
    src/capture_thread.cc
    src/publisher_thread.cc
)
```

- [ ] **步骤 2: 重新生成构建文件并验证**

```bash
cd /home/cat/lyh/rk3588lyh/build
rm -rf *
cmake ..
make
```

预期：无编译错误。

---

### 任务 5: 更新文档

**文件:**
- 修改: `README.md`
- 修改: `AGENTS.md`

- [ ] **步骤 1: 更新 README.md 架构说明**

修改 "项目结构" 部分，删除 camera_manager 相关条目：

当前：
```
├── src/
│   ├── main.cc
│   ├── camera_manager.h / .cc  # 摄像头管理器
│   ├── camera_status.h / .cc
│   ├── capture_thread.h / .cc
│   └── publisher_thread.h / .cc
```

改为：
```
├── src/
│   ├── main.cc                 # 主程序（直接管理单个摄像头）
│   ├── camera_status.h / .cc   # 摄像头状态结构
│   ├── capture_thread.h / .cc  # 视频采集线程
│   └── publisher_thread.h / .cc# MQTT 状态发布
```

- [ ] **步骤 2: 更新 "线程模型" 图示**

当前：
```
main()
├── CameraManager
│   ├── CameraContext[0]
│   │   └── CaptureThread (视频采集 + GStreamer 推流)
│   └── CameraContext[1] ...
│
└── PublisherThread (MQTT 状态上报)
```

改为：
```
main()
├── CaptureThread (视频采集 + GStreamer 推流)
└── PublisherThread (MQTT 状态上报)
```

- [ ] **步骤 3: 更新 AGENTS.md 中的项目结构**

修改 `AGENTS.md` 的 "1. 项目结构" 部分，删除 camera_manager 相关描述。

---

### 任务 6: 编译验证与功能测试

- [ ] **步骤 1: 清理并重新编译**

```bash
cd /home/cat/lyh/rk3588lyh
rm -rf build/*
cd build
cmake ..
make -j$(nproc)
```

预期：无错误，生成 `main` 可执行文件。

- [ ] **步骤 2: 检查二进制文件**

```bash
ls -lh /home/cat/lyh/rk3588lyh/build/main
```

预期：文件存在，大小约 500-600KB。

- [ ] **步骤 3: 验证配置文件格式**

确认 `config.yaml` 中的 camera 数组至少有一个元素（当前已有）。确保配置文件路径正确。

- [ ] **步骤 4: 静态分析（可选）**

如果有 clangd 或 cppcheck，运行静态检查：

```bash
# 如果有 clangd
clang-tidy src/*.cc -- -std=c++17 2>/dev/null || echo "clang-tidy 未配置，跳过"
```

---

## 预期交付物

- ✅ camera_manager.h/.cc 已删除
- ✅ main.cc 简化为单摄像头直接管理
- ✅ PublisherThread 改为发布单个摄像头 JSON 对象
- ✅ CMakeLists.txt 移除 camera_manager.cc
- ✅ README.md 和 AGENTS.md 文档更新
- ✅ 编译通过，生成可执行文件

---

## 注意事项

1. **向后兼容**：配置文件格式保持 `camera[]` 数组不变，但只使用第一个元素。这样未来扩展多摄像头时无需改配置。
2. **JSON 格式变化**：MQTT 上报的 JSON 从 `{"cameras": [...]}` 改为 `{"camera": {...}}`，注意 Web 端需同步修改。
3. **线程安全**：PublisherThread 直接引用 CameraStatus，确保生命周期管理（main 中先创建 status，再传给 publisher）。
4. **错误处理**：配置文件为空或缺少摄像头配置时，打印错误并退出。

---

## 回滚计划

如果改造后出现问题：
1. 保留原代码的 git commit 记录
2. 主要修改集中在 main.cc 和 publisher_thread.cc，可快速 revert

---

**计划创建完成，保存在：** `docs/superpowers/plans/2025-04-20-single-camera.md`

**执行建议：** 使用 subagent-driven-development 逐个任务执行，每个任务完成后验证编译状态。
