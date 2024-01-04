# ION-DVT2-BLE Firmware

## Version 1.0.0 (Release Date: 2024-01-03)

### Instructions
1. Copy below files to HMI:
    - bootloader.bin
    - network_adapter.bin
    - ota_data_initial.bin
    - partition-table.bin
    - your_private_key.bin (e.g. vu.bin, tri.bin)
    - ble_flashing.sh

2. Execute "ble restart loader" on 148 to push esp-ble module to loader mode.
    e.g.:
        Gridania-Telematic > ble restart loader                                
        Booting ESP32-BLE to Bootloader Mode[1,0]...                           
        ---> Done...  

3. On HMI console, execute "chmod +x ble_flashing.sh" to make the script executable
    e.g.:
        root@gridania-evt2-soc:~/ble_fw/ble_self_hosted# chmod +x ble_flashing.sh

4. On HMI console, execute "./ble_flashing.sh your_private_key.bin" to flash these binary files to device
    e.g.:
        root@gridania-evt2-soc:~/ble_fw/ble_self_hosted# ./ble_flashing.sh vu.bin 
        esptool.py v3.3.4-dev
        Serial port /dev/ttyLP1
        Connecting...
        Failed to get PID of a device on /dev/ttyLP1, using standard reset sequence.
        .
        Chip is ESP32-S3 (revision v0.1)
        Features: WiFi, BLE
        Crystal is 40MHz
        MAC: f4:12:fa:53:c2:84
        Uploading stub...
        Running stub...
        Stub running...
        Changing baud rate to 460800
        Changed.
        Configuring flash size...
        Auto-detected Flash size: 4MB
        Flash will be erased from 0x00000000 to 0x00005fff...
        Flash will be erased from 0x00008000 to 0x00008fff...
        Flash will be erased from 0x00009000 to 0x0000bfff...
        Flash will be erased from 0x0000d000 to 0x0000efff...
        Flash will be erased from 0x00010000 to 0x000d6fff...
        Compressed 21072 bytes to 13248...
        Wrote 21072 bytes (13248 compressed) at 0x00000000 in 0.8 seconds (effective 205.9 kbit/s)...
        Hash of data verified.
        Compressed 3072 bytes to 138...
        Wrote 3072 bytes (138 compressed) at 0x00008000 in 0.1 seconds (effective 310.8 kbit/s)...
        Hash of data verified.
        Compressed 12288 bytes to 361...
        Wrote 12288 bytes (361 compressed) at 0x00009000 in 0.2 seconds (effective 414.2 kbit/s)...
        Hash of data verified.
        Compressed 8192 bytes to 31...
        Wrote 8192 bytes (31 compressed) at 0x0000d000 in 0.2 seconds (effective 403.1 kbit/s)...
        Hash of data verified.
        Compressed 812976 bytes to 473972...
        Wrote 812976 bytes (473972 compressed) at 0x00010000 in 13.5 seconds (effective 482.5 kbit/s)...
        Hash of data verified.

        Leaving...
        Hard resetting via RTS pin...
        Flash completed successfully.
        root@gridania-evt2-soc:~/ble_fw/ble_self_hosted# 

5. Execute "ble restart app" on 148 to start the new firmwrae
    e.g.:
        Gridania-Telematic > ble restart app                                   
        Booting ESP32-BLE to Application MODE[1,1]...                          
        ---> Done... 

### Compatibility

- This firmware is developed base on esp32-s3 platform and works on ION DVT2 board only.

### Contributors

- vu.le@ionmobility.com
- tu.nguyen@ionmobility.com

### Resources

- https://monocypher.org
- https://github.com/Ion-Mobility-Systems/ion-mobility-crypto-example

## How to Get It

- N/A

