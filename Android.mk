LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := SDL2_net

LOCAL_C_INCLUDES := $(LOCAL_PATH)/src $(LOCAL_PATH)/include
LOCAL_CFLAGS :=

LOCAL_SRC_FILES := src/SDLnet.c src/SDLnetTCP.c src/SDLnetUDP.c src/SDLnetselect.c

LOCAL_SHARED_LIBRARIES := SDL2

LOCAL_EXPORT_C_INCLUDES += $(LOCAL_PATH)/include

# https://developer.android.com/guide/practices/page-sizes
LOCAL_LDFLAGS += "-Wl,-z,max-page-size=16384"
LOCAL_LDFLAGS += "-Wl,-z,common-page-size=16384"

include $(BUILD_SHARED_LIBRARY)
