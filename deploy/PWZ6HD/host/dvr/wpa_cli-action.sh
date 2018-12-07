#!/bin/sh

wifi_dhcp=`cfg get network wifi_dhcp`

if [ -z "$wifi_dhcp" ]; then
  wifi_dhcp=0
fi

wifi_ip=`cfg get network wifi_ip`
wifi_mask=`cfg get network wifi_mask`
wifi_broadcast=`cfg get network wifi_broadcast`

if [ -z "$wifi_ip" ] ; then
    wifi_ip=192.168.3.100
    wifi_mask=255.255.255.0
    wifi_broadcast=192.168.3.255
fi

killall udhcpc

case $2 in
CONNECTED)
  case $wifi_dhcp in
    "0")
        ifconfig $1 $wifi_ip netmask $wifi_mask broadcast $wifi_broadcast up
        ;;
    "1")
        if [ -e /var/run/wdhcpc.pid ]; then
            kill `cat /var/run/wdhcpc.pid`
            sleep 1
        fi
        udhcpc -i $1 -p /var/run/wdhcpc.pid -b -q -s /davinci/dvr/udhcpc.script
        ;;
  esac
;;
esac
