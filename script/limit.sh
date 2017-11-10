#!/bin/sh

#设置文件
echo "1048576" > /proc/sys/fs/file-max
#epoll最大监听数
echo "1048576" > /proc/sys/fs/epoll/max_user_watches
#本地端口区段
echo "1024 65535"> /proc/sys/net/ipv4/ip_local_port_range
#listen 并发数
echo "1024" > /proc/sys/net/core/somaxconn
echo "1024" > /proc/sys/net/ipv4/tcp_max_syn_backlog
#检查活动链接
#ss -lt
#检查arp
#ip neigh

#tcp页
# /proc/sys/net/ipv4/tcp_mem
#tcp写缓冲区设置
#/proc/sys/net/ipv4/tcp_rmem
#tcp读缓冲区设置
#/proc/sys/net/ipv4/tcp_wmem
#syn风暴监视
#echo "1" > /proc/sys/net/ipv4/tcp_syncookies

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

#echo "net.ipv4.tcp_timestamps=0" >> /etc/sysctl.conf
#echo "net.core.somaxconn = 8192" >> /etc/sysctl.conf
#echo "fs.file-max=1048576" >> /etc/sysctl.conf
#echo "net.ipv4.ip_local_port_range = 1024 65535" >> /etc/sysctl.conf
#echo "net.ipv4.tcp_mem = 786432 2097152 3145728">> /etc/sysctl.conf
#echo "net.ipv4.tcp_rmem = 4096 4096 16777216">> /etc/sysctl.conf
#echo "net.ipv4.tcp_wmem = 4096 4096 16777216">> /etc/sysctl.conf
#开启重用
#echo "net.ipv4.tcp_tw_reuse = 1">> /etc/sysctl.conf
#tcp快速回收
#echo "net.ipv4.tcp_tw_recycle = 1">> /etc/sysctl.conf

#sysctl -p
