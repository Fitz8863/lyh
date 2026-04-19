#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <memory>
#include <string>
#include "camera_status.h"
#include "capture_thread.h"

class YoloInferenceThread;  // 前向声明

class CameraManager {
public:
    CameraManager();
    ~CameraManager();
    
    void SetCamera(const std::string& camera_id, const std::string& location,
                   const std::string& http_url, const std::string& rtsp_url,
                   const std::string& device,
                   int width, int height, int fps,
                   // YOLO参数 (可选)
                   const std::string& model_path = "",
                   int thread_num = 3,
                   float conf_threshold = 0.25f,
                   float nms_threshold = 0.45f);
    
    void Stop();
    
    CameraStatus* GetStatus() { return status_.get(); }
    bool IsRunning() const { return capture_ && capture_->IsRunning(); }
    
    // 注入Publisher到YOLO线程 (延迟初始化)
    void SetPublisher(PublisherThread* publisher);
    
    // 获取YOLO线程指针 (用于传递Publisher)
    YoloInferenceThread* GetYoloThread() { return yolo_thread_.get(); }
    
private:
    std::unique_ptr<CameraStatus> status_;
    std::unique_ptr<CaptureThread> capture_;
    std::unique_ptr<YoloInferenceThread> yolo_thread_;  // YOLO推理线程
};

#endif