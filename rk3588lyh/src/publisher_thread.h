#ifndef PUBLISHER_THREAD_H
#define PUBLISHER_THREAD_H

#include <thread>
#include <string>
#include <memory>
#include "camera_status.h"

namespace mqtt { class async_client; }

class PublisherThread {
public:
    PublisherThread(CameraStatus* status,
                  const std::string& device_id,
                  const std::string& server, 
                  const std::string& topic, int heart_rate,
                  const std::string& user_name, const std::string& password);
    ~PublisherThread();
    
    void Start();
    void Stop();
    bool IsRunning() const;
    bool IsConnected() const;
    
    // 发布单条检测结果JSON (到 topic/detections)
    bool PublishDetection(const std::string& json_payload);
    
private:
    void Run();
    std::string BuildJsonMessage();
    
    CameraStatus* status_;
    std::thread thread_;
    std::string device_id_;
    std::string server_;
    std::string topic_;
    std::string detection_topic_;  // 检测结果主题 (topic + "/detections")
    int heart_rate_;
    std::string user_name_;
    std::string password_;
    bool running_;
    bool connected_;
    std::unique_ptr<mqtt::async_client> client_;
};

#endif