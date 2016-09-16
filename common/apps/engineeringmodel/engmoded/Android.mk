LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
#include $(BUILD_SHARED_LIBRARY)

LOCAL_CFLAGS += -DBUILD_ENG
LOCAL_SRC_FILES:= HTTPServer.cpp HTTPRequest.cpp HTTPResponse.cpp HTTPServerMain.cpp ATProcesser.cpp eng_appclient_lib.c 

LOCAL_C_INCLUDES    += engclient.h  engopt.h engapi.h device/sprd/common/apps/engineeringmodel/engcs
LOCAL_MODULE := engmoded
LOCAL_STATIC_LIBRARIES := libcutils 
LOCAL_SHARED_LIBRARIES := libstlport libengclient
LOCAL_MODULE_TAGS := optional
include external/stlport/libstlport.mk
include $(BUILD_EXECUTABLE)
