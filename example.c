#include "battery.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
	kbx_data data;

	if (kbx_init(&data)) {
		return 1;
	}

	for (int i = 0; i < 3; i++) {
		kbx_refresh(&data);
		printf("Battery level: %d%%; Charging status: %s\n", data.battery_level, data.charging_status ? "Charging" : "Not charging");
		sleep(1);
	}

	kbx_release(&data);
	return 0;
}