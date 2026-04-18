#ifndef PUBLISHER_THREAD_H
#define PUBLISHER_THREAD_H

#include <thread>
#include <string>
#include <memory>
#include <vector>
#include "camera_status.h"

namespace mqtt { class async_client; }

class PublisherThread {
public:
    PublisherThread(const std::vector<std::reference_wrapper<CameraStatus>>& statuses,
                    const std::string& device_id,
                    const std::string& server, const std::string& topic, int interval);
    ~PublisherThread();
    
    void Start();
    void Stop();
    bool IsRunning() const;
    bool IsConnected() const;
    void UpdateStatuses(const std::vector<std::reference_wrapper<CameraStatus>>& statuses);
    
private:
    void Run();
    std::string BuildJsonMessage();
    
    std::vector<std::reference_wrapper<CameraStatus>> statuses_;
    std::thread thread_;
    std::string device_id_;
    std::string server_;
    std::string topic_;
    int interval_;
    bool running_;
    bool connected_;
    std::unique_ptr<mqtt::async_client> client_;
    std::mutex statuses_mutex_;
};

#endif