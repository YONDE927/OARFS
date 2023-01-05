#!/bin/sh

if [ $# -lt 1 ]; then
    echo "./net-switch.sh [on/off]"
    exit 0
fi

if [ $1 = "off" ]; then
    sudo iptables -A INPUT -p tcp --dport 8080 -j DROP
    sudo iptables -L INPUT --line-numbers
elif [ $1 = "on" ]; then
    sudo iptables -D INPUT -p tcp --dport 8080 -j DROP
    sudo iptables -L INPUT --line-numbers
fi

