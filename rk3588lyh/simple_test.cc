#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>

int main() {
    int width = 1920;
    int height = 1080;
    int fps = 25;

    std::string read_pipeline =
        "v4l2src device=/dev/video0 ! "
        "image/jpeg, width=(int)" + std::to_string(width) +
        ", height=(int)" + std::to_string(height) +
        ", framerate=" + std::to_string(fps) + "/1 ! "
        "jpegdec ! "
        "videoconvert ! video/x-raw, format=BGR ! "
        "appsink drop=1 max-buffers=1";

    std::cout << "正在打开摄像头 (MJPEG " << width << "x" << height << "@" << fps << "fps)..." << std::endl;

    cv::VideoCapture cap(read_pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened()) {
        std::cerr << "错误：无法打开摄像头 pipeline！" << std::endl;
        return -1;
    }

    std::string cmd =
        "ffmpeg -y "
        "-f rawvideo -pix_fmt bgr24 -s " + std::to_string(width) + "x" + std::to_string(height) +
        " -r " + std::to_string(fps) +
        " -i - "
        "-c:v h264_rkmpp "
        "-fflags nobuffer -flags low_delay "
        "-rtsp_transport udp "
        "-f rtsp rtsp://10.60.83.159:8554/rk3588lyh";

    FILE* ffmpeg = popen(cmd.c_str(), "w");
    if (!ffmpeg) {
        std::cerr << "错误：无法打开 ffmpeg 管道！" << std::endl;
        return -1;
    }

    std::cout << "RTSP 推流已启动" << std::endl;

    cv::Mat frame;
    int count = 0;
    while (count < 300) {
        cap >> frame;
        if (frame.empty()) break;
        fwrite(frame.data, 1, width * height * 3, ffmpeg);
        count++;
    }

    cap.release();
    pclose(ffmpeg);
    std::cout << "推流完成" << std::endl;
    return 0;
}
