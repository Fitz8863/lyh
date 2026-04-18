#include "camera_manager.h"

CameraManager::~CameraManager() {
    StopAll();
}

void CameraManager::AddCamera(const std::string& camera_id, const std::string& location,
                              const std::string& http_url, const std::string& rtsp_url,
                              const std::string& voice_rtsp_url, const std::string& voice_http_url,
                              const std::string& device,
                              int width, int height, int fps) {
    CameraContext ctx;
    ctx.status = std::make_unique<CameraStatus>();
    ctx.status->camera_id = camera_id;
    ctx.status->location = location;
    ctx.status->http_url = http_url;
    ctx.status->rtsp_url = rtsp_url;
    ctx.status->voice_rtsp_url = voice_rtsp_url;
    ctx.status->voice_http_url = voice_http_url;
    ctx.status->width = width;
    ctx.status->height = height;
    
    ctx.capture = std::make_unique<CaptureThread>(*ctx.status, device, width, height, fps, rtsp_url);
    ctx.capture->Start();
    
    ctx.voice_capture = std::make_unique<VoiceCaptureThread>(*ctx.status, voice_rtsp_url);
    ctx.voice_capture->Start();
    ctx.status->voice_running.store(true);
    
    cameras_.push_back(std::move(ctx));
}

void CameraManager::StopAll() {
    for (auto& ctx : cameras_) {
        if (ctx.voice_capture) {
            ctx.voice_capture->Stop();
        }
        if (ctx.capture) {
            ctx.capture->Stop();
        }
    }
    cameras_.clear();
}

std::vector<std::reference_wrapper<CameraStatus>> CameraManager::GetAllStatuses() {
    std::vector<std::reference_wrapper<CameraStatus>> result;
    for (auto& ctx : cameras_) {
        if (ctx.status) {
            result.push_back(std::ref(*ctx.status));
        }
    }
    return result;
}