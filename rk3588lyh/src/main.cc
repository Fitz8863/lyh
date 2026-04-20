#include <iostream>
#include <memory>
#include <vector>
#include <csignal>
#include <thread>
#include "global_running.h"
#include "camera_status.h"
#include "capture_thread.h"
#include "publisher_thread.h"
#include "yolo_detector.h"
#include <yaml-cpp/yaml.h>

std::atomic<bool> g_running(true);

void signal_handler(int) {
    g_running = false;
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    std::string config_path = std::string(std::getenv("HOME")) + "/lyh/rk3588lyh/config.yaml";
    YAML::Node config = YAML::LoadFile(config_path);
    
    std::string mqtt_server = "mqtt://" + config["mqtt"]["server"].as<std::string>();
    std::string mqtt_topic = config["mqtt"]["heart_topic"].as<std::string>();
    int heart_rate_ms = config["mqtt"]["heart_rate"].as<int>();
    int publish_interval_sec = heart_rate_ms / 1000;
    if (publish_interval_sec <= 0) publish_interval_sec = 1;
    
    std::string device_id = config["device"]["device_id"].as<std::string>();
    
    const auto& cam = config["camera"];
    if (!cam) {
        std::cerr << "错误：配置文件中没有摄像头配置！" << std::endl;
        return 1;
    }
    
    std::string camera_id = cam["id"].as<std::string>();
    std::string location = cam["location"].as<std::string>();
    std::string http_url = cam["http_url"].as<std::string>();
    std::string rtsp_url = cam["rtsp_url"].as<std::string>();
    std::string device = cam["device"].as<std::string>();
    int width = cam["width"].as<int>();
    int height = cam["height"].as<int>();
    int fps = cam["fps"].as<int>();
    bool use_yolo = cam["use_yolo"].as<bool>();
    
    std::cout << "已启动摄像头: " << camera_id << " (" << location << ")" << std::endl;
    
    CameraStatus camera_status;
    camera_status.camera_id = camera_id;
    camera_status.location = location;
    camera_status.http_url = http_url;
    camera_status.rtsp_url = rtsp_url;
    camera_status.width = width;
    camera_status.height = height;
    
    YoloDetector* detector = nullptr;
    std::string mqtt_report_topic;
    
    if (use_yolo) {
        const auto& det_cfg = cam["detector"];
        if (det_cfg) {
            std::string model_path = det_cfg["model_path"].as<std::string>();
            int thread_num = det_cfg["thread_num"].as<int>();
            mqtt_report_topic = det_cfg["mqtt_report_topic"].as<std::string>();
            
            detector = new YoloDetector(model_path, thread_num);
            int ret = detector->init();
            if (ret != 0) {
                std::cerr << "错误：YOLOv8 模型初始化失败！" << std::endl;
                delete detector;
                detector = nullptr;
            } else {
                std::cout << "YOLOv8 模型加载成功 (" << model_path << ", " << thread_num << " 线程)" << std::endl;
            }
        } else {
            std::cout << "未配置检测器参数，跳过 YOLOv8 推理" << std::endl;
        }
    } else {
        std::cout << "YOLOv8 推理已禁用 (use_yolo: false)" << std::endl;
    }
    
    CaptureThread capture_thread(camera_status, device, width, height, fps, rtsp_url, detector);
    capture_thread.Start();
    
    PublisherThread publisher(camera_status, device_id, mqtt_server, mqtt_topic, heart_rate_ms, mqtt_report_topic);
    publisher.Start();
    
    std::cout << "所有服务已启动，按 Ctrl+C 退出..." << std::endl;
    
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "正在停止服务..." << std::endl;
    
    capture_thread.Stop();
    publisher.Stop();
    
    if (detector) {
        delete detector;
        detector = nullptr;
    }
    
    std::cout << "所有服务已停止" << std::endl;
    return 0;
}
