# Makefile for MediatekWiFi KEXT
# Build with: make
# Clean with: make clean

KEXTNAME = MediatekWiFi
KEXT_VERSION = 1.0
SDK = macosx
ARCH = x86_64

# Source files
SRCS = MediatekWifi.cpp

# Build directory
BUILD_DIR = build

# Compiler flags
CXXFLAGS = -fPIC -fno-common -fno-builtin
CXXFLAGS += -static
CXXFLAGS += -mkernel
CXXFLAGS += -nostdinc
CXXFLAGS += -O2

# Include paths for kernel development
KERNEL_PATH = /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
IOKIT_PATH = $(KERNEL_PATH)/System/Library/Frameworks/IOKit.framework

CXXFLAGS += -I$(KERNEL_PATH)/System/Library/Frameworks/Kernel.framework/PrivateHeaders
CXXFLAGS += -I$(IOKIT_PATH)/Headers

# Linker flags
LDFLAGS = -mkernel
LDFLAGS += -nostdlib
LDFLAGS += -lkmod
LDFLAGS += -lcc_kext

# Targets
all: $(BUILD_DIR)/$(KEXTNAME).kext

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/$(KEXTNAME).o: $(SRCS) | $(BUILD_DIR)
	@clang++ $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/$(KEXTNAME).kext: $(BUILD_DIR)/$(KEXTNAME).o Info.plist | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/$(KEXTNAME).kext/Contents/MacOS
	@mkdir -p $(BUILD_DIR)/$(KEXTNAME).kext/Contents/Resources
	@clang++ $(LDFLAGS) -o $(BUILD_DIR)/$(KEXTNAME).kext/Contents/MacOS/$(KEXTNAME) $(BUILD_DIR)/$(KEXTNAME).o
	@cp Info.plist $(BUILD_DIR)/$(KEXTNAME).kext/Contents/
	@chmod -R 755 $(BUILD_DIR)/$(KEXTNAME).kext

clean:
	@rm -rf $(BUILD_DIR)

.PHONY: all clean
