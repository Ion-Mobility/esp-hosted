#!/bin/bash
scp ble_flashing.sh root@192.168.7.1:/home/root/ble_fw/ble_self_hosted/.
scp ble.key root@192.168.7.1:/home/root/ble_fw/ble_self_hosted/.
scp build/bootloader/bootloader.bin root@192.168.7.1:/home/root/ble_fw/ble_self_hosted/.
scp build/partition_table/partition-table.bin  root@192.168.7.1:/home/root/ble_fw/ble_self_hosted/.
scp build/ota_data_initial.bin root@192.168.7.1:/home/root/ble_fw/ble_self_hosted/.
scp build/network_adapter.bin root@192.168.7.1:/home/root/ble_fw/ble_self_hosted/.
