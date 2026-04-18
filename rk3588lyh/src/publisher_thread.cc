#include "publisher_thread.h"
#include <mqtt/async_client.h>
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>

PublisherThread::PublisherThread(const std::vector<std::reference_wrapper<CameraStatus>>& statuses,
                                 const std::string& device_id,
                                 const std::string& server, const std::string& topic, int interval)
    : statuses_(statuses), device_id_(device_id), server_(server), topic_(topic), interval_(interval),
      running_(false), connected_(false) {}

PublisherThread::~PublisherThread() {
    Stop();
}

void PublisherThread::Start() {
    if (!running_) {
        running_ = true;
        thread_ = std::thread(&PublisherThread::Run, this);
    }
}

void PublisherThread::Stop() {
    if (running_) {
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
        if (connected_ && client_) {
            client_->disconnect()->wait();
        }
    }
}

bool PublisherThread::IsRunning() const {
    return running_;
}

bool PublisherThread::IsConnected() const {
    return connected_;
}

void PublisherThread::UpdateStatuses(const std::vector<std::reference_wrapper<CameraStatus>>& statuses) {
    std::lock_guard<std::mutex> lock(statuses_mutex_);
    statuses_ = statuses;
}

std::string PublisherThread::BuildJsonMessage() {
    std::lock_guard<std::mutex> lock(statuses_mutex_);
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    
    int64_t timestamp = 0;
    if (!statuses_.empty()) {
        timestamp = statuses_[0].get().GetTimestamp();
    }
    
    oss << "{\"device_id\":\"" << device_id_ << "\",\"timestamp_ns\":" << timestamp << ",\"cameras\":[";
    
    for (size_t i = 0; i < statuses_.size(); ++i) {
        const auto& status = statuses_[i].get();
        if (i > 0) oss << ",";
        oss << "{"
            << "\"id\":\"" << status.camera_id << "\","
            << "\"location\":\"" << status.location << "\","
            << "\"http_url\":\"" << status.http_url << "\","
            << "\"rtsp_url\":\"" << status.rtsp_url << "\","
            << "\"voice_rtsp_url\":\"" << status.voice_rtsp_url << "\","
            << "\"voice_http_url\":\"" << status.voice_http_url << "\","
            << "\"voice_running\":" << (status.voice_running.load() ? "true" : "false") << ","
            << "\"resolution\":{"
            << "\"width\":" << status.width << ","
            << "\"height\":" << status.height << ","
            << "\"fps\":" << status.GetFps() << "}"
            << "}";
    }
    
    oss << "]}";
    return oss.str();
}

void PublisherThread::Run() {
    try {
        client_ = std::make_unique<mqtt::async_client>(server_, "rk3588_pub");
        
        mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(20);
        connOpts.set_clean_session(true);
        
        client_->connect(connOpts)->wait();
        connected_ = true;
        std::cout << "MQTT 连接成功，主题: " << topic_ << std::endl;
        
        while (running_) {
            auto start = std::chrono::steady_clock::now();
            
            std::string payload = BuildJsonMessage();
            auto msg = mqtt::make_message(topic_, payload, 1, false);
            client_->publish(msg)->wait();
            
            std::cout << "已发送状态信息 - 当前的摄像头数: " << statuses_.size() << std::endl;
            
            auto elapsed = std::chrono::steady_clock::now() - start;
            auto sleep_time = std::chrono::seconds(interval_) - elapsed;
            if (sleep_time > std::chrono::seconds(0)) {
                std::this_thread::sleep_for(sleep_time);
            }
        }
        
        client_->disconnect()->wait();
        connected_ = false;
        std::cout << "MQTT 客户端已断开" << std::endl;
        
    } catch (const mqtt::exception& exc) {
        std::cerr << "MQTT 错误: " << exc.what() << std::endl;
        connected_ = false;
    }
}