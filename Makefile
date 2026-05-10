.PHONY: clean configure build test

CMAKE ?= cmake
CTEST ?= ctest
BUILD_DIR ?= build-gtest
VCPKG_ROOT ?= $(HOME)/vcpkg
VCPKG_BINARY_SOURCES_VALUE ?= clear
NINJA ?= $(shell command -v ninja)
TOOLCHAIN_FILE := $(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake

ifndef NINJA
$(error ninja is required but was not found in PATH)
endif

configure:
	VCPKG_BINARY_SOURCES="$(VCPKG_BINARY_SOURCES_VALUE)" $(CMAKE) -S . -B $(BUILD_DIR) -G Ninja \
		-DCMAKE_MAKE_PROGRAM="$(NINJA)" \
		-DCMAKE_TOOLCHAIN_FILE="$(TOOLCHAIN_FILE)"

build: configure
	$(CMAKE) --build $(BUILD_DIR)

test: build
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure

clean:
	rm -rf $(BUILD_DIR)
