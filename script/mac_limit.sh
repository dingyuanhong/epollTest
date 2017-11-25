#http://tinylee.info/mac-maxfiles-portrange.html

sudo sysctl -w kern.ipc.somaxconn=1048600
#系统默认的最大连接数限制
sudo sysctl -w kern.maxfiles=1048600
#单个进程默认最大连接数限制
sudo sysctl -w kern.maxfilesperproc=1048600

#shell能打开的最大文件数
ulimit -n 1048600
#动态端口范围
sudo sysctl -w net.inet.ip.portrange.first=32768

#永久
#sudo touch /etc/sysctl.conf
#kern.maxfiles=1048600
#kern.maxfilesperproc=1048600
#net.inet.ip.portrange.first=32768
#net.inet.ip.portrange.last=65535


#~/.bashrc
#ulimit－n 1048576
