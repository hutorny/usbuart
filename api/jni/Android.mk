LOCAL_PATH := $(call my-dir)

USBUART_PATH := $(realpath $(LOCAL_PATH)/../..)
USBUART_API  := $(realpath $(LOCAL_PATH)/..)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := 														\
	$(USBUART_PATH)/include													\
	$(USBUART_PATH)/src														\

LOCAL_CPPFLAGS := -fPIC -std=c++1y  -fexceptions -fno-rtti -DDEBUG
LOCAL_CFLAGS   := -fPIC -DDEBUG
LOCAL_SHARED_LIBRARIES := usb-1.0

LOCAL_SRC_FILES := \
  $(USBUART_PATH)/src/core.cpp												\
  $(USBUART_PATH)/src/generic.cpp											\
  $(USBUART_PATH)/src/ch34x.cpp												\
  $(USBUART_PATH)/src/ftdi.cpp												\
  $(USBUART_PATH)/src/pl2303.cpp											\
  $(LOCAL_PATH)/alog.cpp													\
  $(LOCAL_PATH)/info_usbuart_api_UsbUartContext.cpp							\

LOCAL_CLASS_PATH = $(USBUART_API)/gen:$(SDK)/platforms/$(APP_PLATFORM)/android.jar

$(LOCAL_MODULE): $(LOCAL_PATH)/info_usbuart_api_UsbUartContext.h

LOCAL_JAVA_SRC_FILES := EIA_TIA_232_Info.java Channel.java UsbUartContext.java
LOCAL_JAVA_SOURCES = $(addprefix											\
	$(USBUART_API)/src/info/usbuart/api/, $(LOCAL_JAVA_SRC_FILES))

$(USBUART_API)/gen/info/usbuart/api/UsbUartContext.class: $(LOCAL_JAVA_SOURCES)   
	$(if $(JAVA_HOME),,$(error JAVA_HOME is not set))
	$(if $(SDK),,$(error SDK is not set))
	$(hide) $(JAVA_HOME)/bin/javac $^ -classpath $(LOCAL_CLASS_PATH) 		\
		-d $(USBUART_API)/gen

$(LOCAL_PATH)/info_usbuart_api_UsbUartContext.h:									\
  $(USBUART_API)/gen/info/usbuart/api/UsbUartContext.class
	$(if $(JAVA_HOME),,$(error JAVA_HOME is not set))
	$(if $(SDK),,$(error SDK is not set))
	$(hide) $(JAVA_HOME)/bin/javah -jni										\
		-cp $(USBUART_API)													\
		-classpath $(LOCAL_CLASS_PATH)										\
		info.usbuart.api.UsbUartContext

LOCAL_MODULE := usbuart

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
include $(LOCAL_PATH)/../../libusb/android/jni/libusb.mk
