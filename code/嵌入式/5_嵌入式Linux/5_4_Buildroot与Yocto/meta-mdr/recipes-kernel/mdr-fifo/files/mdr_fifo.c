// SPDX-License-Identifier: GPL-2.0
/*
 * mdr_fifo：一个纯软件的先进先出缓冲区，5.7 的示例设备。
 *
 * 数据只存在内核里，不碰任何硬件。QEMU 与开发板上都能加载。
 */

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/kfifo.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#define MDR_FIFO_CAPACITY 256

static dev_t mdr_fifo_devno;
static struct cdev mdr_fifo_cdev;
static struct class *mdr_fifo_class;

static DEFINE_KFIFO(mdr_fifo_buf, char, MDR_FIFO_CAPACITY);
static DEFINE_MUTEX(mdr_fifo_lock);
static DECLARE_WAIT_QUEUE_HEAD(mdr_fifo_readq);
static DECLARE_WAIT_QUEUE_HEAD(mdr_fifo_writeq);

static int mdr_fifo_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int mdr_fifo_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t mdr_fifo_read(struct file *file, char __user *ubuf,
			     size_t count, loff_t *ppos)
{
	char tmp[MDR_FIFO_CAPACITY];
	unsigned int n;
	int ret;

	if (mutex_lock_interruptible(&mdr_fifo_lock))
		return -ERESTARTSYS;

	while (kfifo_is_empty(&mdr_fifo_buf)) {
		mutex_unlock(&mdr_fifo_lock);
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible(mdr_fifo_readq,
					     !kfifo_is_empty(&mdr_fifo_buf)))
			return -ERESTARTSYS;
		if (mutex_lock_interruptible(&mdr_fifo_lock))
			return -ERESTARTSYS;
	}

	n = kfifo_out(&mdr_fifo_buf, tmp, min(count, sizeof tmp));
	mutex_unlock(&mdr_fifo_lock);

	ret = copy_to_user(ubuf, tmp, n);
	if (ret)
		return -EFAULT;
	return n;
}

static ssize_t mdr_fifo_write(struct file *file, const char __user *ubuf,
			      size_t count, loff_t *ppos)
{
	char tmp[MDR_FIFO_CAPACITY];
	unsigned int n;
	int ret;

	n = min(count, sizeof tmp);
	ret = copy_from_user(tmp, ubuf, n);
	if (ret)
		return -EFAULT;

	if (mutex_lock_interruptible(&mdr_fifo_lock))
		return -ERESTARTSYS;
	n = kfifo_in(&mdr_fifo_buf, tmp, n);
	mutex_unlock(&mdr_fifo_lock);

	wake_up_interruptible(&mdr_fifo_readq);
	return n;
}

static __poll_t mdr_fifo_poll(struct file *file, poll_table *wait)
{
	__poll_t mask = 0;

	poll_wait(file, &mdr_fifo_readq, wait);
	poll_wait(file, &mdr_fifo_writeq, wait);

	if (!kfifo_is_empty(&mdr_fifo_buf))
		mask |= EPOLLIN | EPOLLRDNORM;
	if (!kfifo_is_full(&mdr_fifo_buf))
		mask |= EPOLLOUT | EPOLLWRNORM;
	return mask;
}

static const struct file_operations mdr_fifo_fops = {
	.owner = THIS_MODULE,
	.open = mdr_fifo_open,
	.release = mdr_fifo_release,
	.read = mdr_fifo_read,
	.write = mdr_fifo_write,
	.poll = mdr_fifo_poll,
};

static int __init mdr_fifo_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&mdr_fifo_devno, 0, 1, "mdr_fifo");
	if (ret)
		return ret;

	cdev_init(&mdr_fifo_cdev, &mdr_fifo_fops);
	mdr_fifo_cdev.owner = THIS_MODULE;
	ret = cdev_add(&mdr_fifo_cdev, mdr_fifo_devno, 1);
	if (ret)
		goto err_region;

	mdr_fifo_class = class_create("mdr_fifo");
	if (IS_ERR(mdr_fifo_class)) {
		ret = PTR_ERR(mdr_fifo_class);
		goto err_cdev;
	}
	if (IS_ERR(device_create(mdr_fifo_class, NULL, mdr_fifo_devno, NULL,
				 "mdr_fifo"))) {
		ret = -ENODEV;
		goto err_class;
	}

	pr_info("mdr_fifo: ready, capacity %u\n", MDR_FIFO_CAPACITY);
	return 0;

err_class:
	class_destroy(mdr_fifo_class);
err_cdev:
	cdev_del(&mdr_fifo_cdev);
err_region:
	unregister_chrdev_region(mdr_fifo_devno, 1);
	return ret;
}

static void __exit mdr_fifo_exit(void)
{
	device_destroy(mdr_fifo_class, mdr_fifo_devno);
	class_destroy(mdr_fifo_class);
	cdev_del(&mdr_fifo_cdev);
	unregister_chrdev_region(mdr_fifo_devno, 1);
	pr_info("mdr_fifo: unloaded\n");
}

module_init(mdr_fifo_init);
module_exit(mdr_fifo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mdr");
MODULE_DESCRIPTION("纯软件 FIFO 字符设备");
