from ultralytics import YOLO

# 加载你训练好的模型
model = YOLO("/home/fitz/projects/lyh/convert/yolov8s_humanaction_250/weights/best.pt")

# 开始导出
model.export(
    format="rknn", 
    imgsz=640,         
    half=False,        # CPU 建议关掉 FP16，使用默认 FP32
    int8=False,        # 如果不需要量化，保持 False
    dynamic=False,     
    simplify=True,     # 必须开启，去掉冗余算子
    opset=12           # 增加兼容性，建议指定 opset
)
