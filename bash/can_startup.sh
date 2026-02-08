#!/bin/sh
set -eu

CAN_IF="can0"
BITRATE=500000
TXQLEN=1000

echo "Starting CAN interface: ${CAN_IF}"

if ip link show ${CAN_IF} > /dev/null 2>&1; then
	sudo ip link set ${CAN_IF} down
fi

sudo ip link set ${CAN_IF} type can bitrate ${BITRATE}

sudo ip link set ${CAN_IF} txqueuelen ${TXQLEN}

sudo ip link set ${CAN_IF} up

echo "CAN interface status:"
ip -details link show ${CAN_IF}
