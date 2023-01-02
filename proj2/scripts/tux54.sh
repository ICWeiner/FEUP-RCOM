#!bin/bash

ifconfig eth0 172.16.50.254/24
ifconfig eth1 172.16.51.253/24
route add default gw 172.16.51.254
route -n
echo 1 > /proc/sys/net/ipv4/ip_forward
echo 0 > /proc/sys/net/ipv4/icmp_echo_ignore_broadcasts
echo "nameserver 172.16.1.1" > /etc/resolv.conf