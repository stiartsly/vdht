LOCAL_PATH := $(call my-dir)

# for libvutils.a
include $(CLEAR_VARS)

vutils_src_files  := \
    utils/varray.c \
    utils/vdict.c \
    utils/vhashmap.c \
    utils/vmem.c \
    utils/vmisc.c \
    utils/vsys.c

vutils_inc_path   := \
    $(LOCAL_PATH)/utils \
    $(LOCAL_PATH)/../sqlite3-3.6.22

LOCAL_MODULE      := libvutils
LOCAL_MODULE_TAGS := optional eng
LOCAL_SRC_FILES   := $(vutils_src_files)
LOCAL_C_INCLUDES  := $(vutils_inc_path)

LOCAL_CFLAGS      += -D_GNU_SOURCE -D_ANDROID

include $(BUILD_STATIC_LIBRARY)

#for libvdht.so
include $(CLEAR_VARS)

vdht_src_files    := \
    vapp.c \
    vcfg.c \
    vdht.c \
    vdht_core.c \
    vhost.c \
    vlog.c \
    vlsctl.c \
    vmsger.c \
    vnode.c \
    vnodeId.c \
    vnode_nice.c \
    vperf.c \
    vroute.c \
    vroute_conn.c \
    vroute_node.c \
    vroute_recr.c \
    vroute_srvc.c \
    vroute_srvc_probe.c \
    vrpc.c \
    vticker.c \
    vupnpc.c

vdht_inc_path     := \
    $(LOCAL_PATH)/3rdparties/miniupnpc \
    $(LOCAL_PATH)/utils \
    $(LOCAL_PATH)/../sqlite3-3.6.22

LOCAL_MODULE      := libvdht
LOCAL_MODULE_TAGS := optional eng
LOCAL_SRC_FILES   := $(vdht_src_files)
LOCAL_C_INCLUDES  := $(vdht_inc_path)

LOCAL_CFLAGS      += -D_GNU_SOURCE -D_ANDROID
LOCAL_LDFLAGS     += -lsqlite
LOCAL_STATIC_LIBRARIES += libminiupnpc libvutils
LOCAL_LDLIBS      += -lpthread

include $(BUILD_SHARED_LIBRARY)

# for vdhtd
include $(CLEAR_VARS)

vdhtd_inc_path    := \
    $(LOCAL_PATH)/utils \
    $(LOCAL_PATH)/../sqlite3-3.6.22

LOCAL_MODULE      := vdhtd
LOCAL_MODULE_TAGS := optional eng
LOCAL_SRC_FILES   := vmain.c
LOCAL_C_INCLUDES  := $(vdhtd_inc_path)

LOCAL_CFLAGS      += -D_GNU_SOURCE -D_ANDROID
LOCAL_SHARED_LIBRARIES += libvdht

include $(BUILD_EXECUTABLE)

# for libvdhtapi.so
include $(CLEAR_VARS)

vdhtapi_src_files := \
    lsctl/vhashgen.c \
    lsctl/vlsctlc.c \
    lsctl/vdhtapi.c

LOCAL_MODULE      := libvdhtapi
LOCAL_MODULE_TAGS := optional eng
LOCAL_SRC_FILES   := $(vdhtapi_src_files)
LOCAL_C_INCLUDES  := $(LOCAL_PATH)/lsctl

LOCAL_CFLAGS      += -D_GNU_SOURCE -D_ANDROID

include $(BUILD_SHARED_LIBRARY)

# for vlsctlc
include $(CLEAR_VARS)

LOCAL_MODULE      := vlsctlc
LOCAL_MODULE_TAGS := optional eng
LOCAL_SRC_FILES   := lsctl/vlsctlc_main.c

LOCAL_CFLAGS      += -D_GNU_SOURCE -D_ANDROID
LOCAL_SHARED_LIBRARIES += libvdhtapi

include $(BUILD_EXECUTABLE)

# for vserver
include $(CLEAR_VARS)

LOCAL_MODULE      := vserver
LOCAL_MODULE_TAGS := optional eng
LOCAL_SRC_FILES   := example/vserver_main.c
LOCAL_C_INCLUDES  := $(LOCAL_PATH)/lsctl

LOCAL_CFLAGS      += -D_GNU_SOURCE -D_ANDROID
LOCAL_SHARED_LIBRARIES += libvdhtapi

include $(BUILD_EXECUTABLE)

# for vclient
include $(CLEAR_VARS)

LOCAL_MODULE      := vclient
LOCAL_MODULE_TAGS := optional eng
LOCAL_SRC_FILES   := example/vclient_main.c
LOCAL_C_INCLUDES  := $(LOCAL_PATH)/lsctl

LOCAL_CFLAGS      += -D_GNU_SOURCE -D_ANDROID
LOCAL_SHARED_LIBRARIES += libvdhtapi

include $(BUILD_EXECUTABLE)

# for vhashgen
include $(CLEAR_VARS)

vhashgen_inc_path := \
    $(LOCAL_PATH)/utils \
    $(LOCAL_PATH)/../sqlite3-3.6.22

LOCAL_MODULE      := vhashgen
LOCAL_MODULE_TAGS := optional eng
LOCAL_SRC_FILES   := hashgen/vhashgen_main.c
LOCAL_C_INCLUDES  := $(vhashgen_inc_path)

LOCAL_CFLAGS      += -D_GNU_SOURCE -D_ANDROID

include $(BUILD_EXECUTABLE)
