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
         std::string pipeline = "v4l2src device=" + std::string(video_name) +
            " ! image/jpeg, width=1920, height=1080, framerate=25/1 ! "
            "jpegdec ! videoconvert ! appsink";
        cap.open(pipeline, cv::CAP_GSTREAMER);

        if (!cap.isOpened())
        {
            std::cerr << "错误：无法打开摄像头 pipeline！" << std::endl;
            return -1;
        }
    }
    else
    {
        cap.open(std::string(video_name));
        std::cout << "打开摄像头成功" << std::endl;
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
        if (frame.empty())
            break;

        detector.submit(frame);

        cv::Mat result_frame;
        object_detect_result_list results;
        if (detector.retrieve(result_frame, results))
        {
            draw_results(result_frame, results);

            cv::imshow("YOLOv8 Detection", result_frame);
            if (cv::waitKey(1) == 27)
                break;

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
