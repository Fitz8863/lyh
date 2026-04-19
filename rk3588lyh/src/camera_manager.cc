#include "camera_manager.h"
#include "yolo_inference_thread.h"

CameraManager::CameraManager() = default;

CameraManager::~CameraManager() {
    Stop();
}

void CameraManager::SetCamera(
    const std::string& camera_id, const std::string& location,
    const std::string& http_url, const std::string& rtsp_url,
    const std::string& device,
    int width, int height, int fps,
    const std::string& model_path, int thread_num,
    float conf_threshold, float nms_threshold) {
    
    status_ = std::make_unique<CameraStatus>();
    status_->camera_id = camera_id;
    status_->location = location;
    status_->http_url = http_url;
    status_->rtsp_url = rtsp_url;
    status_->width = width;
    status_->height = height;
    
    // 创建采集线程 (暂时不传递YOLO线程，稍后SetPublisher注入)
    capture_ = std::make_unique<CaptureThread>(*status_, device, width, height, fps, rtsp_url, nullptr);
    capture_->Start();
    
    // 如果配置了YOLO模型路径，则创建YOLO推理线程
    if (!model_path.empty() && thread_num > 0) {
        yolo_thread_ = std::make_unique<YoloInferenceThread>(
            model_path, thread_num, conf_threshold, nms_threshold,
            status_.get(), nullptr);  // publisher先传nullptr，后续注入
        yolo_thread_->Start();
        std::cout << "YOLO推理已启用: " << model_path << " (线程数: " << thread_num << ")" << std::endl;
    }
}

void CameraManager::Stop() {
    if (capture_) {
        capture_->Stop();
        capture_ = nullptr;
    }
    if (yolo_thread_) {
        yolo_thread_->Stop();
        yolo_thread_ = nullptr;
    }
    status_ = nullptr;
}

void CameraManager::SetPublisher(PublisherThread* publisher) {
    if (yolo_thread_) {
        yolo_thread_->SetPublisher(publisher);
    }
}