#include "voice_capture_thread.h"
#include "global_running.h"
#include <iostream>
#include <chrono>
#include <cstdio>

VoiceCaptureThread::VoiceCaptureThread(CameraStatus& status, const std::string& rtsp_url)
    : status_(status), rtsp_url_(rtsp_url), running_(false), pipe_(nullptr) {}

VoiceCaptureThread::~VoiceCaptureThread() {
    Stop();
}

void VoiceCaptureThread::Start() {
    if (!running_) {
        running_ = true;
        thread_ = std::thread(&VoiceCaptureThread::Run, this);
    }
}

void VoiceCaptureThread::Stop() {
    if (running_) {
        running_ = false;
        status_.voice_running.store(false);
        
        if (thread_.joinable()) {
            thread_.join();
        }
        
        if (pipe_) {
            pclose(pipe_);
            pipe_ = nullptr;
        }
    }
}

bool VoiceCaptureThread::IsRunning() const {
    return running_;
}

void VoiceCaptureThread::Run() {
    std::string cmd = 
        "ffmpeg -f alsa -channels 1 -sample_rate 44100 -i hw:3,0 "
        "-c:a aac -b:a 128k "
        "-f rtsp -rtsp_transport tcp " + rtsp_url_;

    std::cout << "音频推流命令: " << cmd << std::endl;

    pipe_ = popen(cmd.c_str(), "w");
    if (!pipe_) {
        std::cerr << "无法启动 ffmpeg" << std::endl;
        running_ = false;
        status_.voice_running.store(false);
        return;
    }

    std::cout << "音频推流已启动 → " << rtsp_url_ << std::endl;

    auto last_print = std::chrono::steady_clock::now();

    while (running_ && g_running && status_.voice_running.load()) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_print).count();
        if (elapsed >= 5) {
            std::cout << "音频推流中..." << std::endl;
            last_print = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (pipe_) {
        pclose(pipe_);
        pipe_ = nullptr;
    }
    running_ = false;
    status_.voice_running.store(false);
    std::cout << "音频推流线程已退出" << std::endl;
}