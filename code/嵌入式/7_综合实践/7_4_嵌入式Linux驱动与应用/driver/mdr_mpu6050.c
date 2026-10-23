// SPDX-License-Identifier: GPL-2.0
/*
 * mdr_mpu6050.c —— MPU6050 的字符设备驱动，7.4 的产品形态。
 *
 * 在 5.8 的 sysfs 驱动基础上补齐三件事：
 *   - 把 INT 引脚接上，用线程化中断把样本推进 kfifo；
 *   - 提供 /dev/mdr_imu，read 一次拿到一个带时间戳的定长样本；
 *   - 把采样率与量程做成可读写的 sysfs 属性。
 *
 * 没有中断时（例如 QEMU 上的 i2c-stub）退化为按需读取：read 每次发起
 * 一次 I²C 块传输，把当前值当作一个样本交出去。
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/ktime.h>
#include <linux/miscdevice.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include "mdr_imu.h"

#define MDR_MPU6050_REG_SMPLRT_DIV	0x19
#define MDR_MPU6050_REG_CONFIG		0x1A
#define MDR_MPU6050_REG_GYRO_CONFIG	0x1B
#define MDR_MPU6050_REG_ACCEL_CONFIG	0x1C
#define MDR_MPU6050_REG_ACCEL_XOUT_H	0x3B
#define MDR_MPU6050_REG_PWR_MGMT_1	0x6B
#define MDR_MPU6050_REG_WHO_AM_I	0x75

#define MDR_MPU6050_WHO_AM_I_VAL	0x68
#define MDR_MPU6050_FIFO_SAMPLES	64

/* 下标就是 ACCEL_CONFIG 的位 4:3，即量程档位 */
static const u16 mdr_accel_lsb_per_g[] = { 16384, 8192, 4096, 2048 };

/* 陀螺仪固定 ±250 °/s，131 LSB/(°/s) */
#define MDR_MPU6050_GYRO_LSB_PER_DPS	131

struct mdr_imu {
	struct i2c_client *client;
	struct mutex lock;	/* 保护 kfifo、accel_range 与硬件配置 */
	struct kfifo fifo;
	wait_queue_head_t readq;
	u8 accel_range;
	bool has_irq;
	struct miscdevice misc;
};

static int mdr_imu_read_raw(struct mdr_imu *priv, s16 accel[3], s16 gyro[3])
{
	u8 raw[14];
	int ret;

	ret = i2c_smbus_read_i2c_block_data(priv->client,
					    MDR_MPU6050_REG_ACCEL_XOUT_H,
					    sizeof raw, raw);
	if (ret < 0)
		return ret;
	if (ret != (int)sizeof raw)
		return -EIO;

	/* 0x3B 起依次是 accel(6)、temp(2)、gyro(6)，高字节在前 */
	for (int i = 0; i < 3; i++) {
		accel[i] = (s16)((raw[2 * i] << 8) | raw[2 * i + 1]);
		gyro[i] = (s16)((raw[8 + 2 * i] << 8) | raw[9 + 2 * i]);
	}
	return 0;
}

static int mdr_imu_fill(struct mdr_imu *priv, struct mdr_imu_sample *s)
{
	s16 accel[3];
	s16 gyro[3];
	u16 accel_lsb = mdr_accel_lsb_per_g[priv->accel_range];
	int ret;

	ret = mdr_imu_read_raw(priv, accel, gyro);
	if (ret)
		return ret;
	s->timestamp_ns = ktime_get_boottime_ns();
	for (int i = 0; i < 3; i++) {
		/* 量程越大 LSB/g 越小；乘 1000 之后仍在 32 位整数范围内 */
		s->accel[i] = (s16)((int)accel[i] * 1000 / accel_lsb);
		s->gyro[i] = (s16)((int)gyro[i] * 100 /
				   MDR_MPU6050_GYRO_LSB_PER_DPS);
	}
	s->accel_range = priv->accel_range;
	s->reserved = 0;
	return 0;
}

static irqreturn_t mdr_imu_irq_top(int irq, void *dev_id)
{
	return IRQ_WAKE_THREAD;
}

static irqreturn_t mdr_imu_irq_thread(int irq, void *dev_id)
{
	struct mdr_imu *priv = dev_id;
	struct mdr_imu_sample s;

	/* 线程上下文里可以睡眠，i2c_smbus_* 才允许调用 */
	mutex_lock(&priv->lock);
	if (mdr_imu_fill(priv, &s) == 0)
		kfifo_in(&priv->fifo, &s, 1);
	mutex_unlock(&priv->lock);
	wake_up_interruptible(&priv->readq);
	return IRQ_HANDLED;
}

static ssize_t mdr_imu_read(struct file *file, char __user *ubuf,
			    size_t count, loff_t *ppos)
{
	/*
	 * misc 框架在 .open 之后把 file->private_data 换成了 miscdevice
	 * 指针，所以要靠 container_of 找回自己的对象（5.7 讲过这一点）。
	 */
	struct mdr_imu *priv = container_of(file->private_data,
					    struct mdr_imu, misc);
	struct mdr_imu_sample s;
	int ret;

	if (count < sizeof s)
		return -EINVAL;

	for (;;) {
		mutex_lock(&priv->lock);
		ret = kfifo_out(&priv->fifo, &s, 1);
		mutex_unlock(&priv->lock);
		if (ret == 1)
			break;
		if (!priv->has_irq) {
			/* 没有中断：主动读一次寄存器，当作一个样本 */
			mutex_lock(&priv->lock);
			ret = mdr_imu_fill(priv, &s);
			mutex_unlock(&priv->lock);
			if (ret)
				return ret;
			break;
		}
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		ret = wait_event_interruptible(priv->readq,
					       !kfifo_is_empty(&priv->fifo));
		if (ret)
			return ret;
	}

	if (copy_to_user(ubuf, &s, sizeof s))
		return -EFAULT;
	return sizeof s;
}

static __poll_t mdr_imu_poll(struct file *file, poll_table *wait)
{
	struct mdr_imu *priv = container_of(file->private_data,
					    struct mdr_imu, misc);
	__poll_t mask = 0;

	poll_wait(file, &priv->readq, wait);
	if (!priv->has_irq || !kfifo_is_empty(&priv->fifo))
		mask |= EPOLLIN | EPOLLRDNORM;
	return mask;
}

static const struct file_operations mdr_imu_fops = {
	.owner = THIS_MODULE,
	.read = mdr_imu_read,
	.poll = mdr_imu_poll,
	.llseek = noop_llseek,
};

static ssize_t sample_rate_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mdr_imu *priv = dev_get_drvdata(dev);
	u8 div;
	int ret;

	mutex_lock(&priv->lock);
	ret = i2c_smbus_read_byte_data(priv->client,
				       MDR_MPU6050_REG_SMPLRT_DIV);
	mutex_unlock(&priv->lock);
	if (ret < 0)
		return ret;
	div = (u8)ret;

	/* DLPF 开启时内部基准是 1 kHz，采样率 = 1000 / (1 + div) */
	return sysfs_emit(buf, "%u\n", 1000 / (1 + div));
}

static ssize_t sample_rate_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t len)
{
	struct mdr_imu *priv = dev_get_drvdata(dev);
	unsigned int rate;
	u8 div;
	int ret;

	ret = kstrtouint(buf, 10, &rate);
	if (ret)
		return ret;
	/* 4～1000 Hz 才能用整数分频表示 */
	if (rate < 4 || rate > 1000)
		return -EINVAL;
	div = (u8)(1000 / rate - 1);

	mutex_lock(&priv->lock);
	ret = i2c_smbus_write_byte_data(priv->client,
					MDR_MPU6050_REG_SMPLRT_DIV, div);
	mutex_unlock(&priv->lock);
	if (ret < 0)
		return ret;
	return len;
}
static DEVICE_ATTR_RW(sample_rate);

static ssize_t accel_range_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mdr_imu *priv = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%u\n", priv->accel_range);
}

static ssize_t accel_range_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t len)
{
	struct mdr_imu *priv = dev_get_drvdata(dev);
	unsigned int range;
	int ret;

	ret = kstrtouint(buf, 10, &range);
	if (ret)
		return ret;
	if (range > 3)
		return -EINVAL;

	mutex_lock(&priv->lock);
	ret = i2c_smbus_write_byte_data(priv->client,
					MDR_MPU6050_REG_ACCEL_CONFIG,
					(u8)(range << 3));
	if (ret == 0)
		priv->accel_range = (u8)range;
	mutex_unlock(&priv->lock);
	return ret < 0 ? ret : (ssize_t)len;
}
static DEVICE_ATTR_RW(accel_range);

static int mdr_imu_hw_init(struct mdr_imu *priv)
{
	struct i2c_client *client = priv->client;
	int ret;

	ret = i2c_smbus_read_byte_data(client, MDR_MPU6050_REG_WHO_AM_I);
	if (ret < 0)
		return ret;
	if (ret != MDR_MPU6050_WHO_AM_I_VAL) {
		dev_err(&client->dev, "WHO_AM_I = 0x%02x，不是 MPU6050\n", ret);
		return -ENODEV;
	}

	/* 清 SLEEP 位，时钟源用内部 8 MHz 振荡器 */
	ret = i2c_smbus_write_byte_data(client, MDR_MPU6050_REG_PWR_MGMT_1, 0x00);
	if (ret < 0)
		return ret;
	msleep(100);

	/* DLPF 档位 3：加速度 44 Hz、陀螺仪 42 Hz，内部基准 1 kHz */
	ret = i2c_smbus_write_byte_data(client, MDR_MPU6050_REG_CONFIG, 0x03);
	if (ret < 0)
		return ret;

	/* 默认 ±2 g、±250 °/s、100 Hz */
	ret = i2c_smbus_write_byte_data(client, MDR_MPU6050_REG_ACCEL_CONFIG,
					0x00);
	if (ret < 0)
		return ret;
	ret = i2c_smbus_write_byte_data(client, MDR_MPU6050_REG_GYRO_CONFIG,
					0x00);
	if (ret < 0)
		return ret;
	return i2c_smbus_write_byte_data(client, MDR_MPU6050_REG_SMPLRT_DIV, 9);
}

static int mdr_imu_probe(struct i2c_client *client)
{
	struct mdr_imu *priv;
	int ret;

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	priv->client = client;
	priv->accel_range = 0;
	mutex_init(&priv->lock);
	init_waitqueue_head(&priv->readq);
	i2c_set_clientdata(client, priv);

	ret = kfifo_alloc(&priv->fifo,
			  MDR_MPU6050_FIFO_SAMPLES *
				  sizeof(struct mdr_imu_sample),
			  GFP_KERNEL);
	if (ret)
		return ret;

	ret = mdr_imu_hw_init(priv);
	if (ret)
		goto err_fifo;

	if (client->irq > 0) {
		/*
		 * 上半部只唤醒线程，真正的 I²C 读取在线程里做。
		 * 触发方式由设备树的 interrupts 属性给出，驱动不再传
		 * IRQF_TRIGGER_*，否则两处不一致时行为取决于谁赢。
		 * IRQF_ONESHOT：线程处理完之前不重新打开这条中断线。
		 */
		ret = devm_request_threaded_irq(&client->dev, client->irq,
						mdr_imu_irq_top,
						mdr_imu_irq_thread,
						IRQF_ONESHOT,
						"mdr_mpu6050", priv);
		if (ret) {
			dev_err(&client->dev, "申请中断 %d 失败: %d\n",
				client->irq, ret);
			goto err_fifo;
		}
		priv->has_irq = true;
	}

	priv->misc.minor = MISC_DYNAMIC_MINOR;
	priv->misc.name = "mdr_imu";
	priv->misc.fops = &mdr_imu_fops;
	priv->misc.parent = &client->dev;
	priv->misc.mode = 0660;
	ret = misc_register(&priv->misc);
	if (ret)
		goto err_fifo;

	/*
	 * misc 设备没有 dev_groups，属性在注册之后逐个挂到 this_device 上，
	 * 出现在 /sys/class/misc/mdr_imu/ 下。
	 */
	ret = device_create_file(priv->misc.this_device,
				 &dev_attr_sample_rate);
	if (ret)
		goto err_misc;
	ret = device_create_file(priv->misc.this_device,
				 &dev_attr_accel_range);
	if (ret)
		goto err_rate;

	dev_info(&client->dev, "MPU6050 就绪，地址 0x%02x，%s\n",
		 client->addr, priv->has_irq ? "中断模式" : "轮询模式");
	return 0;

err_rate:
	device_remove_file(priv->misc.this_device, &dev_attr_sample_rate);
err_misc:
	misc_deregister(&priv->misc);
err_fifo:
	kfifo_free(&priv->fifo);
	return ret;
}

static void mdr_imu_remove(struct i2c_client *client)
{
	struct mdr_imu *priv = i2c_get_clientdata(client);

	device_remove_file(priv->misc.this_device, &dev_attr_accel_range);
	device_remove_file(priv->misc.this_device, &dev_attr_sample_rate);
	misc_deregister(&priv->misc);
	kfifo_free(&priv->fifo);

	/* 回到休眠，省掉几毫安的静态电流 */
	i2c_smbus_write_byte_data(client, MDR_MPU6050_REG_PWR_MGMT_1, 0x40);
}

static const struct of_device_id mdr_mpu6050_of_match[] = {
	{ .compatible = "mdr,mpu6050" },
	{ }
};
MODULE_DEVICE_TABLE(of, mdr_mpu6050_of_match);

static const struct i2c_device_id mdr_mpu6050_id[] = {
	{ "mdr_mpu6050", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mdr_mpu6050_id);

static struct i2c_driver mdr_mpu6050_driver = {
	.probe = mdr_imu_probe,
	.remove = mdr_imu_remove,
	.id_table = mdr_mpu6050_id,
	.driver = {
		.name = "mdr_mpu6050",
		.of_match_table = mdr_mpu6050_of_match,
	},
};
module_i2c_driver(mdr_mpu6050_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MDR 的 MPU6050 字符设备驱动（7.4）");