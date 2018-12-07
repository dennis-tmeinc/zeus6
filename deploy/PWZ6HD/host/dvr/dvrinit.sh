#!/bin/sh

export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/mnt/nand:/mnt/nand/dvr
export LD_LIBRARY_PATH=/mnt/nand/dvr/lib

ifconfig lo 127.0.0.1
ifconfig eth0 192.168.1.100

# load wifi drivers (station mode)
/mnt/nand/dvr/drivers/wifi.sh

mkdir -p /var/dvr/disks

# setup DVR configure file
mkdir /etc/dvr
ln -sf /mnt/nand/dvr/dvrsvr.conf /etc/dvr/dvr.conf

hostname `cfg get system hostname`
if [ ! -f /etc/TZ ] ; then
    timezone=`cfg get system timezone`
    tzx=`cfg get timezones $timezone`
    echo ${tzx%% *} > /etc/TZ
fi

echo "start hotplug"
# start hotplug deamond
zdaemon tdevd /mnt/nand/dvr/tdevhotplug
# mount disk already found
zdaemon tdevmount /mnt/nand/dvr/tdevhotplug

sleep 2

# start io module
echo "Start IO PROCESS"
zdaemon ioprocess

sleep 2

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
setnetwork

#setup web server
mkdir /var/www
ln -sf /davinci/dvr/www/cgi /var/www/cgi
inetd /davinci/dvr/inetd.conf

