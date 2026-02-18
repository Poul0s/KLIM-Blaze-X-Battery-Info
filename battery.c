#include "battery.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

static char is_debug() {
	# if defined KBX_FORCE_DEBUG && KBX_FORCE_DEBUG == 1
	return 1;
	# else
	const char * env_val = getenv("KBX_DEBUG");
	const char debug = env_val != NULL && env_val[0] == '1' && env_val[1] == 0;
	return debug;
	# endif
}

static void log_debug(const char *str, ...) {
	static char debug = -1;
	if (debug == -1) {
		debug = is_debug();
	}

	if (debug) {
		va_list args;
		va_start(args, str);
		vprintf(str, args);
		va_end(args);
	}
}

static void log_error(const char *str, int libusb_error_val) {
	static char debug = -1;
	if (debug == -1) {
		debug = is_debug();
	}

	if (debug) {
		if (libusb_error_val) {
			fprintf(stderr, "%s: (%s). %s\n", str, libusb_error_name(libusb_error_val), strerror(errno));
		} else {
			write(2, str, strlen(str));
			write(2, "\n", 1);
		}
	}
}

static void kbx_clear_state(kbx_data *data, int state) {
	log_debug("Cleaning failed initialisation at state %d\n", state);
	switch (state) {
		case 3:
			libusb_detach_kernel_driver(data->dev_handle, 1);
			[[fallthrough]];
		case 2:
			libusb_close(data->dev_handle);
			[[fallthrough]];
		case 1:
			libusb_exit(data->ctx);
			data->ctx = NULL;
			break;
		default:
			break;
	};
}

int kbx_init(kbx_data *data) {
	int r;

	log_debug("Initializing libusb context\n");
	r = libusb_init_context(&data->ctx, NULL, 0);
	if (r) {
		log_error("Failed to initialize libusb context", r);
		return r;
	}

	log_debug("Fetching correct usb device\n");
	libusb_device **list;
	ssize_t cnt = libusb_get_device_list(data->ctx, &list);

	ssize_t mouse_idx = -1;

	for (ssize_t i = 0; i < cnt; i++) {
		libusb_device *device = list[i];
		struct libusb_device_descriptor desc;
		r = libusb_get_device_descriptor(device, &desc);
		if (r < 0)
			continue;
		
		if (desc.idVendor == 0x260d && (desc.idProduct == 0x1074 || desc.idProduct == 0x1113)) {
			mouse_idx = i;
			data->dev_desc = desc;
			log_debug("Found potential mouse : Device %zd: Vendor ID: %04x, Product ID: %04x\n", i, desc.idVendor, desc.idProduct);
			break;
		}
	}

	if (mouse_idx == -1) {
		log_error("No usb device found.", 0);
		kbx_clear_state(data, 1);
		return 1;
	}

	log_debug("Opening usb device\n");
	r = libusb_open(list[mouse_idx], &data->dev_handle);
	libusb_free_device_list(list, 1);

	if (r) {
		kbx_clear_state(data, 1);
		log_error("Failed to open usb device, did you add 99-klimblazex.rules into /etc/udev/rules.d/ and reloaded udev ?", r);
		return r;
	}

	log_debug("Detaching kernel driver if active\n");
	if (libusb_kernel_driver_active(data->dev_handle, 1) == 1) {
		log_debug("Kernel driver active at interface 1. Detaching...\n");
		r = libusb_detach_kernel_driver(data->dev_handle, 1);
		if (r) {
			log_error("Failed to detach kernel driver at interface 1", r);
			kbx_clear_state(data, 2);
			return r;
		}
	}

	log_debug("Claiming interface 1\n");
	r = libusb_claim_interface(data->dev_handle, 1);
	if (r) {
		log_error("Failed to claim interface 1", r);
		kbx_clear_state(data, 3);
		return r;
	}

	data->battery_level = -1;
	data->charging_status = -1;

	return 0;
}

int kbx_refresh(kbx_data *data) {
	unsigned char data_frag[] = {
		0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x49
	};
	int data_len = sizeof(data_frag);

	log_debug("Sending control transfer to request battery status\n");
	int r = libusb_control_transfer(
		data->dev_handle,
		LIBUSB_DT_HID,
		LIBUSB_CLASS_HUB,
		0x0308,
		0x0001,
		data_frag,
		data_len,
		1000
	);

	if (r < 0) {
		log_error("Control transfer failed", r);
		return r;
	}


	unsigned char buf[17];
	int transferred;

	log_debug("Receiving interrupt transfer with battery status\n");
	r = libusb_interrupt_transfer(
		data->dev_handle,
		0x82,
		buf,
		sizeof(buf),
		&transferred,
		1000
	);

	if (r) {
		log_error("Interrupt transfer failed", r);
	} 
	
	if (transferred > 2 && buf[0] == 0x09 && buf[1] == 0x04) {
		data->battery_level = buf[6];
		data->charging_status = buf[7] & 0x01;
		return 0;
	} else {
		return r;
	}
}

void kbx_release(kbx_data *data) {
	if (data->ctx) {
		log_debug("Releasing interfaces and closing device\n");
		libusb_release_interface(data->dev_handle, 1);
		libusb_detach_kernel_driver(data->dev_handle, 1);
		libusb_close(data->dev_handle);
		libusb_exit(data->ctx);
	}
}
