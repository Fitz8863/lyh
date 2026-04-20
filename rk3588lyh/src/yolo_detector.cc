#include "yolo_detector.h"
#include "rknnPool.hpp"
#include "image_utils.h"
#include <queue>
#include <memory>

struct FrameTask {
    cv::Mat frame;
    image_buffer_t* img_buffer;
};

struct YoloDetector::Impl {
    std::string model_path;
    float nms_threshold;
    float box_conf_threshold;
    int thread_num;
    std::queue<FrameTask> pending_frames;
    int max_pending;
    rknnPool<rkYolov8, image_buffer_t*, object_detect_result_list>* pool;
};

YoloDetector::YoloDetector(const std::string& model_path, int thread_num,float nms_threshold,float box_conf_threshold)
{
    impl = new Impl();
    impl->model_path = model_path;
    impl->thread_num = thread_num;
    impl->box_conf_threshold = box_conf_threshold;
    impl->nms_threshold = nms_threshold;
    impl->max_pending = thread_num;
    impl->pool = nullptr;
}

YoloDetector::~YoloDetector()
{
    if (impl)
    {
        flush();
        delete impl->pool;
        delete impl;
    }
}

int YoloDetector::init()
{
    init_post_process();

    impl->pool = new rknnPool<rkYolov8, image_buffer_t*, object_detect_result_list>(
        impl->model_path, impl->thread_num, impl->nms_threshold, impl->box_conf_threshold);

    int ret = impl->pool->init();
    if (ret != 0)
    {
        printf("Thread pool init fail! ret=%d model_path=%s\n", ret, impl->model_path.c_str());
        return -1;
    }

    return 0;
}

void YoloDetector::submit(const cv::Mat& frame)
{
    if (frame.empty() || frame.data == nullptr) return;
    if (impl->pending_frames.size() >= impl->max_pending) return;

    int width = frame.cols;
    int height = frame.rows;
    int stride = frame.step.p[0];
    int channel = frame.channels();
    if (width <= 0 || height <= 0) return;

    image_buffer_t* src_image = new image_buffer_t();
    memset(src_image, 0, sizeof(image_buffer_t));
    src_image->width = width;
    src_image->height = height;
    src_image->format = IMAGE_FORMAT_RGB888;
    src_image->size = width * height * 3;

    unsigned char* img_data = new unsigned char[src_image->size];
    if (img_data == nullptr)
    {
        delete src_image;
        return;
    }

    cv::Mat rgb_frame;
    cv::cvtColor(frame, rgb_frame, cv::COLOR_BGR2RGB);

    if (rgb_frame.step.p[0] == width * 3)
    {
        memcpy(img_data, rgb_frame.data, src_image->size);
    }
    else
    {
        for (int y = 0; y < height; y++)
        {
            memcpy(img_data + y * width * 3, rgb_frame.data + y * rgb_frame.step.p[0], width * 3);
        }
    }
    src_image->virt_addr = img_data;

    FrameTask task;
    task.frame = frame.clone();
    task.img_buffer = src_image;

    impl->pending_frames.push(task);
    impl->pool->put(src_image);
}

bool YoloDetector::retrieve(cv::Mat& out_frame, object_detect_result_list& results)
{
    if (impl->pending_frames.empty()) return false;

    int ret = impl->pool->get(results);
    if (ret != 0) return false;

    if (impl->pending_frames.empty())
    {
        memset(&results, 0, sizeof(results));
        return false;
    }

    FrameTask& front_task = impl->pending_frames.front();

    if (front_task.frame.empty() || front_task.img_buffer == nullptr || front_task.img_buffer->virt_addr == nullptr)
    {
        delete[] front_task.img_buffer->virt_addr;
        delete front_task.img_buffer;
        impl->pending_frames.pop();
        memset(&results, 0, sizeof(results));
        return false;
    }

    out_frame = front_task.frame.clone();

    delete[] front_task.img_buffer->virt_addr;
    delete front_task.img_buffer;
    impl->pending_frames.pop();

    return true;
}

bool YoloDetector::empty() const
{
    return impl->pending_frames.empty();
}

void YoloDetector::flush()
{
    while (!impl->pending_frames.empty())
    {
        object_detect_result_list results;
        if (impl->pool->get(results) == 0)
        {
            FrameTask& front_task = impl->pending_frames.front();
            delete[] front_task.img_buffer->virt_addr;
            delete front_task.img_buffer;
            impl->pending_frames.pop();
        }
    }
}
