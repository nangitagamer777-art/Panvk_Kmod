#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <dlfcn.h>
#include <signal.h>

static PFN_vkGetInstanceProcAddr GPA;

int main() {
    signal(SIGSEGV, SIG_IGN);
    signal(SIGABRT, SIG_IGN);
    
    dlerror();
    void *lib = dlopen("/data/local/tmp/libvulkan_panfrost.so", RTLD_NOW);
    if (!lib) { printf("dlopen failed: %s\n", dlerror()); return 1; }
    GPA = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vk_icdGetInstanceProcAddr");
    if (!GPA) { printf("no GPA\n"); return 1; }
    
    PFN_vkCreateInstance vkCreateInstance = (PFN_vkCreateInstance)GPA(NULL, "vkCreateInstance");
    VkApplicationInfo appInfo = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "test", .apiVersion = VK_API_VERSION_1_0 };
    VkInstanceCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &appInfo };
    
    VkInstance instance = NULL;
    VkResult res = vkCreateInstance(&createInfo, NULL, &instance);
    printf("vkCreateInstance: %d, instance=%p\n", res, instance);
    
    if (res == VK_SUCCESS && instance) {
        PFN_vkEnumeratePhysicalDevices vkEnumerate = (PFN_vkEnumeratePhysicalDevices)GPA(instance, "vkEnumeratePhysicalDevices");
        uint32_t count = 0;
        vkEnumerate(instance, &count, NULL);
        printf("GPU count: %u\n", count);
        
        if (count > 0) {
            printf("SUCCESS: GPU detected!\n");
        }
    }
    
    dlclose(lib);
    return 0;
}
