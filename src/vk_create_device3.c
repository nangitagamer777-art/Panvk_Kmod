#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <signal.h>

int main() {
    signal(SIGSEGV, SIG_IGN);
    signal(SIGABRT, SIG_IGN);
    
    void *lib = dlopen("/data/local/tmp/libvulkan_panfrost.so", RTLD_NOW);
    if (!lib) { printf("dlopen failed\n"); return 1; }
    PFN_vkGetInstanceProcAddr GPA = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vk_icdGetInstanceProcAddr");
    
    /* Instance */
    PFN_vkCreateInstance vkCreateInstance = (PFN_vkCreateInstance)GPA(NULL, "vkCreateInstance");
    VkApplicationInfo ai = {.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.pApplicationName="test",.apiVersion=VK_API_VERSION_1_0};
    VkInstanceCreateInfo ci = {.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&ai};
    VkInstance inst;
    vkCreateInstance(&ci, NULL, &inst);
    
    /* Physical device */
    PFN_vkEnumeratePhysicalDevices vkEnumPD = (PFN_vkEnumeratePhysicalDevices)GPA(inst, "vkEnumeratePhysicalDevices");
    VkPhysicalDevice pd;
    uint32_t pdCount = 1;
    vkEnumPD(inst, &pdCount, &pd);
    
    /* Get device extensions to see if portability is there */
    PFN_vkEnumerateDeviceExtensionProperties vkEnumDevExt = (PFN_vkEnumerateDeviceExtensionProperties)GPA(inst, "vkEnumerateDeviceExtensionProperties");
    uint32_t extCount = 0;
    vkEnumDevExt(pd, NULL, &extCount, NULL);
    VkExtensionProperties *exts = calloc(extCount, sizeof(*exts));
    vkEnumDevExt(pd, NULL, &extCount, exts);
    
    int hasPortability = 0;
    for (uint32_t i = 0; i < extCount; i++) {
        if (strcmp(exts[i].extensionName, "VK_KHR_portability_subset") == 0) hasPortability = 1;
    }
    printf("Portability subset: %s\n", hasPortability ? "YES" : "NO");
    free(exts);
    
    /* Create device with NO features, NO extensions */
    PFN_vkCreateDevice vkCreateDevice = (PFN_vkCreateDevice)GPA(inst, "vkCreateDevice");
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = {.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,.queueFamilyIndex=0,.queueCount=1,.pQueuePriorities=&prio};
    VkPhysicalDeviceFeatures feats = {0};
    VkDeviceCreateInfo dci = {.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.queueCreateInfoCount=1,.pQueueCreateInfos=&qci,.pEnabledFeatures=&feats,.enabledExtensionCount=0};
    
    VkDevice dev;
    VkResult r = vkCreateDevice(pd, &dci, NULL, &dev);
    printf("vkCreateDevice: %d\n", r);
    
    if (r == VK_SUCCESS) {
        printf("SUCCESS! Device created.\n");
        PFN_vkDestroyDevice vkDestroyDevice = (PFN_vkDestroyDevice)GPA(inst, "vkDestroyDevice");
        vkDestroyDevice(dev, NULL);
    }
    
    PFN_vkDestroyInstance vkDestroyInstance = (PFN_vkDestroyInstance)GPA(inst, "vkDestroyInstance");
    vkDestroyInstance(inst, NULL);
    dlclose(lib);
    return 0;
}
