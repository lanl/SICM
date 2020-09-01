#include <stdio.h>

#include <sicm_low.h>

int main() {
    sicm_device_list devs = sicm_init();
    sicm_device *default_dev = sicm_default_device(0);

    if (default_dev != sicm_default_device(-1)) {
        fprintf(stderr, "Default SICM device changed unexpectedly.\n");
        return 1;
    }

    // need at least 2 devices
    if (default_dev) {
        sicm_device *first = devs.devices[0];

        if (default_dev != first) {
            fprintf(stderr, "Default SICM device was not the device at index 0.\n");
            return 1;
        }

        for(unsigned int i = 1; i < devs.count; i++) {
            sicm_device *new_default = sicm_default_device(i);

            // should not be the same as device[0]
            if (first == new_default) {
                fprintf(stderr, "Changing the default device somehow got the device at index 0\n");
                return 1;
            }

            // default device should not change
            if (new_default != sicm_default_device(-1)) {
                fprintf(stderr, "Default SICM device (index %u) changed unexpectedly.\n", i);
                return 1;
            }
        }
    }

    sicm_fini();
    return 0;
}
