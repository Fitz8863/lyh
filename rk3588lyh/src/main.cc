#include <iostream>
#include <memory>
#include <vector>
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
    
    std::string mqtt_server = "mqtt://" + config["mqtt"]["server"].as<std::string>();
    std::string mqtt_topic = config["mqtt"]["topic"].as<std::string>();
    int publish_interval = config["mqtt"]["publish_interval"].as<int>();
    
    std::string device_id = config["device"]["device_id"].as<std::string>();
    
    CameraManager camera_manager;
    
    const auto& cameras = config["camera"];
    for (std::size_t i = 0; i < cameras.size(); ++i) {
        const auto& cam = cameras[i];
        
        std::string camera_id = cam["id"].as<std::string>();
        std::string location = cam["location"].as<std::string>();
        std::string http_url = cam["http_url"].as<std::string>();
        std::string rtsp_url = cam["rtsp_url"].as<std::string>();
        std::string voice_rtsp_url = cam["voice_rtsp_url"].as<std::string>();
        std::string voice_http_url = cam["voice_http_url"].as<std::string>();
        std::string device = cam["device"].as<std::string>();
        int width = cam["width"].as<int>();
        int height = cam["height"].as<int>();
        int fps = cam["fps"].as<int>();
        
        camera_manager.AddCamera(camera_id, location, http_url, rtsp_url, 
                                  voice_rtsp_url, voice_http_url, device, 
                                  width, height, fps);
        std::cout << "已启动摄像头: " << camera_id << " (" << location << ")" << std::endl;
    }
    
    auto statuses = camera_manager.GetAllStatuses();
    PublisherThread publisher(statuses, device_id, mqtt_server, mqtt_topic, publish_interval);
    publisher.Start();
    
    std::cout << "所有服务已启动，按 Ctrl+C 退出..." << std::endl;
    
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "正在停止服务..." << std::endl;
    
    camera_manager.StopAll();
    publisher.Stop();
    
    std::cout << "所有服务已停止" << std::endl;
    return 0;
}