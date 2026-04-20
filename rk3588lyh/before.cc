#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>

int main()
{
    // ====================== 参数配置 ======================
    int width = 1920;
    int height = 1080;
    int fps = 25;

    std::string device = "/dev/video0";
    std::string rtsp_url = "rtsp://10.60.83.159:8554/rk3588";

    // ====================== Pipeline 1：读取摄像头 (MJPEG) ======================
    std::string read_pipeline =
        "v4l2src device=" + device + " ! "
                                     "image/jpeg, width=(int)" +
        std::to_string(width) +
        ", height=(int)" + std::to_string(height) +
        ", framerate=" + std::to_string(fps) + "/1 ! "
                                               "jpegdec ! "
                                               "videoconvert ! video/x-raw, format=BGR ! "
                                               "appsink drop=1 max-buffers=1";

    std::cout << "正在打开摄像头 (MJPEG " << width << "x" << height << "@" << fps << "fps)..." << std::endl;

    cv::VideoCapture cap(read_pipeline, cv::CAP_GSTREAMER);

    if (!cap.isOpened())
    {
        std::cerr << "错误：无法打开摄像头 pipeline！" << std::endl;
        return -1;
    }

    std::cout << "摄像头打开成功！开始推流..." << std::endl;

    // ====================== Pipeline 2：RK3588 硬件编码推流 ======================

    std::string cmd =
        "ffmpeg -y "
        "-f rawvideo -pix_fmt bgr24 -s " + std::to_string(width) + "x" + std::to_string(height) +
        " -r " + std::to_string(fps) +
        " -i - "
        "-c:v h264_rkmpp " // 移除了不支持的 -preset 和 -tune
        "-b:v 4M "         // 建议加上码率控制，否则画面可能会很糊或带宽波动大
        "-fflags nobuffer -flags low_delay "
        "-rtsp_transport udp "
        "-f rtsp  rtsp://10.60.83.159:8554/rk3588";


    // 打开管道写入 FFmpeg
    FILE *ffmpeg = popen(cmd.c_str(), "w");
    if (!ffmpeg)
    {
        std::cerr << "Failed to open ffmpeg pipe!" << std::endl;
        return -1;
    }

    // cv::VideoWriter writer(push_pipeline, cv::CAP_GSTREAMER, 0, fps, cv::Size(width, height), true);

    // if (!writer.isOpened())
    // {
    //     std::cerr << "错误：无法打开推流 pipeline！" << std::endl;
    //     std::cerr << "请运行下面命令查看 rtspclientsink 支持的属性：" << std::endl;
    //     std::cerr << "gst-inspect-1.0 rtspclientsink" << std::endl;
    //     return -1;
    // }

    std::cout << "RTSP 推流已启动 → " << rtsp_url << " (8Mbps)" << std::endl;

    // ====================== 主循环 ======================
    cv::Mat frame;
    while (true)
    {
        cap >> frame;

        if (frame.empty())
        {
            std::cerr << "读取摄像头帧失败！" << std::endl;
            break;
        }

        fwrite(frame.data, 1, width * height * 3, ffmpeg);
    }

    cap.release();
    // writer.release();
    cv::destroyAllWindows();

    std::cout << "程序已退出，推流停止。" << std::endl;
    return 0;
}