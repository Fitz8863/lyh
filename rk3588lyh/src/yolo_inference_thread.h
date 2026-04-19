#ifndef YOLO_INFERENCE_THREAD_H
#define YOLO_INFERENCE_THREAD_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <string>
#include <memory>
#include <sstream>
#include <iomanip>

#include <opencv2/opencv.hpp>
#include "yolo_detector.h"
#include "camera_status.h"
#include "publisher_thread.h"
#include "postprocess.h"  // for draw_results and coco_cls_to_name

// 检测结果结构体
struct DetectionResult {
    std::string class_name;     // 检测类别名称
    float confidence;           // 置信度
    cv::Rect bbox;              // 边界框 (在原图坐标系)
    int camera_id;              // 摄像头ID
    std::string location;       // 位置描述
    int64_t timestamp_ns;       // 时间戳 (纳秒)
};

class YoloInferenceThread {
public:
    YoloInferenceThread(const std::string& model_path,
                        int thread_num,
                        float conf_threshold,
                        float nms_threshold,
                        CameraStatus* status,
                        PublisherThread* publisher);
    
    ~YoloInferenceThread();
    
    void Start();
    void Stop();
    bool IsRunning() const;
    
    // 从CaptureThread推送帧 (非阻塞)
    void PushFrame(const cv::Mat& frame);
    
    // 设置Publisher (延迟注入，用于在main中先创建YOLO再创建Publisher)
    void SetPublisher(PublisherThread* publisher) { publisher_ = publisher; }
    
private:
    void Run();
    void ProcessFrame(cv::Mat frame);
    void PublishDetection(const DetectionResult& det);
    
    std::string model_path_;
    int thread_num_;
    float conf_threshold_;
    float nms_threshold_;
    
    CameraStatus* status_;               // 共享状态 (只读)
    PublisherThread* publisher_;         // MQTT发布器指针
    
    std::unique_ptr<YoloDetector> detector_;
    
    // 帧队列 (锁保护)
    std::queue<cv::Mat> frame_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cond_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    
    // 统计
    std::atomic<int> total_frames_{0};
    std::atomic<int> detected_objects_{0};
};

#endif
