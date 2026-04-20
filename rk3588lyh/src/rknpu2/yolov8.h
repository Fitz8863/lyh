// Copyright (c) 2023 by Rockchip Electronics Co., Ltd. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _RKNN_DEMO_YOLOV8_H_
#define _RKNN_DEMO_YOLOV8_H_

#include <string>
#include <mutex>
#include "common_types.h"

typedef enum rknn_core_enum{
    CORE_AUTO,  /* default, run on NPU core randomly. */
    CORE_0,     /* run on NPU core 0. */
    CORE_1,     /* run on NPU core 1. */
    CORE_2,     /* run on NPU core 2. */
    CORE_0_1,   /* run on NPU core 0 and core 1. */
    CORE_0_1_2, /* run on NPU core 0 and core 1 and core 2. */
    CORE_ALL,
}rknn_core;

class rkYolov8
{
private:
    std::mutex mtx;

    std::string model_path;
    float nms_threshold, box_conf_threshold;
    rknn_app_context_t app_ctx;

public:
    rkYolov8(const char *model_path,float nms_threshold,float box_conf_threshold);
    int init_yolov8_model(rknn_app_context_t *input_app_ctx, bool share_weight, rknn_core core = CORE_0_1_2);
    rknn_app_context_t *Get_app_ctx();
    object_detect_result_list inference_yolov8_model(image_buffer_t *img);
    ~rkYolov8();
};

#endif
