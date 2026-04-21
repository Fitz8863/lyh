#ifndef PUBLISHER_THREAD_H
#define PUBLISHER_THREAD_H

#include <string>
#include <mutex>
#include <thread>
#include <memory>
#include <atomic>
#include "camera_status.h"
#include <mqtt/async_client.h>

class PublisherThread {
public:
    PublisherThread(CameraStatus& status,
                    const std::string& device_id,
                    const std::string& server, const std::string& topic, int interval_ms,
                    const std::string& report_topic = "");
    ~PublisherThread();
    
    void Start();
    void Stop();
    bool IsRunning() const;
    bool IsConnected() const;
    
    std::string BuildJsonMessage();
    std::string BuildDetectionMessage(const object_detect_result_list& results, int match_frames, int success_count);
    
private:
    void Run();
    
    CameraStatus& status_;
    std::mutex status_mutex_;
    std::string device_id_;
    std::string server_;
    std::string topic_;
    std::string report_topic_;
    int interval_ms_;
    std::thread thread_;
    std::atomic<bool> running_;
    bool connected_;
    std::unique_ptr<mqtt::async_client> client_;
};

#endif
