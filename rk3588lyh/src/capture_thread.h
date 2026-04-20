#ifndef CAPTURE_THREAD_H
#define CAPTURE_THREAD_H

#include <thread>
#include <string>
#include <memory>
#include "camera_status.h"
#include "yolo_detector.h"

class CaptureThread {
public:
    CaptureThread(CameraStatus& status, const std::string& device, 
                  int width, int height, int fps, const std::string& rtsp_url,
                  YoloDetector* detector = nullptr);
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
};

#endif
