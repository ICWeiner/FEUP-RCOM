#!bin/bash

ifconfig eth0 172.16.50.1/24
route add -net 172.16.51.1/24 gw 172.16.50.254
route add default gw 172.16.50.254
route -n
echo "nameserver 172.16.1.1" > /etc/resolv.conf