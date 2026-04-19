#include <iostream>
#include <memory>
#include <csignal>
#include <thread>
#include "global_running.h"
#include "camera_manager.h"
#include "capture_thread.h"
#include "publisher_thread.h"
#include <yaml-cpp/yaml.h>

std::atomic<bool> g_running(true);

void signal_handler(int) {
    g_running = false;
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    YAML::Node config = YAML::LoadFile("../config.yaml");
    
std::string mqtt_server = "tcp://" + config["mqtt"]["server"].as<std::string>();
    std::string mqtt_topic = config["mqtt"]["heart_topic"].as<std::string>();
    int mqtt_heart_rate = config["mqtt"]["heart_rate"].as<int>();
    std::string mqtt_user = config["mqtt"]["user_name"].as<std::string>();
    std::string mqtt_password = config["mqtt"]["password"].as<std::string>();
    
    std::string device_id = config["device"]["device_id"].as<std::string>();
    
    CameraManager camera_manager;
    
    const auto& cam = config["camera"];
    std::string camera_id = cam["id"].as<std::string>();
    std::string location = cam["location"].as<std::string>();
    std::string http_url = cam["http_url"].as<std::string>();
    std::string rtsp_url = cam["rtsp_url"].as<std::string>();
    std::string device = cam["device"].as<std::string>();
    int width = cam["width"].as<int>();
    int height = cam["height"].as<int>();
    int fps = cam["fps"].as<int>();
    
    // 读取 detector 配置 (可选)
    std::string model_path = "/home/cat/lyh/rk3588lyh/model/yolov8s.rknn";  // 默认路径
    int thread_num = 3;
    float conf_threshold = 0.25f;
    float nms_threshold = 0.45f;
    
    if (cam["detector"]) {
        if (cam["detector"]["model_path"])
            model_path = cam["detector"]["model_path"].as<std::string>();
        if (cam["detector"]["thread_num"])
            thread_num = cam["detector"]["thread_num"].as<int>();
        if (cam["detector"]["confidence_threshold"])
            conf_threshold = cam["detector"]["confidence_threshold"].as<float>();
        if (cam["detector"]["nms_threshold"])
            nms_threshold = cam["detector"]["nms_threshold"].as<float>();
    }
    
    camera_manager.SetCamera(camera_id, location, http_url, rtsp_url,
                        device, width, height, fps,
                        model_path, thread_num, conf_threshold, nms_threshold);
    std::cout << "已启动摄像头: " << camera_id << " (" << location << ")" << std::endl;
    
    PublisherThread publisher(camera_manager.GetStatus(), device_id, mqtt_server, mqtt_topic, mqtt_heart_rate, mqtt_user, mqtt_password);
    publisher.Start();
    
    // 注入 Publisher 到 YOLO 线程 (延迟绑定)
    camera_manager.SetPublisher(&publisher);
    
    std::cout << "所有服务已启动，按 Ctrl+C 退出..." << std::endl;
    
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "正在停止服务..." << std::endl;
    
    camera_manager.Stop();
    publisher.Stop();
    
    std::cout << "所有服务已停止" << std::endl;
    return 0;
}