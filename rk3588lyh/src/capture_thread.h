#ifndef CAPTURE_THREAD_H
#define CAPTURE_THREAD_H

#include <thread>
#include <string>
#include "camera_status.h"

class YoloInferenceThread;  // 前向声明

class CaptureThread {
public:
    CaptureThread(CameraStatus& status, const std::string& device, 
                  int width, int height, int fps, const std::string& rtsp_url,
                  YoloInferenceThread* yolo_thread = nullptr);
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
    YoloInferenceThread* yolo_thread_;  // YOLO推理线程指针
};

#endif
