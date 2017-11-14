#!/bin/sh

#日志
#dmesg
#cat /var/log/messages
#检查活动链接
#ss -lt
#检查arp
#ip neigh

#设置文件
echo "1048576" > /proc/sys/fs/file-max
#epoll最大监听数
echo "1048576" > /proc/sys/fs/epoll/max_user_watches
#本地端口区段
echo "1024 65535"> /proc/sys/net/ipv4/ip_local_port_range
#listen 并发数
echo "8192" > /proc/sys/net/core/somaxconn
echo "1024" > /proc/sys/net/ipv4/tcp_max_syn_backlog

#tcp页
# /proc/sys/net/ipv4/tcp_mem
#tcp写缓冲区设置
#/proc/sys/net/ipv4/tcp_rmem
#tcp读缓冲区设置
#/proc/sys/net/ipv4/tcp_wmem
#syn风暴监视
echo "1" > /proc/sys/net/ipv4/tcp_syncookies

#临时设置
sysctl -w fs.file-max=1048576
sysctl -w fs.epoll.max_user_watches=1048576
ulimit -SHn 1048576
ulimit -Hn 1048576
ulimit -n 1048576

#echo "ulimit -Hn 1048576" >> /etc/profile
#echo "ulimit -n 1048576" >> /etc/profile

#永久设置
#/etc/security/limits.conf
#* hard nofile 1048576
#* soft nofile 1048576
#echo "* hard nofile 1048576" >> /etc/security/limits.conf
#echo "* soft nofile 1048576" >> /etc/security/limits.conf

#arp响应
echo "1" > /proc/sys/net/ipv4/conf/all/arp_ignore
echo "1" > /proc/sys/net/ipv4/conf/lo/arp_ignore
echo "2" > /proc/sys/net/ipv4/conf/lo/arp_announce
echo "2" > /proc/sys/net/ipv4/conf/all/arp_announce

echo "net.ipv4.conf.all.arp_ignore = 1" >> /etc/sysctl.conf
echo "net.ipv4.conf.all.arp_announce = 2" >> /etc/sysctl.conf

#listen 并发数
echo "net.core.somaxconn = 8192" >> /etc/sysctl.conf
#连接数
echo "fs.file-max=1048576" >> /etc/sysctl.conf
#等待连接的网络连接数,尚未收到客户端确认信息的连接请求的最大值
echo "net.ipv4.tcp_max_syn_backlog = 65536" >> /etc/sysctl.conf
#同时保持TIME_WAIT的最大数量
echo "net.ipv4.tcp_max_tw_buckets = 5000" >> /etc/sysctl.conf
#keepalive消息的频度,20分钟
echo "net.ipv4.tcp_keepalive_time = 1200" >> /etc/sysctl.conf
#tcp syn重试次数
echo "net.ipv4.tcp_syn_retries = 2" >> /etc/sysctl.conf

#时间戳可以避免序列号的卷绕
echo "net.ipv4.tcp_timestamps=0" >> /etc/sysctl.conf
#向外连接的端口范围
echo "net.ipv4.ip_local_port_range = 1024 65535" >> /etc/sysctl.conf
echo "net.ipv4.tcp_mem = 786432 2097152 3145728">> /etc/sysctl.conf
echo "net.ipv4.tcp_rmem = 4096 4096 16777216">> /etc/sysctl.conf
echo "net.ipv4.tcp_wmem = 4096 4096 16777216">> /etc/sysctl.conf
#开启重用
echo "net.ipv4.tcp_tw_reuse = 1">> /etc/sysctl.conf
#tcp快速回收
echo "net.ipv4.tcp_tw_recycle = 1">> /etc/sysctl.conf
#syn风暴监视
echo "net.ipv4.tcp_syncookies = 1">> /etc/sysctl.conf
#fin快速关闭
echo "net.ipv4.tcp_fin_timeout = 30">> /etc/sysctl.conf

sysctl -p
