#include "publisher_thread.h"
#include "postprocess.h"
#include <mqtt/async_client.h>
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>

PublisherThread::PublisherThread(CameraStatus& status,
                                 const std::string& device_id,
                                 const std::string& server, const std::string& topic, int interval_ms,
                                 const std::string& report_topic)
    : status_(status), device_id_(device_id), server_(server), topic_(topic), 
      report_topic_(report_topic), interval_ms_(interval_ms),
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

std::string PublisherThread::BuildJsonMessage() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    
    int64_t timestamp = status_.GetTimestamp();
    
    oss << "{"
        << "\"device_id\":\"" << device_id_ << "\","
        << "\"timestamp_ns\":" << timestamp << ","
        << "\"camera\":{"
        << "\"id\":\"" << status_.camera_id << "\","
        << "\"location\":\"" << status_.location << "\","
        << "\"http_url\":\"" << status_.http_url << "\","
        << "\"rtsp_url\":\"" << status_.rtsp_url << "\","
        << "\"resolution\":{"
        << "\"width\":" << status_.width << ","
        << "\"height\":" << status_.height << ","
        << "\"fps\":" << status_.GetFps() << "}"
        << "}"
        << "}";
    
    return oss.str();
}

std::string PublisherThread::BuildDetectionMessage() {
    object_detect_result_list results = status_.GetDetectResults();
    
    if (results.count <= 0) {
        return "";
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    
    int64_t timestamp = status_.GetTimestamp();
    
    oss << "{"
        << "\"device_id\":\"" << device_id_ << "\","
        << "\"camera_id\":\"" << status_.camera_id << "\","
        << "\"timestamp_ns\":" << timestamp << ","
        << "\"detections\":[";
    
    for (int i = 0; i < results.count; i++) {
        if (i > 0) oss << ",";
        const auto& det = results.results[i];
        oss << "{"
            << "\"class\":\"" << coco_cls_to_name(det.cls_id) << "\","
            << "\"class_id\":" << det.cls_id << ","
            << "\"confidence\":" << (det.prop * 100) << ","
            << "\"box\":{"
            << "\"left\":" << det.box.left << ","
            << "\"top\":" << det.box.top << ","
            << "\"right\":" << det.box.right << ","
            << "\"bottom\":" << det.box.bottom
            << "}"
            << "}";
    }
    
    oss << "],"
        << "\"count\":" << results.count
        << "}";
    
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
        std::cout << "MQTT 连接成功，心跳主题: " << topic_ << std::endl;
        if (!report_topic_.empty()) {
            std::cout << "检测上报主题: " << report_topic_ << std::endl;
        }
        
        while (running_) {
            auto start = std::chrono::steady_clock::now();
            
            std::string payload = BuildJsonMessage();
            auto msg = mqtt::make_message(topic_, payload, 1, false);
            client_->publish(msg)->wait();
            
            if (!report_topic_.empty() && status_.has_new_detection.load()) {
                std::string det_payload = BuildDetectionMessage();
                if (!det_payload.empty()) {
                    auto det_msg = mqtt::make_message(report_topic_, det_payload, 1, false);
                    client_->publish(det_msg)->wait();
                }
                status_.has_new_detection.store(false);
            }
            
            auto elapsed = std::chrono::steady_clock::now() - start;
            auto sleep_time = std::chrono::milliseconds(interval_ms_) - elapsed;
            if (sleep_time > std::chrono::milliseconds(0)) {
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
