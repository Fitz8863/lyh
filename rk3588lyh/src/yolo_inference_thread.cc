#include "yolo_inference_thread.h"
#include <iostream>
#include <chrono>
#include <sstream>

YoloInferenceThread::YoloInferenceThread(
    const std::string& model_path,
    int thread_num,
    float conf_threshold,
    float nms_threshold,
    CameraStatus* status,
    PublisherThread* publisher)
    : model_path_(model_path), thread_num_(thread_num),
      conf_threshold_(conf_threshold), nms_threshold_(nms_threshold),
      status_(status), publisher_(publisher) {
    
    detector_ = std::make_unique<YoloDetector>(model_path_, thread_num_);
}

YoloInferenceThread::~YoloInferenceThread() {
    Stop();
}

void YoloInferenceThread::Start() {
    if (!running_.load()) {
        running_.store(true);
        
        // 初始化YOLO检测器
        if (detector_->init() != 0) {
            std::cerr << "错误: YOLO检测器初始化失败" << std::endl;
            running_.store(false);
            return;
        }
        
        std::cout << "YOLO推理线程已启动 (线程数: " << thread_num_ << ")" << std::endl;
        thread_ = std::thread(&YoloInferenceThread::Run, this);
    }
}

void YoloInferenceThread::Stop() {
    if (running_.exchange(false)) {
        queue_cond_.notify_all();  // 唤醒等待的线程
        if (thread_.joinable()) {
            thread_.join();
        }
        std::cout << "YOLO推理线程已停止" << std::endl;
    }
}

bool YoloInferenceThread::IsRunning() const {
    return running_.load() && thread_.joinable();
}

void YoloInferenceThread::PushFrame(const cv::Mat& frame) {
    if (!running_.load() || frame.empty()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(queue_mutex_);
    // 限制队列大小防止内存暴涨 (建议: thread_num * 3)
    if (frame_queue_.size() >= static_cast<size_t>(thread_num_ * 3)) {
        frame_queue_.pop();  // 丢弃最旧的帧
    }
    frame_queue_.push(frame.clone());
    queue_cond_.notify_one();
}

void YoloInferenceThread::Run() {
    cv::Mat frame;
    
    while (running_.load()) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cond_.wait(lock, [this] { 
                return !frame_queue_.empty() || !running_.load(); 
            });
            
            if (!running_.load() && frame_queue_.empty()) {
                break;
            }
            
            if (frame_queue_.empty()) {
                continue;
            }
            
            frame = std::move(frame_queue_.front());
            frame_queue_.pop();
        }
        
        ProcessFrame(frame);
    }
    
    // 清理剩余队列
    while (!frame_queue_.empty()) {
        frame_queue_.pop();
    }
}

void YoloInferenceThread::ProcessFrame(cv::Mat frame) {
    total_frames_++;
    
    // 提交推理
    detector_->submit(frame);
    
    // 获取结果
    cv::Mat result_frame;
    object_detect_result_list results;
    if (detector_->retrieve(result_frame, results) && results.count > 0) {
        
        // 在帧副本上绘制检测框
        draw_results(result_frame, results);
        
        // 更新统计到 CameraStatus
        if (status_) {
            status_->UpdateDetections(results.count);
        }
        
        // 发布每条检测记录到 MQTT
        for (int i = 0; i < results.count; i++) {
            const auto& det = results.results[i];
            DetectionResult dr;
            dr.class_name = coco_cls_to_name(det.cls_id);
            dr.confidence = det.prop;
            dr.bbox = cv::Rect(det.box.left, det.box.top, 
                               det.box.right - det.box.left, 
                               det.box.bottom - det.box.top);
            dr.camera_id = status_ ? std::stoi(status_->camera_id) : 0;
            dr.location = status_ ? status_->location : "unknown";
            
            auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            dr.timestamp_ns = now;
            
            PublishDetection(dr);
            detected_objects_++;
        }
    }
}

void YoloInferenceThread::PublishDetection(const DetectionResult& det) {
    if (!publisher_ || !publisher_->IsConnected()) {
        return;
    }
    
    // 构建JSON消息 (手动拼接, 避免额外JSON库依赖)
    std::ostringstream oss;
    oss << "{"
        << "\"location\":\"" << det.location << "\","
        << "\"id\":" << det.camera_id << ","
        << "\"type\":\"" << det.class_name << "\","
        << "\"time\":" << det.timestamp_ns
        << "}";
    
    std::string payload = oss.str();
    publisher_->PublishDetection(payload);
}
