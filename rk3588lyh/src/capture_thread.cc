#include "capture_thread.h"
#include "global_running.h"
#include "postprocess.h"
#include <opencv2/opencv.hpp>
#include <chrono>
#include <iostream>

namespace
{
bool ContainsTargetClass(const object_detect_result_list& results, int target_class_id)
{
    for (int i = 0; i < results.count; ++i)
    {
        if (results.results[i].cls_id == target_class_id)
        {
            return true;
        }
    }
    return false;
}

void DrawFpsOverlay(cv::Mat& frame, float fps)
{
    const std::string text = cv::format("FPS: %.1f", fps);
    const double font_scale = 1.0;
    const int thickness = 2;
    int baseline = 0;
    const cv::Size text_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, font_scale, thickness, &baseline);
    const int margin = 20;
    const cv::Point origin(frame.cols - text_size.width - margin, margin + text_size.height);
    cv::putText(frame, text, origin, cv::FONT_HERSHEY_SIMPLEX, font_scale, cv::Scalar(0, 255, 0), thickness, cv::LINE_AA);
}
}

CaptureThread::CaptureThread(CameraStatus &status, const std::string &device,
                             int width, int height, int fps, const std::string &rtsp_url,
                             YoloDetector *detector, const DetectionTriggerConfig &trigger_config)
    : status_(status), device_(device), width_(width), height_(height),
      fps_(fps), rtsp_url_(rtsp_url), running_(false), detector_(detector), trigger_config_(trigger_config) {}

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
    std::string read_pipeline =
        "v4l2src device=" + device_ + " ! "
        "image/jpeg, width=(int)" + std::to_string(width_) +
        ", height=(int)" + std::to_string(height_) +
        ", framerate=" + std::to_string(fps_) + "/1 ! "
        "jpegdec ! "
        "videoconvert ! video/x-raw, format=BGR ! "
        "appsink drop=1 max-buffers=1 sync=false";

    std::cout << "正在打开摄像头 (MJPEG " << width_ << "x" << height_ << "@" << fps_ << "fps)..." << std::endl;

    cv::VideoCapture cap(read_pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened())
    {
        std::cerr << "错误：无法打开摄像头 pipeline！" << std::endl;
        running_ = false;
        status_.running.store(false);
        return;
    }

    cv::Mat frame;
    cap >> frame;
    if (frame.empty())
    {
        std::cerr << "错误：无法读取首帧！" << std::endl;
        cap.release();
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
    const bool trigger_enabled = detector_ready && trigger_config_.enabled;
    auto trigger_window_start = std::chrono::steady_clock::now();
    int trigger_match_frames = 0;
    int trigger_success_count = 0;
    object_detect_result_list last_matching_results{};

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
        DrawFpsOverlay(frame, current_fps);
        fwrite(frame.data, 1, width_ * height_ * 3, ffmpeg);
    }

    while (running_ && status_.running.load() && g_running)
    {
        cap >> frame;
        if (frame.empty())
        {
            std::cerr << "读取摄像头帧失败！" << std::endl;
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
                if (trigger_enabled)
                {
                    const auto trigger_now = std::chrono::steady_clock::now();
                    const auto trigger_elapsed = std::chrono::duration_cast<std::chrono::seconds>(trigger_now - trigger_window_start).count();
                    if (trigger_elapsed >= trigger_config_.window_seconds)
                    {
                        if (trigger_match_frames >= trigger_config_.frame_threshold)
                        {
                            trigger_success_count++;
                            std::cout << "[Trigger] window=" << trigger_config_.window_seconds
                                      << "s target_class_id=" << trigger_config_.target_class_id
                                      << " match_frames=" << trigger_match_frames
                                      << " >= threshold=" << trigger_config_.frame_threshold
                                      << " => success_count=" << trigger_success_count
                                      << "/" << trigger_config_.trigger_count << std::endl;
                            if (trigger_success_count >= trigger_config_.trigger_count)
                            {
                                status_.QueueDetectionReport(last_matching_results, trigger_match_frames, trigger_success_count);
                                std::cout << "[Trigger] MQTT report queued: success_count reached "
                                          << trigger_config_.trigger_count << std::endl;
                                trigger_success_count = 0;
                            }
                        }
                        else
                        {
                            std::cout << "[Trigger] window=" << trigger_config_.window_seconds
                                      << "s target_class_id=" << trigger_config_.target_class_id
                                      << " match_frames=" << trigger_match_frames
                                      << " < threshold=" << trigger_config_.frame_threshold
                                      << " => success_count stays " << trigger_success_count
                                      << "/" << trigger_config_.trigger_count << std::endl;
                        }
                        trigger_window_start = trigger_now;
                        trigger_match_frames = 0;
                    }

                    if (ContainsTargetClass(results, trigger_config_.target_class_id))
                    {
                        trigger_match_frames++;
                        last_matching_results = results;
                    }
                }

                draw_results(annotated_frame, results);
                DrawFpsOverlay(annotated_frame, current_fps);
                status_.SetDetectResults(results);
                fwrite(annotated_frame.data, 1, width_ * height_ * 3, ffmpeg);
            }
            else
            {
                DrawFpsOverlay(frame, current_fps);
                fwrite(frame.data, 1, width_ * height_ * 3, ffmpeg);
            }
        }
        else
        {
            DrawFpsOverlay(frame, current_fps);
            fwrite(frame.data, 1, width_ * height_ * 3, ffmpeg);
        }
    }

    cap.release();
    pclose(ffmpeg);
    std::cout << "摄像头推流线程已退出" << std::endl;
}
