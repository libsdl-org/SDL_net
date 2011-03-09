LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := SDL_net

LOCAL_CFLAGS := -I$(LOCAL_PATH)/../SDL/include

LOCAL_SRC_FILES := SDLnet.c SDLnetTCP.c SDLnetUDP.c SDLnetselect.c

LOCAL_SHARED_LIBRARIES := SDL

include $(BUILD_SHARED_LIBRARY)
