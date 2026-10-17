LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := neverlose

IMGUI := imgui

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/$(IMGUI) \
    $(LOCAL_PATH)/$(IMGUI)/backends \
    $(LOCAL_PATH)/menu

LOCAL_SRC_FILES := \
    $(IMGUI)/imgui.cpp \
    $(IMGUI)/imgui_draw.cpp \
    $(IMGUI)/imgui_tables.cpp \
    $(IMGUI)/imgui_widgets.cpp \
    $(IMGUI)/backends/imgui_impl_android.cpp \
    $(IMGUI)/backends/imgui_impl_opengl3.cpp \
    menu/neverlose_menu.cpp \
    menu/hexsync_intro.cpp \
    menu/hexsync_glow.cpp \
    menu/blur.cpp \
    menu/gui.cpp \
    main.cpp

LOCAL_CFLAGS += -DIMGUI_IMPL_OPENGL_ES3 -DIMGUI_DISABLE_DEMO_WINDOWS -O2
LOCAL_CPPFLAGS += -std=c++17

LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv3
LOCAL_STATIC_LIBRARIES := android_native_app_glue

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
