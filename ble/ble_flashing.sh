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
KEY_FILE="$current_directory/ble.key"
OPTIONS="--before default_reset --after hard_reset --chip esp32s3 write_flash"


# BLE restart loader
ble_restart_loader() {
    ble_restart_loader="00000148#FD00"
    cansend can0 $ble_restart_loader
    result=$?
    # Check the result
    if [ $result -eq 0 ]; then
        echo "ble restart loader successfully."
        return 0
    else
        echo "Error: ble restart loader failed with exit code $result."
        return -1
    fi
}

# BLE restart app
ble_restart_app() {
    # BLE restart app
    ble_restart_app="00000148#FD01"
    cansend can0 $ble_restart_app
    result=$?
    # Check the result
    if [ $result -eq 0 ]; then
        echo "ble restart app successfully."
        return 0
    else
        echo "Error: ble restart app failed with exit code $result."
        return -1
    fi
}

ble_flashing() {
    $ESPTOOL -p $SERIAL_PORT -b $FLASH_BAUD  $OPTIONS --flash_mode $FLASH_MODE --flash_size $FLASH_SIZE --flash_freq $FLASH_FREQ 0x0 $BOOTLOADER 0x8000 $PARTTION 0x9000 $KEY_FILE 0xd000 $OTA_DATA_INITIAL 0x10000 $NETWORK_ADAPTER
    # Check the exit status of the esptool.py command
    if [ $? -eq 0 ]; then
        echo "Flash completed successfully."
        return 0
    else
        echo "Flash failed."
        return -1
    fi
}

__main__() {
    ble_restart_loader
    if [ $? -ne 0 ]; then
        return -1
    fi
    sleep 1

    ble_flashing
    if [ $? -ne 0 ]; then
        return -1
    fi

    sleep 1
    ble_restart_app
    if [ $? -ne 0 ]; then
        return -1
    fi
}

__main__