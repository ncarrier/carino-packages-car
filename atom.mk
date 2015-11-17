LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := car
LOCAL_DESCRIPTION := Sensor and actuator controller
LOCAL_CATEGORY_PATH := main

LOCAL_SRC_FILES := car.cpp

LOCAL_LIBRARIES := \
	libci \
	libarduino

include $(BUILD_EXECUTABLE)

