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
                    const std::string& server, const std::string& topic, int interval_ms);
    ~PublisherThread();
    
    void Start();
    void Stop();
    bool IsRunning() const;
    bool IsConnected() const;
    
    std::string BuildJsonMessage();
    
private:
    void Run();
    
    CameraStatus& status_;
    std::mutex status_mutex_;
    std::string device_id_;
    std::string server_;
    std::string topic_;
    int interval_ms_;
    std::thread thread_;
    std::atomic<bool> running_;
    bool connected_;
    std::unique_ptr<mqtt::async_client> client_;
};

#endif