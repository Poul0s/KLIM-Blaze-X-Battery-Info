# klimbx-battery

A small C project to retrieve the battery level and charging status of a **KLIM Blaze X** wireless mouse over USB, using `libusb-1.0`.

## How it works

The library communicates directly with the mouse via USB HID control and interrupt transfers, without relying on any kernel HID driver. It sends a `SET_REPORT` request to trigger a battery status report, then reads the response from the interrupt IN endpoint.

Tested on Linux with the following product IDs:
- `0x260d:0x1113` (The mouse)
- `0x260d:0x1074` (The dongle)

## Requirements

- `libusb-1.0` (development headers + shared library)

On Arch:
```bash
sudo pacman -S libusb
```

On Debian/Ubuntu:
```bash
sudo apt install libusb-1.0-0-dev
```

## USB permissions (required)

By default, accessing USB devices requires root. To allow your user to access the KLIM Blaze X without `sudo`, copy the udev rule:

```bash
sudo cp ./99-klimblazex.rules /etc/udev/rules.d/99-klimblazex.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## Usage in your project

Copy `battery.h` and `battery.c` into your project, then:

```c
#include "battery.h"
#include <stdio.h>

int main(void) {
    kbx_data data;

    if (kbx_init(&data))
        return 1;

    kbx_refresh(&data);
    printf("Battery: %d%% - %s\n", data.battery_level,
           data.charging_status ? "Charging" : "Not charging");

    kbx_release(&data);
    return 0;
}
```

Compile with:
```bash
gcc your_program.c battery.c -lusb-1.0 -o your_program
```

### API

```c
// Initialize the library and open the USB device
// Returns 0 on success, non-zero on error
int kbx_init(kbx_data *data);

// Poll the mouse for battery level and charging status
// Updates data->battery_level (0-100) and data->charging_status (0 or 1)
// Returns 0 on success, non-zero on error
int kbx_refresh(kbx_data *data);

// Release the USB device and free resources
int kbx_release(kbx_data *data);
```

### The `kbx_data` struct

```c
typedef struct s_kbx_data {
    libusb_context                  *ctx;
    struct libusb_device_descriptor  dev_desc;
    libusb_device_handle            *dev_handle;
    int                              battery_level;    // 0-100, -1 if unknown
    int                              charging_status;  // 1 = charging, 0 = not, -1 if unknown
} kbx_data;
```

## Debug mode

Set the environment variable `KBX_DEBUG=1` to enable verbose logging:
```bash
KBX_DEBUG=1 ./your_program
```

Or force debug at compile time:
```bash
gcc your_program.c battery.c -lusb-1.0 -DKBX_FORCE_DEBUG=1 -o your_program
```

## Building and running the example

```bash
gcc example.c battery.c -lusb-1.0 -o example
./example
```

Expected output:
```
Battery level: 85%; Charging status: Not charging
Battery level: 85%; Charging status: Not charging
Battery level: 85%; Charging status: Not charging
```