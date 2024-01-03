# ION-DVT2-BLE Firmware Release Notes

## Version 1.0.0 (Release Date: 2024-01-03)

### New Features

- Feature 1: Auto pair with authorized phone.
- Feature 2: Sync up bike's data with phone.
- Feature 3: Provide bike's setting.
- Feature 4: Power on, open seat, ping Bike...

### Enhancements

- Enhancement 1: Create secure session after successfuly paired.
- Enhancement 2: Use message code to communicate with 148 instead of human readable at command text.

### Bug Fixes

- Fix 1: Remove ble-selfhosted code to avoid the potencial issue that it boots to wrong firmware.

### Changes

- Change 1: N/A.
- Change 2: N/A.

### Deprecated Features

- Deprecated Feature 1: Remve steering unlock.
- Deprecated Feature 2: Remove last trip info.

### Known Issues

- Known Issue 1: N/A.
- Known Issue 2: N/A

### Miscellaneous

- N/A

### Upgrading Instructions

- Execute ./cp_to_imx.sh to copy firmware to hmi
- Execute "ble restart loader" on 148 to push esp-ble module to loader mode
- Execute ./ble_flashing.sh to flash these binary files to device
- Execute "ble restart app" on 148 to start the new firmwrae

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

