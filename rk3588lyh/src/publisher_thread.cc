#include "publisher_thread.h"
#include <mqtt/async_client.h>
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>

PublisherThread::PublisherThread(CameraStatus* status,
                                const std::string& device_id,
                                const std::string& server, 
                                const std::string& topic, int heart_rate,
                                const std::string& user_name, const std::string& password)
    : status_(status), device_id_(device_id), server_(server), topic_(topic),
      heart_rate_(heart_rate), user_name_(user_name), password_(password),
      running_(false), connected_(false) {
    detection_topic_ = topic_ + "/detections";  // 例如: "rk3588lyh/info/detections"
}

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

std::string PublisherThread::BuildJsonMessage() {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    
    int64_t timestamp = status_ ? status_->GetTimestamp() : 0;
    
    oss << "{\"device_id\":\"" << device_id_ << "\",\"timestamp_ns\":" << timestamp << ",";
    oss << "\"camera\":{";
    if (status_) {
        oss << "\"id\":\"" << status_->camera_id << "\","
            << "\"location\":\"" << status_->location << "\","
            << "\"http_url\":\"" << status_->http_url << "\","
            << "\"rtsp_url\":\"" << status_->rtsp_url << "\","
            << "\"resolution\":{"
            << "\"width\":" << status_->width << ","
            << "\"height\":" << status_->height << ","
            << "\"fps\":" << status_->GetFps() << "}"
            << "}";
    }
    oss << "}";
    return oss.str();
}

void PublisherThread::Run() {
    try {
        std::cout << "正在连接 MQTT broker: " << server_ << std::endl;
        client_ = std::make_unique<mqtt::async_client>(server_, "rk3588_pub");
        
        mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(20);
        connOpts.set_clean_session(true);
        connOpts.set_user_name(user_name_);
        connOpts.set_password(password_);
        
        client_->connect(connOpts)->wait();
        connected_ = true;
        std::cout << "MQTT 连接成功，主题: " << topic_ << " (检测主题: " << detection_topic_ << ")" << std::endl;
        
        auto last_send_time = std::chrono::steady_clock::now();
        
        while (running_) {
            auto now = std::chrono::steady_clock::now();
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_send_time).count();
            if (elapsed >= heart_rate_) {
                std::string payload = BuildJsonMessage();
                auto token = client_->publish(topic_, payload.data(), payload.length(), 1, false);
                token->wait();
                // std::cout << "已发送心跳" << std::endl;
                last_send_time = now;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        client_->disconnect()->wait();
        connected_ = false;
        std::cout << "MQTT 客户端已断开" << std::endl;
        
    } catch (const mqtt::exception& exc) {
        std::cerr << "MQTT 错误: " << exc.what() << std::endl;
        connected_ = false;
    } catch (const std::exception& exc) {
        std::cerr << "通用错误: " << exc.what() << std::endl;
        connected_ = false;
    }
}

bool PublisherThread::PublishDetection(const std::string& json_payload) {
    if (!running_ || !connected_ || !client_) {
        return false;
    }
    
    try {
        auto token = client_->publish(detection_topic_, json_payload.data(), 
                                      json_payload.length(), 1, false);
        token->wait();
        return true;
    } catch (const mqtt::exception& e) {
        std::cerr << "MQTT检测发布失败: " << e.what() << std::endl;
        return false;
    }
}