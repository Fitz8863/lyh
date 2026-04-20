#ifndef CAMERA_STATUS_H
#define CAMERA_STATUS_H

#include <atomic>
#include <mutex>
#include <string>
#include <chrono>
#include "postprocess.h"

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
    
    mutable std::mutex detect_mutex;
    object_detect_result_list detect_results;
    std::atomic<bool> has_new_detection{false};
    
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
    
    void SetDetectResults(const object_detect_result_list& results) {
        std::lock_guard<std::mutex> lock(detect_mutex);
        detect_results = results;
        has_new_detection.store(results.count > 0);
    }
    
    object_detect_result_list GetDetectResults() const {
        std::lock_guard<std::mutex> lock(detect_mutex);
        return detect_results;
    }
};

#endif