#define _GNU_SOURCE
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <signal.h>
#include <setjmp.h>
#include <string.h>
#include <unistd.h>

static jmp_buf jump;
static int crash_count = 0;

void sig_handler(int sig, siginfo_t *info, void *ctx) {
    crash_count++;
    fprintf(stderr, "[test] Signal %d at addr %p (crash #%d)\n", sig, info->si_addr, crash_count);
    if (crash_count < 10) {
        longjmp(jump, 1);
    } else {
        fprintf(stderr, "[test] Too many crashes, exiting\n");
        _exit(1);
    }
}

int main() {
    struct sigaction sa = { .sa_sigaction = sig_handler, .sa_flags = SA_SIGINFO };
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    
    void *lib = dlopen("/data/local/tmp/libvulkan_panfrost.so", RTLD_NOW);
    if (!lib) { printf("dlopen failed\n"); return 1; }
    PFN_vkGetInstanceProcAddr GPA = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vk_icdGetInstanceProcAddr");
    
    PFN_vkCreateInstance vkCreateInstance = (PFN_vkCreateInstance)GPA(NULL, "vkCreateInstance");
    VkApplicationInfo ai = {.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.pApplicationName="test",.apiVersion=VK_API_VERSION_1_0};
    VkInstanceCreateInfo ci = {.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&ai};
    VkInstance inst;
    vkCreateInstance(&ci, NULL, &inst);
    
    PFN_vkEnumeratePhysicalDevices vkEnumPD = (PFN_vkEnumeratePhysicalDevices)GPA(inst, "vkEnumeratePhysicalDevices");
    VkPhysicalDevice pd;
    uint32_t pdCount = 1;
    vkEnumPD(inst, &pdCount, &pd);
    
    PFN_vkCreateDevice vkCreateDevice = (PFN_vkCreateDevice)GPA(inst, "vkCreateDevice");
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = {.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,.queueFamilyIndex=0,.queueCount=1,.pQueuePriorities=&prio};
    VkPhysicalDeviceFeatures feats = {0};
    VkDeviceCreateInfo dci = {.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.queueCreateInfoCount=1,.pQueueCreateInfos=&qci,.pEnabledFeatures=&feats};
    
    VkDevice dev;
    VkResult r = VK_ERROR_INITIALIZATION_FAILED;
    
    if (setjmp(jump) == 0) {
        r = vkCreateDevice(pd, &dci, NULL, &dev);
        printf("vkCreateDevice: %d, dev=%p\n", r, dev);
    } else {
        printf("Crashed but recovered (crash #%d)\n", crash_count);
    }
    
    printf("Final result: %d, crashes: %d\n", r, crash_count);
    
    dlclose(lib);
    return 0;
}
