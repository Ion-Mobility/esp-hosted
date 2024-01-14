#!/bin/bash

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
current_directory=$(dirname "$0")

BOOTLOADER="$current_directory/bootloader.bin"
PARTTION="$current_directory/partition-table.bin"
OTA_DATA_INITIAL="$current_directory/ota_data_initial.bin"
NETWORK_ADAPTER="$current_directory/network_adapter.bin"
BOOTLOADER="$current_directory/bootloader.bin"
OPTIONS="--before default_reset --after hard_reset --chip esp32s3 write_flash"

# Search for .key files only in the specified directory (not in subdirectories)
KEY_FILE=$(find "$current_directory" -maxdepth 1 -type f -name "*.key")
# Check if any .key files were found
if [ -n "$KEY_FILE" ]; then
    $ESPTOOL -p $SERIAL_PORT -b $FLASH_BAUD  $OPTIONS --flash_mode $FLASH_MODE --flash_size $FLASH_SIZE --flash_freq $FLASH_FREQ 0x0 $BOOTLOADER 0x8000 $PARTTION 0x9000 $KEY_FILE 0xd000 $OTA_DATA_INITIAL 0x10000 $NETWORK_ADAPTER

    # Check the exit status of the esptool.py command
    if [ $? -eq 0 ]; then
        echo "Flash completed successfully."
    else
        echo "Flash failed."
    fi
else
    echo "No .key files found in the specified directory."
fi