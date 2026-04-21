#ifndef CAPTURE_THREAD_H
#define CAPTURE_THREAD_H

#include <thread>
#include <string>
#include <memory>
#include "camera_status.h"
#include "yolo_detector.h"

struct DetectionTriggerConfig {
    bool enabled = false;
    int window_seconds = 0;
    int target_class_id = -1;
    int frame_threshold = 0;
    int trigger_count = 0;
};

class CaptureThread {
public:
    CaptureThread(CameraStatus& status, const std::string& device, 
                  int width, int height, int fps, const std::string& rtsp_url,
                  YoloDetector* detector = nullptr,
                  const DetectionTriggerConfig& trigger_config = {});
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
    YoloDetector* detector_;
    DetectionTriggerConfig trigger_config_;
};

#endif
