# typedef - 给类型起别名

## 什么是 typedef？

typedef 就像给类型取一个"外号"，让复杂的类型名变得简单好记。

## 为什么需要 typedef？

1. **简化复杂类型**：

   ```c
   // 未使用 typedef
   struct Point { int x; int y; };
   struct Point p1;

   // 使用 typedef
   typedef struct { int x; int y; } Point;
   Point p2; // 更简洁
   ```

2. 提高可读性：

   ```c
   typedef int Age;       // 年龄
   typedef float Weight;  // 体重

   Age studentAge = 18;
   Weight personWeight = 65.5;
   ```

3. 方便修改类型：

   ```c
   typedef int ID_Type; // 需要修改时只需改这一处

   ID_Type userID_01 = 1001;
   ID_Type userID_02 = 1002;
   ```

## 示例

### 1. 简化结构体

```c
typedef struct {
    char name[50];
    int age;
} Person;

Person p = {"张三", 20}; // 直接使用 Person
```

### 2. 简化指针类型

```c
typedef char* String;

String s = "Hello"; // 等同于 char* s
```

### 3. 简化函数指针

```c
typedef int (*MathFunc)(int, int);

int add(int a, int b) { return a + b; }

MathFunc operation = add; // 函数指针更清晰
int result = operation(3, 5);
```

### 4. 简化数组类型

```c
typedef int Vector3[3];

Vector3 position = {10, 20, 30}; // 等同于 int position[3]
```
