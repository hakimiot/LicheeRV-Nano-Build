#!/bin/sh

# 检查标记，如果已扩容过则直接退出
if [ -f /etc/.rootfs_resized ]; then
    exit 0
fi

# 获取根分区的真实设备节点
ROOT_DEV=$(grep ' / ' /proc/mounts | awk '{print $1}')
# 提取磁盘和分区号，例如 /dev/mmcblk0p2 -> /dev/mmcblk0 和 2
DISK=$(echo $ROOT_DEV | sed 's/p[0-9]*$//')
PART=$(echo $ROOT_DEV | grep -o '[0-9]*$')

if [ -z "$DISK" ] || [ -z "$PART" ]; then
    echo "Failed to detect rootfs device!"
    exit 1
fi

echo "Resizing partition $PART on $DISK to fill disk..."

# 使用 yes 自动回答 'Yes'，忽略占用警告
if ! yes Yes | parted ---pretend-input-tty "$DISK" resizepart "$PART" 100%; then
    echo "parted resizepart failed!"
    exit 1
fi

# 刷新内核分区表
partprobe "$DISK" 2>/dev/null || true

# 扩展文件系统
if ! resize2fs "$ROOT_DEV"; then
    echo "resize2fs failed!"
    exit 1
fi

# 创建标记，防止下次启动重复执行
touch /etc/.rootfs_resized
echo "Rootfs resize complete."
