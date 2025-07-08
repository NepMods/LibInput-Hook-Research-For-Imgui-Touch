#include <dobby.h>
#include <xdl/xdl.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <android/input.h>
#include "imgui_impl_android.h"
#define LOG_TAG "ImGuiTouchHook"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

void (*origInput)(void *, void *, void *);
void *myInput(void *thiz, void *ex_ab, void *ex_ac) {
    origInput(thiz, ex_ab, ex_ac);
    ImGui_ImplAndroid_HandleInputEvent((AInputEvent *)thiz);
    return;
}

int32_t (*origInput2)(void *instanceTHiz, void *param_1, bool param_2, long param_3, void *param_4, void **thiz_ptr);
int32_t myInput2(void *instanceTHiz, void *param_1, bool param_2, long param_3, void *param_4, void **thiz_ptr) {
    int32_t result = origInput2(instanceTHiz, param_1, param_2, param_3, param_4, thiz_ptr);
    if(thiz_ptr) {
        if(*thiz_ptr) {
            auto thiz = *thiz_ptr;
            ImGui_ImplAndroid_HandleInputEvent((AInputEvent *)thiz);
        }
    }
    return result;
}

void *imgui_touch(void *) {

    sleep(3);

    do {
        sleep(1);
    } while (!IsLibraryLoaded("libEGL.so"));

    auto libIn = xdl_open("libinput.so", RTLD_NOW);

    if (libIn == nullptr) {
        LOGE("Failed to open libinput.so");
        return nullptr;
    }
    LOGI("Opened libinput.so at %p", libIn);

    // try HOOK the old version 

    auto old_touch = xdl_sym(libIn, "_ZN7android13InputConsumer21initializeMotionEventEPNS_11MotionEventEPKNS_12InputMessageE", nullptr);

    if( old_touch != nullptr) {
        LOGI("Found old touch address at %p", old_touch);
        DobbyHook(addr,(void *)myInput,(void **)&origInput);
        return nullptr;
    }

    // HOOK the new version
#ifdef __aarch64__
    const char *consume_symbol = "_ZN7android13InputConsumer7consumeEPNS_26InputEventFactoryInterfaceEblPjPPNS_10InputEventE";
#else
    const char *consume_symbol = "_ZN7android13InputConsumer7consumeEPNS_26InputEventFactoryInterfaceEbxPjPPNS_10InputEventE";
#endif
    auto new_addr = xdl_sym(libIn, consume_symbol,
                        nullptr);
    if (new_addr == nullptr) {
        LOGE("Failed to find new touch address");
        return nullptr;
    }
    LOGI("Found new touch address at %p", new_addr);
    DobbyHook(addr,(void *)myInput2,(void **)&origInput2);
   

    return nullptr;
}


__attribute__((constructor)) void lib_main()
{    pthread_t ptid;
    pthread_create(&ptid, NULL, imgui_touch, NULL);

}
