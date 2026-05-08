import os
import cv2
from rknn.api import RKNN
import numpy as np
 
# color_palette
ONNX_MODEL = '/home/fitz/projects/lyh/convert/yolov8s_humanaction_250/weights/best.onnx'
RKNN_MODEL = './best.rknn'

IMG_PATH = './bus.jpg'
DATASET = './datasets.txt'
RESULT_PATH = './result.jpg'

CLASSES = ['falldown','sitting','standup','walking']
# CLASSES = (
#     "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat",
#     "traffic light", "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat",
#     "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe", "backpack",
#     "umbrella", "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard", "sports ball",
#     "kite", "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket",
#     "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
#     "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair",
#     "couch", "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse",
#     "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink", "refrigerator",
#     "book", "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"
# )

QUANTIZE_ON = True
 
OBJ_THRESH = 0.10
NMS_THRESH = 0.45
 
MODEL_SIZE = (640, 640) 
 
color_palette = np.random.uniform(0, 255, size=(len(CLASSES), 3))

def sigmoid(x):
    return 1 / (1 + np.exp(-x))
 
def letter_box(im, new_shape, pad_color=(0,0,0), info_need=False):
    # Resize and pad image while meeting stride-multiple constraints
    shape = im.shape[:2]  # current shape [height, width]
    if isinstance(new_shape, int):
        new_shape = (new_shape, new_shape)
 
    # Scale ratio
    r = min(new_shape[0] / shape[0], new_shape[1] / shape[1])
 
    # Compute padding
    ratio = r  # width, height ratios
    new_unpad = int(round(shape[1] * r)), int(round(shape[0] * r))
    dw, dh = new_shape[1] - new_unpad[0], new_shape[0] - new_unpad[1]  # wh padding
 
    dw /= 2  # divide padding into 2 sides
    dh /= 2
 
    if shape[::-1] != new_unpad:  # resize
        im = cv2.resize(im, new_unpad, interpolation=cv2.INTER_LINEAR)
    top, bottom = int(round(dh - 0.1)), int(round(dh + 0.1))
    left, right = int(round(dw - 0.1)), int(round(dw + 0.1))
    im = cv2.copyMakeBorder(im, top, bottom, left, right, cv2.BORDER_CONSTANT, value=pad_color)  # add border
    
    if info_need is True:
        return im, ratio, (dw, dh)
    else:
        return im
 
def filter_boxes(boxes, box_confidences, box_class_probs):
    """Filter boxes with object threshold.
    """
    box_confidences = box_confidences.reshape(-1)
    candidate, class_num = box_class_probs.shape
 
    class_max_score = np.max(box_class_probs, axis=-1)
    classes = np.argmax(box_class_probs, axis=-1)
 
    _class_pos = np.where(class_max_score* box_confidences >= OBJ_THRESH)
    scores = (class_max_score * box_confidences)[_class_pos]
 
    boxes = boxes[_class_pos]
    classes = classes[_class_pos]
 
    return boxes, classes, scores
 
def nms_boxes(boxes, scores):
    """Suppress non-maximal boxes.
    # Returns
        keep: ndarray, index of effective boxes.
    """
    x = boxes[:, 0]
    y = boxes[:, 1]
    w = boxes[:, 2] - boxes[:, 0]
    h = boxes[:, 3] - boxes[:, 1]
 
    areas = w * h
    order = scores.argsort()[::-1]
 
    keep = []
    while order.size > 0:
        i = order[0]
        keep.append(i)
 
        xx1 = np.maximum(x[i], x[order[1:]])
        yy1 = np.maximum(y[i], y[order[1:]])
        xx2 = np.minimum(x[i] + w[i], x[order[1:]] + w[order[1:]])
        yy2 = np.minimum(y[i] + h[i], y[order[1:]] + h[order[1:]])
 
        w1 = np.maximum(0.0, xx2 - xx1 + 0.00001)
        h1 = np.maximum(0.0, yy2 - yy1 + 0.00001)
        inter = w1 * h1
 
        ovr = inter / (areas[i] + areas[order[1:]] - inter)
        inds = np.where(ovr <= NMS_THRESH)[0]
        order = order[inds + 1]
    keep = np.array(keep)
    return keep
 
def softmax(x, axis=None):
    x = x - x.max(axis=axis, keepdims=True)
    y = np.exp(x)
    return y / y.sum(axis=axis, keepdims=True)
 
def dfl(position):
    # Distribution Focal Loss (DFL)
    n,c,h,w = position.shape
    p_num = 4
    mc = c//p_num
    y = position.reshape(n,p_num,mc,h,w)
    y = softmax(y, 2)
    acc_metrix = np.array(range(mc),dtype=float).reshape(1,1,mc,1,1)
    y = (y*acc_metrix).sum(2)
    return y
 
 
def box_process(position):
    grid_h, grid_w = position.shape[2:4]
    col, row = np.meshgrid(np.arange(0, grid_w), np.arange(0, grid_h))
    col = col.reshape(1, 1, grid_h, grid_w)
    row = row.reshape(1, 1, grid_h, grid_w)
    grid = np.concatenate((col, row), axis=1)
    stride = np.array([MODEL_SIZE[1]//grid_h, MODEL_SIZE[0]//grid_w]).reshape(1,2,1,1)
 
    position = dfl(position)
    box_xy  = grid +0.5 -position[:,0:2,:,:]
    box_xy2 = grid +0.5 +position[:,2:4,:,:]
    xyxy = np.concatenate((box_xy*stride, box_xy2*stride), axis=1)
 
    return xyxy
 
def post_process(input_data):
    boxes, scores, classes_conf = [], [], []
    defualt_branch=3
    pair_per_branch = len(input_data)//defualt_branch
    # Python 忽略 score_sum 输出
    for i in range(defualt_branch):
        boxes.append(box_process(input_data[pair_per_branch*i]))
        classes_conf.append(input_data[pair_per_branch*i+1])
        scores.append(np.ones_like(input_data[pair_per_branch*i+1][:,:1,:,:], dtype=np.float32))
 
    def sp_flatten(_in):
        ch = _in.shape[1]
        _in = _in.transpose(0,2,3,1)
        return _in.reshape(-1, ch)
 
    boxes = [sp_flatten(_v) for _v in boxes]
    classes_conf = [sp_flatten(_v) for _v in classes_conf]
    scores = [sp_flatten(_v) for _v in scores]
 
    boxes = np.concatenate(boxes)
    classes_conf = np.concatenate(classes_conf)
    scores = np.concatenate(scores)
 
    # filter according to threshold
    boxes, classes, scores = filter_boxes(boxes, scores, classes_conf)
 
    # nms
    nboxes, nclasses, nscores = [], [], []
    for c in set(classes):
        inds = np.where(classes == c)
        b = boxes[inds]
        c = classes[inds]
        s = scores[inds]
        keep = nms_boxes(b, s)
 
        if len(keep) != 0:
            nboxes.append(b[keep])
            nclasses.append(c[keep])
            nscores.append(s[keep])
 
    if not nclasses and not nscores:
        return None, None, None
 
    boxes = np.concatenate(nboxes)
    classes = np.concatenate(nclasses)
    scores = np.concatenate(nscores)
 
    return boxes, classes, scores
 
def draw_detections(img, left, top, right, bottom, score, class_id):
    """
    Draws bounding boxes and labels on the input image based on the detected objects.
    Args:
        img: The input image to draw detections on.
        box: Detected bounding box.
        score: Corresponding detection score.
        class_id: Class ID for the detected object.
    Returns:
        None
    """
 
    # Retrieve the color for the class ID
    color = color_palette[class_id]
 
    # Draw the bounding box on the image
    cv2.rectangle(img, (int(left), int(top)), (int(right), int(bottom)), color, 2)
    
    # 计算矩形框的中心点坐标
    center_point = (int((left + right) / 2), int((top + bottom) / 2))
 
    # 在中心点画一个标记点
    cv2.circle(img, center_point, 5, (0, 0, 255), -1)
    
    # Create the label text with class name and score
    label = f"{CLASSES[class_id]}: {score:.2f}"
 
    # Calculate the dimensions of the label text
    (label_width, label_height), _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 1)
 
    # Calculate the position of the label text
    label_x = left
    label_y = top - 10 if top - 10 > label_height else top + 10
 
    # Draw a filled rectangle as the background for the label text
    cv2.rectangle(img, (label_x, label_y - label_height), (label_x + label_width, label_y + label_height), color,
                  cv2.FILLED)
 
    # Draw the label text on the image
    cv2.putText(img, label, (label_x, label_y), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 0), 1, cv2.LINE_AA)
 
 
def draw(image, boxes, scores, classes):
    img_h, img_w = image.shape[:2]
    # Calculate scaling factors for bounding box coordinates
    if(img_w >= img_h):
        x_factor = img_w / MODEL_SIZE[0]
        y_rsz = img_h * MODEL_SIZE[1] / img_w
        y_factor = img_h / y_rsz
        offset = (MODEL_SIZE[1] - y_rsz) / 2
    else:
        y_factor = img_h / MODEL_SIZE[1]
        x_rsz = img_w * MODEL_SIZE[0] / img_h
        x_factor = img_w / x_rsz
        offset = (MODEL_SIZE[0] - x_rsz) / 2
 
    for box, score, cl in zip(boxes, scores, classes):
        
        x1, y1, x2, y2 = [int(_b) for _b in box]
 
        if(img_w >= img_h):
            left = int(x1 * x_factor)
            top = int((y1 - offset) * y_factor)  
            right = int(x2 * x_factor)
            bottom = int((y2 - offset) * y_factor) 
        else:
            left = int((x1 - offset) * x_factor)
            top = int(y1 * y_factor)  
            right = int((x2 - offset) * x_factor)
            bottom = int(y2 * y_factor) 
 
        print('class: {}, score: {}'.format(CLASSES[cl], score))
        print('box coordinate left,top,right,down: [{}, {}, {}, {}]'.format(left, top, right, bottom))
 
        # Retrieve the color for the class ID
        
        draw_detections(image, left, top, right, bottom, score, cl)
 
        # cv2.rectangle(image, (left, top), (right, bottom), color, 2)
        # cv2.putText(image, '{0} {1:.2f}'.format(CLASSES[cl], score),
        #             (left, top - 6),
        #             cv2.FONT_HERSHEY_SIMPLEX,
        #             0.6, (0, 0, 255), 2)
def resize_image(image, size, letterbox_image):
    """
        对输入图像进行resize
    Args:
        size:目标尺寸
        letterbox_image: bool 是否进行letterbox变换
    Returns:指定尺寸的图像
    """
    from PIL import Image
    ih, iw, _ = image.shape
    h, w = size
    if letterbox_image:
        scale = min(w/iw, h/ih)       # 缩放比例
        nw = int(iw*scale)
        nh = int(ih*scale)
        image = cv2.resize(image, (nw, nh), interpolation=cv2.INTER_LINEAR)
        # 生成画布
        image_back = np.ones((h, w, 3), dtype=np.uint8) * 128
        # 将image放在画布中心区域-letterbox
        image_back[(h-nh)//2: (h-nh)//2 + nh, (w-nw)//2:(w-nw)//2+nw, :] = image
    else:
        image_back = image
    return image_back
 
if __name__ == '__main__':
 
    # Create RKNN object
    rknn = RKNN(verbose=False)

    # pre-process config
    print('--> Config model')
    rknn.config(mean_values=[[0, 0, 0]], std_values=[[255, 255, 255]], 
                target_platform='rk3588')
    print('done')

    # Load ONNX model
    print('--> Loading model')
    ret = rknn.load_onnx(model=ONNX_MODEL) # ,outputs=['output', '522', '534'],outputs=['495', '515', '535']
    if ret != 0:
        print('Load model failed!')
        exit(ret)
    print('done')

    # Build model
    print('--> Building model')
    ret = rknn.build(do_quantization=QUANTIZE_ON, dataset=DATASET)  #  QUANTIZE_ON
    if ret != 0:
        print('Build model failed!')
        exit(ret)
    print('done')


    # Export RKNN model
    print('--> Export rknn model')
    ret = rknn.export_rknn(RKNN_MODEL)
    if ret != 0:
        print('Export rknn model failed!')
        exit(ret)
    print('done')

    
    # Init runtime environment
    print('--> Init runtime environment')
    ret = rknn.init_runtime()
    if ret != 0:
        print('Init runtime environment failed!')
        exit(ret)
    print('量化导出完成')
 
 
#  推理
    img = cv2.imread(IMG_PATH)

        
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    img = cv2.resize(img, MODEL_SIZE)
    
    input = np.expand_dims(img, axis=0)

    outputs = rknn.inference([input])
    
    boxes, classes, scores = post_process(outputs)

    img_p = cv2.cvtColor(img, cv2.COLOR_RGB2BGR)

    if boxes is not None:
        
        draw(img_p, boxes, scores, classes)
        cv2.imwrite(RESULT_PATH, img_p)
    else:
        print("没有检测出东西")
    # print('Detection result save to {}'.format(result_path))

        
    rknn.release()