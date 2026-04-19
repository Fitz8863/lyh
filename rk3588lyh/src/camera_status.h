#ifndef CAMERA_STATUS_H
#define CAMERA_STATUS_H

#include <atomic>
#include <mutex>
#include <string>
#include <chrono>

struct CameraStatus {
    std::atomic<float> fps;
    int width;
    int height;
    std::atomic<bool> running;
    int64_t timestamp_ns;
    mutable std::mutex timestamp_mutex;
    
    std::string camera_id;
    std::string location;
    std::string http_url;
    std::string rtsp_url;
    
    CameraStatus() : fps(0.0f), width(1920), height(1080), running(true), timestamp_ns(0) {}
    
    void UpdateFps(float new_fps) {
        fps.store(new_fps);
    }
    
    void UpdateTimestamp() {
        std::lock_guard<std::mutex> lock(timestamp_mutex);
        auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        timestamp_ns = now;
    }
    
    float GetFps() const {
        return fps.load();
    }
    
    int64_t GetTimestamp() const {
        std::lock_guard<std::mutex> lock(timestamp_mutex);
        return timestamp_ns;
    }
};

#endif