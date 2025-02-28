import os
import random
import math
import numpy as np

def generate_orthogonal_vectors(n):
    # 生成一对正交向量
    v1 = np.random.randn(n)
    v1 = v1 / np.linalg.norm(v1)
    
    # 生成一个随机向量并使其与v1正交
    v2 = np.random.randn(n)
    v2 = v2 - np.dot(v2, v1) * v1
    v2 = v2 / np.linalg.norm(v2)
    
    return v1, v2

def generate_vectors_with_angle(n, angle_degrees):
    # 生成具有特定夹角的向量对
    v1 = np.random.randn(n)
    v1 = v1 / np.linalg.norm(v1)
    
    # 生成一个随机向量作为基础
    v2_base = np.random.randn(n)
    v2_base = v2_base - np.dot(v2_base, v1) * v1
    v2_base = v2_base / np.linalg.norm(v2_base)
    
    # 根据指定角度旋转v2
    angle_rad = math.radians(angle_degrees)
    v2 = v1 * math.cos(angle_rad) + v2_base * math.sin(angle_rad)
    
    return v1, v2

def generate_random_vectors(n):
    # 生成两个随机单位向量
    v1 = np.random.randn(n)
    v1 = v1 / np.linalg.norm(v1)
    
    v2 = np.random.randn(n)
    v2 = v2 / np.linalg.norm(v2)
    
    # 计算夹角
    cos_theta = np.dot(v1, v2)
    angle = math.degrees(math.acos(max(min(cos_theta, 1), -1)))
    
    return v1, v2, angle

def format_vector(v):
    return ' '.join(f'{x:.6f}' for x in v)

base_dir = 'd:\\Projects\\C\\code\\测试数据\\数据结构与算法\\107\\10702'
os.makedirs(base_dir, exist_ok=True)

# 生成20组测试数据（10组原始数据 + 10组新的随机数据）
test_cases = [
    (2, 90),      # 2维正交向量
    (3, 0),       # 3维相同向量
    (4, 180),     # 4维相反向量
    (5, 90),      # 5维正交向量
    (10, 14.42),  # 10维特定角度
    (20, 90),     # 20维正交向量
    (50, 90),     # 50维正交向量
    (100, 180),   # 100维相反向量
    (3, 90),      # 3维正交向量（带小数）
    (4, 19.47),   # 4维特定角度
    (20, None),   # 随机向量对
    (30, None),   # 随机向量对
    (40, None),   # 随机向量对
    (50, None),   # 随机向量对
    (60, None),   # 随机向量对
    (70, None),   # 随机向量对
    (80, None),   # 随机向量对
    (90, None),   # 随机向量对
    (95, None),   # 随机向量对
    (100, None)   # 随机向量对
]

for i, (n, angle) in enumerate(test_cases, 1):
    with open(os.path.join(base_dir, f'{i}.in'), 'w') as f:
        f.write(f'{n}\n')
        if angle is None:
            # 随机向量对
            v1, v2, actual_angle = generate_random_vectors(n)
            f.write(f'{format_vector(v1)}\n')
            f.write(f'{format_vector(v2)}\n')
            angle = actual_angle
        elif angle == 0:
            # 相同向量
            v = np.ones(n) / math.sqrt(n)
            f.write(f'{format_vector(v)}\n')
            f.write(f'{format_vector(v)}\n')
        elif angle == 180:
            # 相反向量
            v = np.ones(n) / math.sqrt(n)
            f.write(f'{format_vector(v)}\n')
            f.write(f'{format_vector(-v)}\n')
        elif angle == 90:
            # 正交向量
            v1, v2 = generate_orthogonal_vectors(n)
            f.write(f'{format_vector(v1)}\n')
            f.write(f'{format_vector(v2)}\n')
        else:
            # 特定角度
            v1, v2 = generate_vectors_with_angle(n, angle)
            f.write(f'{format_vector(v1)}\n')
            f.write(f'{format_vector(v2)}\n')

    # 生成输出文件
    with open(os.path.join(base_dir, f'{i}.out'), 'w') as f:
        f.write(f'{angle:.2f}\n')