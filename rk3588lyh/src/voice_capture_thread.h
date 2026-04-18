#ifndef VOICE_CAPTURE_THREAD_H
#define VOICE_CAPTURE_THREAD_H

#include <thread>
#include <string>
#include "camera_status.h"

class VoiceCaptureThread {
public:
    VoiceCaptureThread(CameraStatus& status, const std::string& rtsp_url);
    ~VoiceCaptureThread();

    void Start();
    void Stop();
    bool IsRunning() const;

private:
    void Run();

    CameraStatus& status_;
    std::thread thread_;
    std::string rtsp_url_;
    bool running_;
    FILE* pipe_;
};

#endif