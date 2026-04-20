#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <iostream>

#include "yolov8.h"
#include "postprocess.h"
#include "common.h"
#include "file_utils.h"
#include "image_utils.h"

static void dump_tensor_attr(rknn_tensor_attr* attr)
{
    printf("  index=%d, name=%s, n_dims=%d, dims=[%d, %d, %d, %d], n_elems=%d, size=%d, fmt=%s, type=%s, qnt_type=%s, "
           "zp=%d, scale=%f\n",
           attr->index, attr->name, attr->n_dims, attr->dims[0], attr->dims[1], attr->dims[2], attr->dims[3],
           attr->n_elems, attr->size, get_format_string(attr->fmt), get_type_string(attr->type),
           get_qnt_type_string(attr->qnt_type), attr->zp, attr->scale);
}

rkYolov8::rkYolov8(const char* model_path,float nms_threshold,float box_conf_threshold) {
    this->model_path = model_path;
    this->nms_threshold = nms_threshold;
    this->box_conf_threshold = box_conf_threshold;
}

// 获取类成员变app_ctx 接口
rknn_app_context_t * rkYolov8::Get_app_ctx(){
    return &(this->app_ctx);
}

int rkYolov8::init_yolov8_model(rknn_app_context_t* input_app_ctx,bool share_weight,rknn_core core) {
    int ret;
    int model_len = 0;
    char *model;

    // rknn_context ctx = 0;

    this->app_ctx = *(input_app_ctx);
    
    // Load RKNN Model
    model_len = read_data_from_file(this->model_path.c_str(), &model);
    if (model == NULL)
    {
        printf("load_model fail!\n");
        return -1;
    }

    // 共享权重
    if(true == share_weight){
        ret = rknn_dup_context(&(input_app_ctx->rknn_ctx), &(this->app_ctx.rknn_ctx));
    }
    else{
        std::cout<<"wwwwwwwwwwwwwwww"<<std::endl;
        ret = rknn_init(&(this->app_ctx.rknn_ctx), model, model_len, 0, NULL);
    }

    std::cout<<"wwwwwwwwwwwwwwww"<<std::endl;

    free(model);
    if (ret < 0)
    {
        printf("rknn_init fail! ret=%d\n", ret);
        return -1;
    }

    // 设置运行的NPU核
    rknn_core_mask core_mask;
    switch (core)
    {
    case CORE_AUTO:
        core_mask = RKNN_NPU_CORE_AUTO;
        break;
    case CORE_0:
        core_mask = RKNN_NPU_CORE_0;
        break;
    case CORE_1:
        core_mask = RKNN_NPU_CORE_1;
        break;
    case CORE_2:
        core_mask = RKNN_NPU_CORE_2;
        break;
    case CORE_0_1:
        core_mask = RKNN_NPU_CORE_0_1;
        break; 
    case CORE_0_1_2:
        core_mask = RKNN_NPU_CORE_0_1_2;
        break;
    case CORE_ALL:
        core_mask = RKNN_NPU_CORE_ALL;
        break;
    }
    
    printf("当前的NPU核:%d\n", core_mask);

    ret = rknn_set_core_mask(this->app_ctx.rknn_ctx, core_mask);
    if (ret < 0)
    {
        printf("rknn_init core error ret=%d\n", ret);
        return -1;
    }

    // Get Model Input Output Number
    rknn_input_output_num io_num;
    ret = rknn_query(this->app_ctx.rknn_ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret != RKNN_SUCC)
    {
        printf("rknn_query fail! ret=%d\n", ret);
        return -1;
    }
    printf("model input num: %d, output num: %d\n", io_num.n_input, io_num.n_output);

    // Get Model Input Info
    printf("input tensors:\n");
    rknn_tensor_attr input_attrs[io_num.n_input];
    memset(input_attrs, 0, sizeof(input_attrs));
    for (int i = 0; i < io_num.n_input; i++)
    {
        input_attrs[i].index = i;
        ret = rknn_query(this->app_ctx.rknn_ctx, RKNN_QUERY_INPUT_ATTR, &(input_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC)
        {
            printf("rknn_query fail! ret=%d\n", ret);
            return -1;
        }
        dump_tensor_attr(&(input_attrs[i]));
    }

    // Get Model Output Info
    printf("output tensors:\n");
    rknn_tensor_attr output_attrs[io_num.n_output];
    memset(output_attrs, 0, sizeof(output_attrs));
    for (int i = 0; i < io_num.n_output; i++)
    {
        output_attrs[i].index = i;
        ret = rknn_query(this->app_ctx.rknn_ctx, RKNN_QUERY_OUTPUT_ATTR, &(output_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC)
        {
            printf("rknn_query fail! ret=%d\n", ret);
            return -1;
        }
        dump_tensor_attr(&(output_attrs[i]));
    }


    // TODO
    if (output_attrs[0].qnt_type == RKNN_TENSOR_QNT_AFFINE_ASYMMETRIC && output_attrs[0].type == RKNN_TENSOR_INT8)
    {
        app_ctx.is_quant = true;
    }
    else
    {
        app_ctx.is_quant = false;
    }

    app_ctx.io_num = io_num;
    app_ctx.input_attrs = (rknn_tensor_attr *)malloc(io_num.n_input * sizeof(rknn_tensor_attr));
    memcpy(app_ctx.input_attrs, input_attrs, io_num.n_input * sizeof(rknn_tensor_attr));
    app_ctx.output_attrs = (rknn_tensor_attr *)malloc(io_num.n_output * sizeof(rknn_tensor_attr));
    memcpy(app_ctx.output_attrs, output_attrs, io_num.n_output * sizeof(rknn_tensor_attr));

    if (input_attrs[0].fmt == RKNN_TENSOR_NCHW)
    {
        printf("model is NCHW input fmt\n");
        app_ctx.model_channel = input_attrs[0].dims[1];
        app_ctx.model_height = input_attrs[0].dims[2];
        app_ctx.model_width = input_attrs[0].dims[3];
    }
    else
    {
        printf("model is NHWC input fmt\n");
        app_ctx.model_height = input_attrs[0].dims[1];
        app_ctx.model_width = input_attrs[0].dims[2];
        app_ctx.model_channel = input_attrs[0].dims[3];
    }
    printf("model input height=%d, width=%d, channel=%d\n",
           app_ctx.model_height, app_ctx.model_width, app_ctx.model_channel);

    return 0;
}

object_detect_result_list rkYolov8::inference_yolov8_model(image_buffer_t *img)
{
    int ret;
    image_buffer_t dst_img;
    letterbox_t letter_box;

    object_detect_result_list od_results;

    rknn_input inputs[app_ctx.io_num.n_input];
    rknn_output outputs[app_ctx.io_num.n_output];
    int bg_color = 114;

    if (img == nullptr || img->virt_addr == nullptr || img->width <= 0 || img->height <= 0)
    {
        printf("Error: Invalid image buffer\n");
        return od_results;
    }

    cv::Mat frame(img->height, img->width, CV_8UC3, img->virt_addr);

    memset(&od_results, 0x00, sizeof(od_results));
    memset(&letter_box, 0, sizeof(letterbox_t));
    memset(&dst_img, 0, sizeof(image_buffer_t));
    memset(inputs, 0, sizeof(inputs));
    memset(outputs, 0, sizeof(outputs));

    // Pre Process
    dst_img.width = app_ctx.model_width;
    dst_img.height = app_ctx.model_height;
    dst_img.format = IMAGE_FORMAT_RGB888;
    dst_img.size = get_image_size(&dst_img);
    dst_img.virt_addr = (unsigned char *)malloc(dst_img.size);

    if (dst_img.virt_addr == NULL)
    {
        printf("malloc buffer size:%d fail!\n", dst_img.size);
        return od_results;
    }

    // letterbox
    ret = convert_image_with_letterbox(img, &dst_img, &letter_box, bg_color);
    if (ret < 0)
    {
        printf("convert_image_with_letterbox fail! ret=%d\n", ret);
        return od_results;
    }

    // Set Input Data
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_UINT8;
    inputs[0].fmt = RKNN_TENSOR_NHWC;
    inputs[0].size = app_ctx.model_width * app_ctx.model_height * app_ctx.model_channel;
    inputs[0].buf = dst_img.virt_addr;

    ret = rknn_inputs_set(app_ctx.rknn_ctx, app_ctx.io_num.n_input, inputs);

    if (ret < 0)
    {
        printf("rknn_input_set fail! ret=%d\n", ret);
        return od_results;
    }

    // Run
    // printf("rknn_run\n");
    ret = rknn_run(app_ctx.rknn_ctx, nullptr);
    if (ret < 0)
    {
        printf("rknn_run fail! ret=%d\n", ret);
        return od_results;
    }

    // Get Output
    memset(outputs, 0, sizeof(outputs));
    for (int i = 0; i < app_ctx.io_num.n_output; i++)
    {
        outputs[i].index = i;
        outputs[i].want_float = (!app_ctx.is_quant);
    }
    ret = rknn_outputs_get(app_ctx.rknn_ctx, app_ctx.io_num.n_output, outputs, NULL);
    if (ret < 0)
    {
        printf("rknn_outputs_get fail! ret=%d\n", ret);
        goto out;
    }

    // Post Process
    post_process(&app_ctx, outputs, &letter_box, this->box_conf_threshold, this->nms_threshold, &od_results);

    // 绘制检测框 - 已移至主线程执行以支持多线程推理
    // Drawing moved to main thread to support multi-threaded inference

    // Remeber to release rknn output
    rknn_outputs_release(app_ctx.rknn_ctx, app_ctx.io_num.n_output, outputs);

out:
    if (dst_img.virt_addr != NULL)
    {
        free(dst_img.virt_addr);
    }

    return od_results;
}


rkYolov8::~rkYolov8() {
     if (this->app_ctx.input_attrs != NULL)
    {
        free(this->app_ctx.input_attrs);
        this->app_ctx.input_attrs = NULL;
    }
    if (this->app_ctx.output_attrs != NULL)
    {
        free(this->app_ctx.output_attrs);
        this->app_ctx.output_attrs = NULL;
    }
    if (this->app_ctx.rknn_ctx != 0)
    {
        rknn_destroy(this->app_ctx.rknn_ctx);
        this->app_ctx.rknn_ctx = 0;
    }
}
