#include "publisher_thread.h"
#include "postprocess.h"
#include <mqtt/async_client.h>
#include <curl/curl.h>
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>

PublisherThread::PublisherThread(CameraStatus& status,
                                 const std::string& device_id,
                                 const std::string& server, const std::string& topic, int interval_ms,
                                 const std::string& report_topic,
                                 const std::string& http_report_url)
    : status_(status), device_id_(device_id), server_(server), topic_(topic), 
      report_topic_(report_topic), http_report_url_(http_report_url), interval_ms_(interval_ms),
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

std::string PublisherThread::BuildDetectionMessage(const object_detect_result_list& results, int target_class_id, int match_frames, int success_count) {
    if (target_class_id < 0) {
        return "";
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    
    int64_t timestamp = status_.GetTimestamp();
    
    oss << "{"
        << "\"device_id\":\"" << device_id_ << "\"," 
        << "\"camera_id\":\"" << status_.camera_id << "\"," 
        << "\"timestamp_ns\":" << timestamp << ","
        << "\"detection\":\"" << coco_cls_to_name(target_class_id) << "\"," 
        << "\"count\":" << results.count << ","
        << "\"window_match_frames\":" << match_frames << ","
        << "\"trigger_success_count\":" << success_count
        << "}";
    
    return oss.str();
}

bool PublisherThread::SendHttpReport(const std::vector<uint8_t>& jpeg_data, int target_class_id) {
    if (http_report_url_.empty() || jpeg_data.empty()) {
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[HTTP] Failed to initialize curl" << std::endl;
        return false;
    }

    bool success = false;
    try {
        std::string violation_type = coco_cls_to_name(target_class_id);
        
        curl_mime* mime = curl_mime_init(curl);
        
        curl_mimepart* file_part = curl_mime_addpart(mime);
        curl_mime_name(file_part, "file");
        curl_mime_filename(file_part, "capture.jpg");
        curl_mime_type(file_part, "image/jpeg");
        curl_mime_data(file_part, reinterpret_cast<const char*>(jpeg_data.data()), jpeg_data.size());
        
        curl_mimepart* camera_part = curl_mime_addpart(mime);
        curl_mime_name(camera_part, "camera_id");
        curl_mime_data(camera_part, status_.camera_id.c_str(), CURL_ZERO_TERMINATED);
        
        curl_mimepart* location_part = curl_mime_addpart(mime);
        curl_mime_name(location_part, "location");
        curl_mime_data(location_part, status_.location.c_str(), CURL_ZERO_TERMINATED);
        
        curl_mimepart* violation_part = curl_mime_addpart(mime);
        curl_mime_name(violation_part, "violation_type");
        curl_mime_data(violation_part, violation_type.c_str(), CURL_ZERO_TERMINATED);
        
        curl_easy_setopt(curl, CURLOPT_URL, http_report_url_.c_str());
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_PROXY, "");
        
        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            long response_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            std::cout << "[HTTP] Report sent to " << http_report_url_ 
                      << " violation_type=" << violation_type
                      << " camera_id=" << status_.camera_id
                      << " response_code=" << response_code << std::endl;
            success = (response_code >= 200 && response_code < 300);
            if (!success) {
                std::cerr << "[HTTP] Server returned error response_code=" << response_code << std::endl;
            }
        } else if (res == CURLE_OPERATION_TIMEDOUT) {
            std::cerr << "[HTTP] Request timeout (5s) to " << http_report_url_ 
                      << " violation_type=" << violation_type
                      << " camera_id=" << status_.camera_id << std::endl;
        } else {
            std::cerr << "[HTTP] Request failed: " << curl_easy_strerror(res) << std::endl;
        }
        
        curl_mime_free(mime);
    } catch (const std::exception& e) {
        std::cerr << "[HTTP] Exception: " << e.what() << std::endl;
    }
    
    curl_easy_cleanup(curl);
    return success;
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
            
            if (!report_topic_.empty()) {
                object_detect_result_list report_results;
                int target_class_id = -1;
                int match_frames = 0;
                int success_count = 0;
                std::vector<uint8_t> jpeg_data;
                if (status_.ConsumeDetectionReport(report_results, target_class_id, match_frames, success_count, jpeg_data)) {
                    std::string det_payload = BuildDetectionMessage(report_results, target_class_id, match_frames, success_count);
                    if (!det_payload.empty()) {
                        auto det_msg = mqtt::make_message(report_topic_, det_payload, 1, false);
                        client_->publish(det_msg)->wait();
                    }
                    
                    if (!http_report_url_.empty() && !jpeg_data.empty()) {
                        SendHttpReport(jpeg_data, target_class_id);
                    }
                }
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
