# yolov8

## 目录

- [1. 说明](#1-说明)
- [2. 当前支持平台](#2-当前支持平台)
- [3. 预训练模型](#3-预训练模型)
- [4. 转换为RKNN](#4-转换为rknn)
- [5. Python示例](#5-python示例)
- [6. Android示例](#6-android示例)
  - [6.1 编译构建](#61-编译构建)
  - [6.2 推送demo文件到设备](#62-推送demo文件到设备)
  - [6.3 运行demo](#63-运行demo)
- [7. Linux示例](#7-linux示例)
  - [7.1 编译构建](#71-编译构建)
  - [7.2 推送demo文件到设备](#72-推送demo文件到设备)
  - [7.3 运行demo](#73-运行demo)
- [8. 预期结果](#8-预期结果)



## 1. 说明

本示例使用的模型来自以下开源项目：  

https://github.com/airockchip/ultralytics_yolov8



## 2. 当前支持平台

RK3562, RK3566, RK3568, RK3576, RK3588, RV1126B, RV1109, RV1126, RK1808, RK3399PRO


## 3. 预训练模型

下载链接： 

[./yolov8n.onnx](https://ftrg.zbox.filez.com/v2/delivery/data/95f00b0fc900458ba134f8b180b3f7a1/examples/yolov8/yolov8n.onnx)<br />[./yolov8s.onnx](https://ftrg.zbox.filez.com/v2/delivery/data/95f00b0fc900458ba134f8b180b3f7a1/examples/yolov8/yolov8s.onnx)<br />[./yolov8m.onnx](https://ftrg.zbox.filez.com/v2/delivery/data/95f00b0fc900458ba134f8b180b3f7a1/examples/yolov8/yolov8m.onnx)

使用shell命令下载：

```
cd model
./download_model.sh
```

**注意**：此处提供的模型为优化后的模型，与官方原始模型有所不同。以 yolov8n.onnx 为例，说明它们之间的区别。
1. 输出信息对比如下。左侧为官方原始模型，右侧为优化后的模型。如图所示，原始模型的一个输出被分为三组。例如，在输出组 ([1,64,80,80],[1,80,80,80],[1,1,80,80]) 中，[1,64,80,80] 为边界框坐标，[1,80,80,80] 为对应80个类别的边界框置信度，[1,1,80,80] 为80个类别置信度之和。

<div align=center>
  <img src="./model_comparison/yolov8_output_comparison.jpg" alt="Image">
</div>

2. 以输出组 ([1,64,80,80],[1,80,80,80],[1,1,80,80]) 为例，我们移除了模型中两个卷积节点后面的子图，保留这两个卷积的输出 ([1,64,80,80],[1,80,80,80])，并添加了一个 reducesum+clip 分支用于计算80个类别置信度之和 ([1,1,80,80])。

<div align=center>
  <img src="./model_comparison/yolov8_graph_comparison.jpg" alt="Image">
</div>


## 4. 转换为RKNN

*用法：*

```shell
cd python
python convert.py <onnx_model> <TARGET_PLATFORM> <dtype(可选)> <output_rknn_path(可选)>

# 例如： 
python convert.py ../model/yolov8n.onnx rk3588
# 输出模型将保存为 ../model/yolov8.rknn
```

*参数说明：*

- `<onnx_model>`：指定ONNX模型路径。
- `<TARGET_PLATFORM>`：指定NPU平台名称。例如 'rk3588'。
- `<dtype>(可选)`：指定为 `i8`、`u8` 或 `fp`。`i8`/`u8` 表示进行量化，`fp` 表示不进行量化。默认为 `i8`。
- `<output_rknn_path>(可选)`：指定RKNN模型的保存路径，默认保存在ONNX模型所在目录下，文件名为 `yolov8.rknn`



## 5. Python示例

*用法：*

```shell
cd python
# 使用PyTorch模型或ONNX模型推理
python yolov8.py --model_path <pt_model/onnx_model> --img_show

# 使用RKNN模型推理
python yolov8.py --model_path <rknn_model> --target <TARGET_PLATFORM> --img_show
```

*参数说明：*

- `<TARGET_PLATFORM>`：指定NPU平台名称。例如 'rk3588'。

- `<pt_model / onnx_model / rknn_model>`：指定模型路径。



## 6. Android示例

**注意：RK1808、RV1109、RV1126 不支持Android。**

#### 6.1 编译构建

请参考 [编译环境搭建指南](../../docs/Compilation_Environment_Setup_Guide.md#android-platform) 文档来搭建交叉编译环境并完成 C/C++ Demo 的编译。  
**注意：请将模型名称替换为 `yolov8`。**

#### 6.2 推送demo文件到设备

通过USB端口连接设备后，推送demo文件到设备：

```shell
adb root
adb remount
adb push install/<TARGET_PLATFORM>_android_<ARCH>/rknn_yolov8_demo/ /data/
```

#### 6.3 运行demo

```sh
adb shell
cd /data/rknn_yolov8_demo

export LD_LIBRARY_PATH=./lib
./rknn_yolov8_demo model/yolov8.rknn model/bus.jpg
```

- 运行后，结果保存为 `out.png`。要在主机PC上查看结果，使用以下命令拉取：

  ```sh
  adb pull /data/rknn_yolov8_demo/out.png
  ```

- 输出结果参考 [预期结果](#8-预期结果)。



## 7. Linux示例

#### 7.1 编译构建

请参考 [编译环境搭建指南](../../docs/Compilation_Environment_Setup_Guide.md#linux-platform) 文档来搭建交叉编译环境并完成 C/C++ Demo 的编译。
**注意：请将模型名称替换为 `yolov8`。**

#### 7.2 推送demo文件到设备

- 如果设备通过USB端口连接，推送demo文件到设备：

```shell
adb push install/<TARGET_PLATFORM>_linux_<ARCH>/rknn_yolov8_demo/ /userdata/
```

- 对于其他开发板，使用 `scp` 或其他方式将 `install/<TARGET_PLATFORM>_linux_<ARCH>/rknn_yolov8_demo/` 下的所有文件推送到 `userdata`。

#### 7.3 运行demo

```sh
adb shell
cd /userdata/rknn_yolov8_demo

export LD_LIBRARY_PATH=./lib
./rknn_yolov8_demo model/yolov8.rknn model/bus.jpg
```

- 运行后，结果保存为 `out.png`。要在主机PC上查看结果，使用以下命令拉取：

  ```
  adb pull /userdata/rknn_yolov8_demo/out.png
  ```

- 输出结果参考 [预期结果](#8-预期结果)。



## 8. 预期结果

本示例将打印测试图像检测结果的标签及对应分数，如下所示：

```
person @ (211 241 283 507) 0.873
person @ (109 235 225 536) 0.866
person @ (476 222 560 521) 0.863
bus @ (99 136 550 456) 0.859
person @ (80 326 116 513) 0.311
```

<img src="result.png">

- 注意：不同平台、不同版本的工具和驱动，结果可能略有差异。
