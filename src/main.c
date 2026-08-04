#include "../include/kbase_shim.h"
#include <stdio.h>

int kbase_shim_create_group_v16(kbase_shim_device_t *dev);

int main() {
    printf("==================================================\n");
    printf("   panvk_kmod — group v1.6 (cmd 42)\n");
    printf("==================================================\n");

    kbase_shim_device_t dev;

    if (kbase_shim_init(&dev) == 0) {
        printf("\n[ ok ] Phase 1 & 2 done.\n");
        kbase_shim_get_gpu_props(&dev);
        kbase_shim_create_group_v16(&dev);
        kbase_shim_csf_kick_and_wait_v4(&dev);
        kbase_shim_close(&dev);
        return 0;
    } else {
        printf("\n[ fail ] Init blew up.\n");
        return 1;
    }
}
