#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <memory>
#include <vector>
#include <string>
#include "camera_status.h"
#include "capture_thread.h"
#include "voice_capture_thread.h"

class CameraManager {
public:
    CameraManager() = default;
    ~CameraManager();

    void AddCamera(const std::string& camera_id, const std::string& location,
                   const std::string& http_url, const std::string& rtsp_url,
                   const std::string& voice_rtsp_url, const std::string& voice_http_url,
                   const std::string& device,
                   int width, int height, int fps);

    void StopAll();

    std::vector<std::reference_wrapper<CameraStatus>> GetAllStatuses();

    size_t Size() const { return cameras_.size(); }

private:
    struct CameraContext {
        std::unique_ptr<CameraStatus> status;
        std::unique_ptr<CaptureThread> capture;
        std::unique_ptr<VoiceCaptureThread> voice_capture;
    };

    std::vector<CameraContext> cameras_;
};

#endif