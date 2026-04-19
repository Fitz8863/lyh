#include <iostream>
#include <memory>
#include <vector>
#include <csignal>
#include <thread>
#include "global_running.h"
#include "camera_status.h"
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
    
    // 使用绝对路径加载配置文件
    std::string config_path = std::string(std::getenv("HOME")) + "/lyh/rk3588lyh/config.yaml";
    YAML::Node config = YAML::LoadFile(config_path);
    
    std::string mqtt_server = "mqtt://" + config["mqtt"]["server"].as<std::string>();
    std::string mqtt_topic = config["mqtt"]["heart_topic"].as<std::string>();
    int heart_rate_ms = config["mqtt"]["heart_rate"].as<int>();
    int publish_interval_sec = heart_rate_ms / 1000;  // 转换为秒
    if (publish_interval_sec <= 0) publish_interval_sec = 1;
    
    std::string device_id = config["device"]["device_id"].as<std::string>();
    
    // camera 配置是单个对象（非数组），直接使用
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
    
    std::cout << "已启动摄像头: " << camera_id << " (" << location << ")" << std::endl;
    
    // 创建单个摄像头状态和采集线程
    CameraStatus camera_status;
    camera_status.camera_id = camera_id;
    camera_status.location = location;
    camera_status.http_url = http_url;
    camera_status.rtsp_url = rtsp_url;
    camera_status.width = width;
    camera_status.height = height;
    
    CaptureThread capture_thread(camera_status, device, width, height, fps, rtsp_url);
    capture_thread.Start();
    
    PublisherThread publisher(camera_status, device_id, mqtt_server, mqtt_topic, heart_rate_ms);
    publisher.Start();
    
    std::cout << "所有服务已启动，按 Ctrl+C 退出..." << std::endl;
    
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "正在停止服务..." << std::endl;
    
    capture_thread.Stop();
    publisher.Stop();
    
    std::cout << "所有服务已停止" << std::endl;
    return 0;
}