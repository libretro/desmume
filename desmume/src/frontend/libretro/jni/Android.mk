LOCAL_PATH := $(call my-dir)

CORE_DIR     := $(LOCAL_PATH)/../../..
LIBRETRO_DIR := $(CORE_DIR)/frontend/libretro

JIT             :=
DESMUME_JIT     := 0
DESMUME_JIT_ARM := 0
HAVE_LIBFAT     := 1

ifeq ($(TARGET_ARCH),arm)
  DESMUME_JIT_ARM := 1
  JIT             := -DHAVE_JIT
else ifeq ($(TARGET_ARCH),x86)
  DESMUME_JIT := 1
  JIT         := -DHAVE_JIT
endif

include $(LIBRETRO_DIR)/Makefile.common

COREFLAGS := -D__LIBRETRO__ -DANDROID $(INCDIR) $(JIT) -Wno-c++11-narrowing

GIT_VERSION := " $(shell git rev-parse --short HEAD || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
  COREFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

include $(CLEAR_VARS)
LOCAL_MODULE       := retro
LOCAL_SRC_FILES    := $(SOURCES_CXX) $(SOURCES_C)
LOCAL_CXXFLAGS     := $(COREFLAGS) -std=gnu++11
LOCAL_CFLAGS       := $(COREFLAGS)
LOCAL_LDFLAGS      := -Wl,-version-script=$(LIBRETRO_DIR)/link.T
LOCAL_CPP_FEATURES := exceptions
include $(BUILD_SHARED_LIBRARY)
