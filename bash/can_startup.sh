#!/bin/bash

CAN_IF="can0"
BITRATE=500000
TXQLEN=1000

echo "Starting CAN interface: ${CAN_IF}"

# 既に up していたら down
if ip link show ${CAN_IF} > /dev/null 2>&1; then
    sudo ip link set ${CAN_IF} down
fi

# bitrate 設定
sudo ip link set ${CAN_IF} type can bitrate ${BITRATE}

# txqueuelen 設定
sudo ip link set ${CAN_IF} txqueuelen ${TXQLEN}

# up
sudo ip link set ${CAN_IF} up

echo "CAN interface status:"
ip -details link show ${CAN_IF}
