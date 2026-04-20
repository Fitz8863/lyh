#ifndef RKNNPOOL_H
#define RKNNPOOL_H

#include "ThreadPool.hpp"
#include "coreNum.hpp"
#include "rknpu2/yolov8.h"
#include <vector>
#include <iostream>
#include <mutex>
#include <queue>
#include <memory>
#include <atomic>
#include <chrono>

template <typename rknnModel, typename inputType, typename outputType>
class rknnPool
{
private:
    int threadNum;
    std::string modelPath;
    float nmsThreshold;
    float boxConfThreshold;
    
    std::mutex queueMtx;
    std::unique_ptr<ThreadPool> pool;
    std::vector<std::shared_ptr<rknnModel>> models;
    std::queue<std::future<outputType>> futures;
    std::atomic<int> model_idx{0};

public:
    rknnPool(const std::string modelPath, int threadNum, float nmsThreshold, float boxConfThreshold);
    int init();
    int put(inputType inputData);
    int get(outputType& outputData);
    int get_nonblock(outputType& outputData);
    bool has_pending();
    ~rknnPool();
};

template <typename rknnModel, typename inputType, typename outputType>
rknnPool<rknnModel, inputType, outputType>::rknnPool(const std::string modelPath, int threadNum, float nmsThreshold, float boxConfThreshold)
{
    this->modelPath = modelPath;
    this->threadNum = threadNum;
    this->nmsThreshold = nmsThreshold;
    this->boxConfThreshold = boxConfThreshold;
}

template <typename rknnModel, typename inputType, typename outputType>
int rknnPool<rknnModel, inputType, outputType>::init()
{
    try
    {
        this->pool = std::make_unique<ThreadPool>(this->threadNum);
        for (int i = 0; i < this->threadNum; i++) {
            models.push_back(std::make_shared<rknnModel>(this->modelPath.c_str(), this->nmsThreshold, this->boxConfThreshold));
        }
    }
    catch (const std::bad_alloc& e)
    {
        std::cout << "Out of memory: " << e.what() << std::endl;
        return -1;
    }
    
    for (int i = 0; i < this->threadNum; i++)
    {
        int core_num = get_core_num();
        rknn_core core;
        switch (core_num)
        {
        case 0:
            core = CORE_0;
            break;
        case 1:
            core = CORE_1;
            break;
        case 2:
            core = CORE_2;
            break;
        default:
            core = CORE_0;
            break;
        }
        std::cout << "Model " << i << " assigned to NPU core " << core_num << std::endl;
        
        int ret = models[i]->init_yolov8_model(models[0]->Get_app_ctx(), i != 0, core);
        if (ret != 0){
            return ret;
        }
    }
    
    std::cout << "Thread pool initialized with " << this->threadNum << " threads" << std::endl;
    return 0;
}

template <typename rknnModel, typename inputType, typename outputType>
int rknnPool<rknnModel, inputType, outputType>::put(inputType inputData)
{
    std::lock_guard<std::mutex> lock(queueMtx);
    
    int idx = model_idx.fetch_add(1) % threadNum;
    
    futures.push(pool->enqueue(&rknnModel::inference_yolov8_model, models[idx], inputData));
    
    return 0;
}

template <typename rknnModel, typename inputType, typename outputType>
int rknnPool<rknnModel, inputType, outputType>::get(outputType& outputData)
{
    std::lock_guard<std::mutex> lock(queueMtx);
    
    if (futures.empty())
    {
        return 1;
    }
    
    std::queue<std::future<outputType>> temp_queue;
    bool found = false;
    
    while (!futures.empty() && !found)
    {
        auto& front = futures.front();
        using namespace std::chrono_literals;
        auto status = front.wait_for(0ms);
        
        if (status == std::future_status::ready && !found)
        {
            outputData = front.get();
            futures.pop();
            found = true;
        }
        else
        {
            temp_queue.push(std::move(futures.front()));
            futures.pop();
        }
    }
    
    while (!temp_queue.empty())
    {
        futures.push(std::move(temp_queue.front()));
        temp_queue.pop();
    }
    
    return found ? 0 : 1;
}

template <typename rknnModel, typename inputType, typename outputType>
int rknnPool<rknnModel, inputType, outputType>::get_nonblock(outputType& outputData)
{
    std::lock_guard<std::mutex> lock(queueMtx);
    
    if (futures.empty())
    {
        return 1;
    }
    
    std::queue<std::future<outputType>> temp_queue;
    bool found = false;
    
    while (!futures.empty() && !found)
    {
        auto& front = futures.front();
        using namespace std::chrono_literals;
        auto status = front.wait_for(0ms);
        
        if (status == std::future_status::ready && !found)
        {
            outputData = front.get();
            futures.pop();
            found = true;
        }
        else
        {
            temp_queue.push(std::move(futures.front()));
            futures.pop();
        }
    }
    
    while (!temp_queue.empty())
    {
        futures.push(std::move(temp_queue.front()));
        temp_queue.pop();
    }
    
    return found ? 0 : 1;
}

template <typename rknnModel, typename inputType, typename outputType>
bool rknnPool<rknnModel, inputType, outputType>::has_pending()
{
    std::lock_guard<std::mutex> lock(queueMtx);
    return !futures.empty();
}

template <typename rknnModel, typename inputType, typename outputType>
rknnPool<rknnModel, inputType, outputType>::~rknnPool()
{
    while (has_pending())
    {
        outputType tmp;
        get(tmp);
    }
}

#endif
