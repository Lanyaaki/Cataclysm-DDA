# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := SDL2
LOCAL_SRC_FILES := lib/libSDL2.so

include $(PREBUILT_SHARED_LIBRARY)

#DDA
include $(CLEAR_VARS)

LOCAL_MODULE    := DDA
LOCAL_CFLAGS    := -g -O0 -DLINUX -DSDL=1 -DANDROID=1 -DGLES=1 -DEGL=1 -DGL_GLEXT_PROTOTYPES
LOCAL_CPPFLAGS  := ${LOCAL_CFLAGS}

LOCAL_C_INCLUDES  :=  \
	$(LOCAL_PATH) \
    $(LOCAL_PATH)/include/ \
    $(LOCAL_PATH)/include/SDL/ \
	$(LOCAL_PATH)/../..

#LOCAL_SRC_FILES :=  sdlcurse.cpp
LOCAL_SRC_FILES := \
    droidcurse.cpp \
	SDL_android_main.cpp \
	$(subst $(LOCAL_PATH)/,, \
	$(wildcard $(LOCAL_PATH)/../../*.cpp)) 

LOCAL_LDLIBS    := -L../lib  -lGLESv1_CM

LOCAL_STATIC_LIBRARIES := SDL2

include $(BUILD_SHARED_LIBRARY)

