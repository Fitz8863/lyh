#include "capture_thread.h"
#include "global_running.h"
#include "postprocess.h"
#include <opencv2/opencv.hpp>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <unistd.h>

namespace
{
std::string QuoteShellArg(const std::string &value)
{
    std::string quoted = "'";
    for (char c : value)
    {
        if (c == '\'')
        {
            quoted += "'\\''";
        }
        else
        {
            quoted += c;
        }
    }
    quoted += "'";
    return quoted;
}

std::string GetSiblingExecutablePath(const std::string &executable_name)
{
    char exe_path[4096];
    const ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len <= 0)
    {
        return executable_name;
    }

    exe_path[len] = '\0';
    const std::filesystem::path current_binary(exe_path);
    return (current_binary.parent_path() / executable_name).string();
}

bool ReadExact(FILE *pipe, unsigned char *buffer, size_t bytes_to_read)
{
    size_t total_read = 0;
    while (total_read < bytes_to_read)
    {
        const size_t n = fread(buffer + total_read, 1, bytes_to_read - total_read, pipe);
        if (n == 0)
        {
            return false;
        }
        total_read += n;
    }
    return true;
}
} // namespace

CaptureThread::CaptureThread(CameraStatus &status, const std::string &device,
                             int width, int height, int fps, const std::string &rtsp_url,
                             YoloDetector *detector)
    : status_(status), device_(device), width_(width), height_(height),
      fps_(fps), rtsp_url_(rtsp_url), running_(false), detector_(detector) {}

CaptureThread::~CaptureThread()
{
    Stop();
}

void CaptureThread::Start()
{
    if (!running_)
    {
        running_ = true;
        thread_ = std::thread(&CaptureThread::Run, this);
    }
}

void CaptureThread::Stop()
{
    if (running_)
    {
        running_ = false;
        status_.running.store(false);
        if (thread_.joinable())
        {
            thread_.join();
        }
    }
}

bool CaptureThread::IsRunning() const
{
    return running_ && status_.running.load();
}

void CaptureThread::Run()
{
    std::cout << "正在通过独立 helper 打开摄像头 (MJPEG " << width_ << "x" << height_ << "@" << fps_ << "fps)..." << std::endl;

    const std::string helper_path = GetSiblingExecutablePath("camera_capture_helper");
    const std::string capture_cmd =
        QuoteShellArg(helper_path) + " " +
        QuoteShellArg(device_) + " " +
        std::to_string(width_) + " " +
        std::to_string(height_) + " " +
        std::to_string(fps_);

    FILE *capture_pipe = popen(capture_cmd.c_str(), "r");
    if (!capture_pipe)
    {
        std::cerr << "错误：无法启动摄像头 helper！" << std::endl;
        running_ = false;
        status_.running.store(false);
        return;
    }

    const size_t frame_bytes = static_cast<size_t>(width_) * static_cast<size_t>(height_) * 3U;
    cv::Mat frame(height_, width_, CV_8UC3);
    if (!ReadExact(capture_pipe, frame.data, frame_bytes))
    {
        std::cerr << "错误：无法从 helper 读取首帧！" << std::endl;
        pclose(capture_pipe);
        running_ = false;
        status_.running.store(false);
        return;
    }

    // 使用 ffmpeg 管道推流（和 before.cc 相同的方式）
    std::string cmd =
        "ffmpeg -y "
        "-f rawvideo -pix_fmt bgr24 -s " +
        std::to_string(width_) + "x" + std::to_string(height_) +
        " -r " + std::to_string(fps_) +
        " -i - "
        "-c:v h264_rkmpp "
        "-b:v 4M -maxrate 4M -bufsize 8M "
        "-g " + std::to_string(fps_) +
        " -keyint_min " + std::to_string(fps_) +
        " -bf 0 "
        "-fflags nobuffer -flags low_delay "
        "-rtsp_transport udp "
        "-f rtsp " +
        rtsp_url_;

    FILE *ffmpeg = popen(cmd.c_str(), "w");
    if (!ffmpeg)
    {
        std::cerr << "错误：无法打开 ffmpeg 管道！" << std::endl;
        pclose(capture_pipe);
        running_ = false;
        status_.running.store(false);
        return;
    }

    std::cout << "RTSP 推流已启动 → " << rtsp_url_ << std::endl;

    auto fps_update_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    float current_fps = 0.0f;
    bool detector_ready = (detector_ != nullptr);

    if (detector_ready)
    {
        std::cout << "YOLOv8 推理已启用（异步模式）" << std::endl;
    }

    if (detector_ready)
    {
        detector_->submit(frame);
    }
    else
    {
        fwrite(frame.data, 1, width_ * height_ * 3, ffmpeg);
    }

    while (running_ && status_.running.load() && g_running)
    {
        if (!ReadExact(capture_pipe, frame.data, frame_bytes))
        {
            std::cerr << "读取 helper 输出帧失败！" << std::endl;
            break;
        }

        auto now = std::chrono::high_resolution_clock::now();
        frame_count++;
        float elapsed = std::chrono::duration<float>(now - fps_update_time).count();

        if (elapsed >= 1.0f)
        {
            current_fps = frame_count / elapsed;
            status_.UpdateFps(current_fps);
            status_.UpdateTimestamp();
            frame_count = 0;
            fps_update_time = now;
        }

        if (detector_ready)
        {
            detector_->submit(frame);

            cv::Mat annotated_frame;
            object_detect_result_list results;
            if (detector_->retrieve(annotated_frame, results))
            {
                draw_results(annotated_frame, results);
                status_.SetDetectResults(results);
                fwrite(annotated_frame.data, 1, width_ * height_ * 3, ffmpeg);
            }
            else
            {
                fwrite(frame.data, 1, width_ * height_ * 3, ffmpeg);
            }
        }
        else
        {
            fwrite(frame.data, 1, width_ * height_ * 3, ffmpeg);
        }
    }

    pclose(capture_pipe);
    pclose(ffmpeg);
    std::cout << "摄像头推流线程已退出" << std::endl;
}
