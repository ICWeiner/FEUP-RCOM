#!bin/bash

ifconfig eth0 172.16.51.1/24
route add -net 172.16.50.1/24 gw 172.16.51.253
route add default gw 172.16.51.254
route -n
echo 0 > /proc/sys/net/ipv4/conf/eth0/accept_redirects
echo 0 > /proc/sys/net/ipv4/conf/all/accept_redirects
echo "nameserver 172.16.1.1" > /etc/resolv.conf