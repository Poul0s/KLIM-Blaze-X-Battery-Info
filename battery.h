#ifndef KLIMBX_BATTERY_H
# define KLIMBX_BATTERY_H
# include <libusb-1.0/libusb.h>

typedef struct s_kbx_data {
	libusb_context					*ctx;
	struct libusb_device_descriptor	dev_desc;
	libusb_device_handle			*dev_handle;

	int							battery_level;
	int							charging_status;

} kbx_data;

int	kbx_init(kbx_data *data);
int	kbx_refresh(kbx_data *data);
int	kbx_release(kbx_data *data);

#endif