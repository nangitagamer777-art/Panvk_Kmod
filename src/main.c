#include "../include/kbase_shim.h"
#include <stdio.h>

int test_end_opcodes(kbase_shim_device_t *dev);

int main() {
    printf("==================================================\n");
    printf("   panvk_kmod — END opcode fuzzing\n");
    printf("==================================================\n");

    kbase_shim_device_t dev;

    if (kbase_shim_init(&dev) == 0) {
        printf("\n[ ok ] Context ready.\n");
        kbase_shim_create_group_v16(&dev);
        test_end_opcodes(&dev);
        kbase_shim_close(&dev);
        return 0;
    }
    printf("\n[ fail ] Init failed.\n");
    return 1;
}
