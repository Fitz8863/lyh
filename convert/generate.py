import os

# 定义目录
image_folder = '/home/fitz/projects/ultralytics/convert/images'
output_path = 'datasets.txt'

# 获取文件列表并排序
files = sorted(os.listdir(image_folder))

with open(output_path, 'w', encoding='utf-8') as f:
    for filename in files:
        # 拼接出相对路径 (比如: image/1.jpg)
        file_path = os.path.join(image_folder, filename)
        
        # 确保只记录文件（排除子目录）
        if os.path.isfile(file_path):
            # 【关键修改点】：将相对路径转换为绝对路径
            abs_path = os.path.abspath(file_path) 
            
            # 写入绝对路径
            f.write(abs_path + '\n')

print(f"文件已保存至 {output_path}，已生成绝对路径！")