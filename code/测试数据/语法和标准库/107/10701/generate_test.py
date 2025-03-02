import os
import random

def factorial(n):
    result = [1]
    length = 1
    for i in range(2, n + 1):
        carry = 0
        for j in range(length):
            prod = result[j] * i + carry
            result[j] = prod % 10
            carry = prod // 10
        while carry:
            result.append(carry % 10)
            carry //= 10
            length += 1
        length = len(result)
    return ''.join(map(str, result[::-1]))

test_cases = [
    random.randint(1, 20),  # 组 1:1-20
    random.randint(5, 20),
    random.randint(10, 20),
    random.randint(21, 50),  # 组 2:21-100
    random.randint(40, 80),
    random.randint(60, 100),
    random.randint(80, 100),
    random.randint(101, 300),  # 组 3:101-1000
    random.randint(300, 700),
    random.randint(700, 1000)
]

base_dir = os.path.dirname(os.path.abspath(__file__))

for i, n in enumerate(test_cases, 1):
    with open(os.path.join(base_dir, f'{i}.in'), 'w') as f:
        f.write(f'{n}\n')
    with open(os.path.join(base_dir, f'{i}.out'), 'w') as f:
        f.write(f'{factorial(n)}\n')