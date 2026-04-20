#include "capture_thread.h"
#include "global_running.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>

CaptureThread::CaptureThread(CameraStatus& status, const std::string& device,
                             int width, int height, int fps, const std::string& rtsp_url)
    : status_(status), device_(device), width_(width), height_(height), 
      fps_(fps), rtsp_url_(rtsp_url), running_(false) {}

CaptureThread::~CaptureThread() {
    Stop();
}

void CaptureThread::Start() {
    if (!running_) {
        running_ = true;
        thread_ = std::thread(&CaptureThread::Run, this);
    }
}

void CaptureThread::Stop() {
    if (running_) {
        running_ = false;
        status_.running.store(false);
        if (thread_.joinable()) {
            thread_.join();
        }
    }
}

bool CaptureThread::IsRunning() const {
    return running_ && status_.running.load();
}

void CaptureThread::Run() {
     cv::VideoCapture cap;
    cap.open(0, cv::CAP_V4L2);

    // 打开后立即设置参数
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G')); // 强制使用 MJPEG 格式
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1920);  // 宽
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 1080); // 高
    cap.set(cv::CAP_PROP_FPS, 25);            // 帧率
    if (!cap.isOpened()) {
        std::cerr << "错误：无法打开摄像头 pipeline！" << std::endl;
        running_ = false;
        status_.running.store(false);
        return;
    }

    std::string push_pipeline =
        "appsrc ! "
        "videoconvert ! "
        "video/x-raw,format=NV12,width=" + std::to_string(width_) +
        ",height=" + std::to_string(height_) +
        ",framerate=" + std::to_string(fps_) + "/1 ! "
        "mpph264enc bps=8000000 ! "
        "h264parse ! "
        "rtspclientsink location=" + rtsp_url_;

    cv::VideoWriter writer(push_pipeline, cv::CAP_GSTREAMER, 0, fps_, cv::Size(width_, height_), true);
    if (!writer.isOpened()) {
        std::cerr << "错误：无法打开推流 pipeline！" << std::endl;
        cap.release();
        running_ = false;
        status_.running.store(false);
        return;
    }

    std::cout << "RTSP 推流已启动 → " << rtsp_url_ << " (8Mbps)" << std::endl;

    auto fps_update_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    float current_fps = 0.0f;
    
    cv::Mat frame;
    while (running_ && status_.running.load() && g_running) {
        cap >> frame;
        
        if (frame.empty()) {
            std::cerr << "读取摄像头帧失败！" << std::endl;
            break;
        }
        
        auto now = std::chrono::high_resolution_clock::now();
        frame_count++;
        float elapsed = std::chrono::duration<float>(now - fps_update_time).count();
        
        if (elapsed >= 1.0f) {
            current_fps = frame_count / elapsed;
            status_.UpdateFps(current_fps);
            status_.UpdateTimestamp();
            frame_count = 0;
            fps_update_time = now;
        }
        
        writer << frame;
    }
    
    cap.release();
    writer.release();
    std::cout << "摄像头推流线程已退出" << std::endl;
}
