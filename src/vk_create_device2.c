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
    
    /* Create instance with Vulkan 1.0 */
    PFN_vkCreateInstance vkCreateInstance = (PFN_vkCreateInstance)GPA(NULL, "vkCreateInstance");
    VkApplicationInfo ai = {.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.pApplicationName="test",.apiVersion=VK_API_VERSION_1_1};
    VkInstanceCreateInfo ci = {.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&ai};
    VkInstance inst;
    vkCreateInstance(&ci, NULL, &inst);
    
    PFN_vkEnumeratePhysicalDevices vkEnumPD = (PFN_vkEnumeratePhysicalDevices)GPA(inst, "vkEnumeratePhysicalDevices");
    VkPhysicalDevice pd;
    uint32_t pdCount = 1;
    vkEnumPD(inst, &pdCount, &pd);
    
    /* vkCreateDevice is a GLOBAL function, get it with NULL */
    PFN_vkCreateDevice vkCreateDevice = (PFN_vkCreateDevice)GPA(inst, "vkCreateDevice");
    printf("vkCreateDevice: %p\n", vkCreateDevice);
    
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = {.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,.queueFamilyIndex=0,.queueCount=1,.pQueuePriorities=&prio};
    VkPhysicalDeviceFeatures feats = {.robustBufferAccess=VK_TRUE};
    VkDeviceCreateInfo dci = {.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.queueCreateInfoCount=1,.pQueueCreateInfos=&qci,.pEnabledFeatures=&feats}; const char *pexts[] = {"VK_KHR_portability_subset"}; dci.enabledExtensionCount = 1; dci.ppEnabledExtensionNames = pexts;
    
    VkDevice dev;
    VkResult r = vkCreateDevice(pd, &dci, NULL, &dev);
    printf("vkCreateDevice: %d (dev=%p)\n", r, dev);
    
    if (r == VK_SUCCESS) {
        PFN_vkDestroyDevice vkDestroyDevice = (PFN_vkDestroyDevice)GPA(inst, "vkDestroyDevice");
        vkDestroyDevice(dev, NULL);
    }
    
    PFN_vkDestroyInstance vkDestroyInstance = (PFN_vkDestroyInstance)GPA(inst, "vkDestroyInstance");
    vkDestroyInstance(inst, NULL);
    dlclose(lib);
    return 0;
}
