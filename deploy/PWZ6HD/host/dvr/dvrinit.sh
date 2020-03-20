#!/bin/sh

export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/mnt/nand:/mnt/nand/dvr
export LD_LIBRARY_PATH=/mnt/nand/dvr/lib

#set system clock from rtc
hwclock -s -u

ifconfig lo 127.0.0.1
ifconfig eth0 192.168.1.100

# setup DVR configure file
mkdir /etc/dvr
ln -sf /mnt/nand/dvr/dvrsvr.conf /etc/dvr/dvr.conf

hostname `cfg get system hostname`
if [ ! -f /etc/TZ ] ; then
    timezone=`cfg get system timezone`
    tzx=`cfg get timezones $timezone`
    TZ=${tzx%% *}
    echo $TZ > /etc/TZ
    export TZ
fi
dvrtime rtctoutc

echo "start hotplug"
mkdir -p /var/dvr/disks
# start hotplug deamond
zdaemon tdevd /mnt/nand/dvr/tdevhotplug
# mount disk already found
zdaemon tdevmount /mnt/nand/dvr/tdevhotplug

sleep 1

# load wifi drivers (station mode)
/mnt/nand/dvr/drivers/wifi.sh

# start io module
echo "Start IO PROCESS"
zdaemon ioprocess

# give some time for ioprocess to fully startup
sleep 2

# gps loggin
zdaemon glog
# tab102
zdaemon tab102

echo "start dvr"
# start dvr server
zdaemon dvrsvr

adb start-server

# start remote access
echo "start rconn"
zdaemon rconn

# start body camera daemon
echo "start body camera daemon"
zdaemon bodycamd

sleep 2
# enable ip route ^^
echo 1 > /proc/sys/net/ipv4/ip_forward
setnetwork

#setup web server
mkdir /var/www
ln -sf /davinci/dvr/www/cgi /var/www/cgi
inetd /davinci/dvr/inetd.conf

