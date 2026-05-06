from ultralytics import YOLO

# 加载你训练好的模型
model = YOLO("/home/fitz/projects/lyh/convert/yolov8s_humanaction_250/weights/best.pt")

# 开始导出
model.export(
    format="onnx", 
    imgsz=640,         
    half=False,        # CPU 建议关掉 FP16，使用默认 FP32
    int8=False,        # 如果不需要量化，保持 False
    dynamic=False,     
    simplify=True,     # 必须开启，去掉冗余算子
    opset=12           # 增加兼容性，建议指定 opset
)

# from ultralytics import YOLO

# # 加载你训练好的模型
# model = YOLO("/home/fitz/projects/ultralytics/models/yolo26n.pt")

# print("开始导出 i5-1240P 专属加速模型...")

# # 优化后的导出参数
# model.export(
#     format="openvino", # 核心优化1：直接导出为 OpenVINO 格式
#     imgsz=640,         
#     half=True,         # 优化2：开启半精度，降低内存带宽压力
#     int8=True,         # 核心优化3：开启 INT8 量化，激活 i5 的 VNNI 硬件加速！
#     dynamic=False,     # 保持 False，固定尺寸推理最快
#     simplify=True,
    
#     # 【极其重要】：开启 INT8 量化必须提供数据集的 yaml 文件！
#     # 因为量化过程需要跑几张真实的图片来校准激活值的范围（Calibration）
#     # 如果你是自定义数据集，请换成你训练时用的 yaml 路径
#     data="/home/fitz/projects/ultralytics/ultralytics/cfg/datasets/coco128.yaml" 
# )

# print("导出完成！请使用生成的 _openvino_model 文件夹进行推理。")
