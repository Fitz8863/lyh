/**
 * RK3588 YOLOv8 多线程 NPU 推理 Demo
 */
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <sys/time.h>
#include "yolo_detector.h"

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Usage: %s <model_path> <video_path or camera_id> [thread_num]\n", argv[0]);
        printf("Example: %s model.rknn 0           # use camera\n", argv[0]);
        printf("         %s model.rknn video.mp4 3  # video, 3 threads\n", argv[0]);
        return -1;
    }

    const char *model_path = argv[1];
    const char *video_name = argv[2];
    int thread_num = (argc >= 4) ? atoi(argv[3]) : 3;

    YoloDetector detector(model_path, thread_num);
    if (detector.init() != 0)
    {
        return -1;
    }

    cv::VideoCapture cap;
    if (std::string(video_name).find("/dev/video") == 0)
    {
        cap.open(0, cv::CAP_V4L2);

        // 打开后立即设置参数
        cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G')); // 强制使用 MJPEG 格式
        cap.set(cv::CAP_PROP_FRAME_WIDTH, 1920);  // 宽
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 1080); // 高
        cap.set(cv::CAP_PROP_FPS, 30);            // 帧率

        if (!cap.isOpened())
        {
            printf("Failed to open V4L2 device\n");
            return -1;
        }
    }
    else
    {
        cap.open(std::string(video_name));
    }

    if (!cap.isOpened())
    {
        printf("Failed to open video source: %s\n", video_name);
        return -1;
    }

    struct timeval time;
    gettimeofday(&time, nullptr);
    auto beforeTime = time.tv_sec * 1000 + time.tv_usec / 1000;
    int fps_count = 0;

    cv::Mat frame;
    while (true)
    {
        cap >> frame;
        if (frame.empty()) break;

        detector.submit(frame);

        cv::Mat result_frame;
        object_detect_result_list results;
        if (detector.retrieve(result_frame, results))
        {
            draw_results(result_frame, results);

            cv::imshow("YOLOv8 Detection", result_frame);
            if (cv::waitKey(1) == 27) break;

            fps_count++;
            if (fps_count >= 60)
            {
                gettimeofday(&time, nullptr);
                auto currentTime = time.tv_sec * 1000 + time.tv_usec / 1000;
                printf("60 frames average FPS: %.2f\n", 60000.0 / (currentTime - beforeTime));
                beforeTime = currentTime;
                fps_count = 0;
            }
        }
    }

    cv::destroyAllWindows();
    return 0;
}
