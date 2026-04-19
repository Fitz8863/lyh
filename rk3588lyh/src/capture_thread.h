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
