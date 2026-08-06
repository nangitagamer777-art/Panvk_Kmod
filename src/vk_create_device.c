#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <signal.h>

static PFN_vkGetInstanceProcAddr GPA;

int main() {
    signal(SIGSEGV, SIG_IGN);
    signal(SIGABRT, SIG_IGN);
    
    void *lib = dlopen("/data/local/tmp/libvulkan_panfrost.so", RTLD_NOW);
    if (!lib) { printf("dlopen failed\n"); return 1; }
    GPA = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vk_icdGetInstanceProcAddr");
    
    /* Create instance */
    PFN_vkCreateInstance vkCreateInstance = (PFN_vkCreateInstance)GPA(NULL, "vkCreateInstance");
    const char *exts[] = {"VK_KHR_get_physical_device_properties2"}; // minimal
    VkApplicationInfo ai = {.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.pApplicationName="test",.apiVersion=VK_API_VERSION_1_0};
    VkInstanceCreateInfo ci = {.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&ai,.enabledExtensionCount=1,.ppEnabledExtensionNames=exts};
    VkInstance inst;
    VkResult r = vkCreateInstance(&ci, NULL, &inst);
    printf("vkCreateInstance: %d\n", r);
    
    /* Get physical device */
    PFN_vkEnumeratePhysicalDevices vkEnumPD = (PFN_vkEnumeratePhysicalDevices)GPA(inst, "vkEnumeratePhysicalDevices");
    uint32_t pdCount = 0;
    vkEnumPD(inst, &pdCount, NULL);
    VkPhysicalDevice pd;
    vkEnumPD(inst, &pdCount, &pd);
    
    /* Get queue families */
    uint32_t qfCount = 0;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetQF = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)GPA(inst, "vkGetPhysicalDeviceQueueFamilyProperties");
    vkGetQF(pd, &qfCount, NULL);
    VkQueueFamilyProperties *qfs = calloc(qfCount, sizeof(*qfs));
    vkGetQF(pd, &qfCount, qfs);
    
    int gfxIdx = -1, compIdx = -1;
    for (uint32_t i = 0; i < qfCount; i++) {
        printf("  Queue %u: flags=0x%x count=%u\n", i, qfs[i].queueFlags, qfs[i].queueCount);
        if (qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) gfxIdx = i;
        if (qfs[i].queueFlags & VK_QUEUE_COMPUTE_BIT) compIdx = i;
    }
    free(qfs);
    
    if (gfxIdx < 0 && compIdx < 0) { printf("No usable queue\n"); return 1; }
    int useIdx = gfxIdx >= 0 ? gfxIdx : compIdx;
    printf("Using queue family %d\n", useIdx);
    
    /* Query device extensions */
    PFN_vkEnumerateDeviceExtensionProperties vkEnumDevExt = (PFN_vkEnumerateDeviceExtensionProperties)GPA(inst, "vkEnumerateDeviceExtensionProperties");
    uint32_t extCount = 0;
    vkEnumDevExt(pd, NULL, &extCount, NULL);
    VkExtensionProperties *devExts = calloc(extCount, sizeof(*devExts));
    vkEnumDevExt(pd, NULL, &extCount, devExts);
    printf("Device extensions: %u\n", extCount);
    
    /* Try creating device with NO extensions (bare minimum) */
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = {.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,.queueFamilyIndex=useIdx,.queueCount=1,.pQueuePriorities=&prio};
    VkPhysicalDeviceFeatures feats = {0};
    VkDeviceCreateInfo dci = {.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.queueCreateInfoCount=1,.pQueueCreateInfos=&qci,.pEnabledFeatures=&feats,.enabledExtensionCount=0,.ppEnabledExtensionNames=NULL};
    
    PFN_vkCreateDevice vkCreateDevice = (PFN_vkCreateDevice)GPA(inst, "vkCreateDevice");
    VkDevice dev;
    r = vkCreateDevice(pd, &dci, NULL, &dev);
    printf("vkCreateDevice (no exts): %d\n", r);
    
    if (r != VK_SUCCESS) {
        /* Try with VK_KHR_portability_subset if available */
        const char *portExts[] = {"VK_KHR_portability_subset"};
        dci.enabledExtensionCount = 1;
        dci.ppEnabledExtensionNames = portExts;
        r = vkCreateDevice(pd, &dci, NULL, &dev);
        printf("vkCreateDevice (portability): %d\n", r);
    }
    
    if (r == VK_SUCCESS) {
        printf("Device created successfully!\n");
        PFN_vkDestroyDevice vkDestroyDevice = (PFN_vkDestroyDevice)GPA(inst, "vkDestroyDevice");
        if (vkDestroyDevice) vkDestroyDevice(dev, NULL);
    }
    
    PFN_vkDestroyInstance vkDestroyInstance = (PFN_vkDestroyInstance)GPA(inst, "vkDestroyInstance");
    vkDestroyInstance(inst, NULL);
    dlclose(lib);
    return 0;
}
