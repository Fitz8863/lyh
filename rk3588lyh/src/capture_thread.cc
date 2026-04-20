#include "capture_thread.h"
#include "global_running.h"
#include "postprocess.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>

CaptureThread::CaptureThread(CameraStatus& status, const std::string& device,
                             int width, int height, int fps, const std::string& rtsp_url,
                             YoloDetector* detector)
    : status_(status), device_(device), width_(width), height_(height), 
      fps_(fps), rtsp_url_(rtsp_url), running_(false), detector_(detector) {}

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
    // 使用 GStreamer pipeline 读取 MJPEG（和 before.cc 相同的方式）
    std::string read_pipeline =
        "v4l2src device=" + device_ + " ! "
        "image/jpeg, width=(int)" + std::to_string(width_) +
        ", height=(int)" + std::to_string(height_) +
        ", framerate=" + std::to_string(fps_) + "/1 ! "
        "jpegdec ! "
        "videoconvert ! video/x-raw, format=BGR ! "
        "appsink drop=1 max-buffers=1";

    std::cout << "正在打开摄像头 (MJPEG " << width_ << "x" << height_ << "@" << fps_ << "fps)..." << std::endl;

    cv::VideoCapture cap(read_pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened()) {
        std::cerr << "错误：无法打开摄像头 pipeline！" << std::endl;
        running_ = false;
        status_.running.store(false);
        return;
    }

    // 使用 ffmpeg 管道推流（和 before.cc 相同的方式）
    std::string cmd =
        "ffmpeg -y "
        "-f rawvideo -pix_fmt bgr24 -s " + std::to_string(width_) + "x" + std::to_string(height_) +
        " -r " + std::to_string(fps_) +
        " -i - "
        "-c:v h264_rkmpp "
        "-fflags nobuffer -flags low_delay "
        "-rtsp_transport udp "
        "-f rtsp " + rtsp_url_;

    FILE* ffmpeg = popen(cmd.c_str(), "w");
    if (!ffmpeg) {
        std::cerr << "错误：无法打开 ffmpeg 管道！" << std::endl;
        cap.release();
        running_ = false;
        status_.running.store(false);
        return;
    }

    std::cout << "RTSP 推流已启动 → " << rtsp_url_ << std::endl;

    auto fps_update_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    float current_fps = 0.0f;
    bool detector_ready = (detector_ != nullptr);
    
    if (detector_ready) {
        std::cout << "YOLOv8 推理已启用（异步模式）" << std::endl;
    }
    
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
        
        if (detector_ready) {
            detector_->submit(frame);
            
            cv::Mat annotated_frame;
            object_detect_result_list results;
            if (detector_->retrieve(annotated_frame, results)) {
                draw_results(annotated_frame, results);
                status_.SetDetectResults(results);
                fwrite(annotated_frame.data, 1, width_ * height_ * 3, ffmpeg);
            } else {
                fwrite(frame.data, 1, width_ * height_ * 3, ffmpeg);
            }
        } else {
            fwrite(frame.data, 1, width_ * height_ * 3, ffmpeg);
        }
    }
    
    cap.release();
    pclose(ffmpeg);
    std::cout << "摄像头推流线程已退出" << std::endl;
}
