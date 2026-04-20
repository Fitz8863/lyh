#include <opencv2/opencv.hpp>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        std::cerr << "Usage: camera_capture_helper <device> <width> <height> <fps>" << std::endl;
        return 1;
    }

    const std::string device = argv[1];
    const int width = std::stoi(argv[2]);
    const int height = std::stoi(argv[3]);
    const int fps = std::stoi(argv[4]);

    std::string read_pipeline =
        "v4l2src device=" + device + " ! "
        "image/jpeg, width=(int)" + std::to_string(width) +
        ", height=(int)" + std::to_string(height) +
        ", framerate=" + std::to_string(fps) + "/1 ! "
        "jpegdec ! "
        "videoconvert ! video/x-raw, format=BGR ! "
        "appsink drop=1 max-buffers=1 sync=false";

    cv::VideoCapture cap(read_pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened())
    {
        std::cerr << "错误：helper 无法打开摄像头 pipeline！" << std::endl;
        return 2;
    }

    setvbuf(stdout, nullptr, _IONBF, 0);

    const size_t frame_bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 3U;
    cv::Mat frame;
    while (true)
    {
        cap >> frame;
        if (frame.empty())
        {
            break;
        }

        const cv::Mat &output = frame.isContinuous() ? frame : frame.clone();
        const size_t written = fwrite(output.data, 1, frame_bytes, stdout);
        if (written != frame_bytes)
        {
            break;
        }
    }

    cap.release();
    return 0;
}
