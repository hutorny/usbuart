APP_ABI := armeabi-v7a
APP_ABI := $(or $(APP_ABI),all)

#If no platform specified, using the most recent one
APP_PLATFORM := $(or $(APP_PLATFORM),$(shell echo `for i in $(NDK)/platforms/*-?? ; do basename $${i%%}; done | tail -1`))
# Workaround for MIPS toolchain linker being unable to find liblog dependency
# of shared object in NDK versions at least up to r9.
#
APP_LDFLAGS := -llog
APP_PIE := 1
APP_STL := gnustl_static
NDK_TOOLCHAIN_VERSION  := 4.9
