#!/bin/bash

# Check if a .bin file is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <path/to/file.bin>"
    exit 1
fi

# Check if the provided file has a .bin extension
if [[ "$1" != *.bin ]]; then
    echo "Error: Please provide a .bin file."
    exit 1
fi

# Set the path to esptool.py
ESPTOOL="esptool.py"

# Set the serial port
SERIAL_PORT="/dev/ttyLP1"

# Set other flash parameters
FLASH_BAUD=460800
FLASH_MODE="dio"
FLASH_SIZE="detect"
FLASH_FREQ="80m"

# Run esptool.py command
$ESPTOOL -p $SERIAL_PORT -b $FLASH_BAUD --before default_reset --after hard_reset --chip esp32s3 write_flash --flash_mode $FLASH_MODE --flash_size $FLASH_SIZE --flash_freq $FLASH_FREQ 0x0 bootloader.bin 0x8000 partition-table.bin 0x9000 "$1" 0xd000 ota_data_initial.bin 0x10000 network_adapter.bin

# Check the exit status of the esptool.py command
if [ $? -eq 0 ]; then
    echo "Flash completed successfully."
else
    echo "Flash failed."
fi
