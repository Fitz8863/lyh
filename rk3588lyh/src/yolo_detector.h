#ifndef _YOLO_DETECTOR_H_
#define _YOLO_DETECTOR_H_

#include <opencv2/opencv.hpp>
#include <string>
#include "postprocess.h"

class YoloDetector
{
public:
    YoloDetector(const std::string& model_path, int thread_num = 3,float nms_threshold = 0.45,float box_conf_threshold = 0.25);
    ~YoloDetector();

    int init();
    void submit(const cv::Mat& frame);
    bool retrieve(cv::Mat& out_frame, object_detect_result_list& results);
    bool empty() const;
    void flush();

private:
    struct Impl;
    Impl* impl;
};

#endif
