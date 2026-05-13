.PHONY: clean configure build test lint analyze fix devci

CMAKE ?= cmake
CTEST ?= ctest
BUILD_DIR ?= build-gtest
VCPKG_ROOT ?= $(HOME)/vcpkg
VCPKG_BINARY_SOURCES_VALUE ?= clear
NINJA ?= $(shell command -v ninja)
TOOLCHAIN_FILE := $(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake

# Sources for formatting (all)
ALL_FILES := $(shell find src -name "*.cpp" -o -name "*.hpp")
# Sources for analysis (only .cpp files to use compilation database effectively)
CPP_FILES := $(shell find src -name "*.cpp")

ifndef NINJA
$(error ninja is required but was not found in PATH)
endif

configure:
	VCPKG_BINARY_SOURCES="$(VCPKG_BINARY_SOURCES_VALUE)" $(CMAKE) -S . -B $(BUILD_DIR) -G Ninja \
		-DCMAKE_MAKE_PROGRAM="$(NINJA)" \
		-DCMAKE_TOOLCHAIN_FILE="$(TOOLCHAIN_FILE)" \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON

build: configure
	$(CMAKE) --build $(BUILD_DIR)

test: build
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure

# LINT: Check formatting and static analysis (read-only)
lint: configure
	@echo "Checking formatting..."
	clang-format --dry-run --Werror $(ALL_FILES)
	@echo "Running clang-tidy..."
	clang-tidy $(CPP_FILES) -p $(BUILD_DIR) --quiet --extra-arg=--gcc-toolchain=/usr

# DEVCI: Run build, test, and lint
# --security-opt seccomp=unconfined: rootless Podman's default seccomp profile blocks
# syscalls (mknod, setcap) that Ubuntu 24.04 dpkg postinst scripts require. This lifts
# that filter for the build; the container is ephemeral so the risk is acceptable.
devci:
	podman build --network=host --security-opt seccomp=unconfined --target bm25-devci-full -f containerfiles/dev-ci .

# ANALYZE: Deep static analysis using scan-build
analyze: clean
	@echo "Running scan-build..."
	VCPKG_BINARY_SOURCES="$(VCPKG_BINARY_SOURCES_VALUE)" scan-build \
		$(CMAKE) -S . -B $(BUILD_DIR) -G Ninja \
		-DCMAKE_MAKE_PROGRAM="$(NINJA)" \
		-DCMAKE_TOOLCHAIN_FILE="$(TOOLCHAIN_FILE)"
	scan-build --status-bugs $(CMAKE) --build $(BUILD_DIR)

# FIX: Automatically apply formatting and tidy fixes
fix: configure
	@echo "Applying clang-format fixes..."
	clang-format -i $(ALL_FILES)
	@echo "Applying clang-tidy fixes..."
	clang-tidy $(CPP_FILES) -p $(BUILD_DIR) --fix --fix-errors --quiet --extra-arg=--gcc-toolchain=/usr

clean:
	rm -rf $(BUILD_DIR)
