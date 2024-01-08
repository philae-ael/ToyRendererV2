BUILD_TYPE?=Debug
OUTPUT_DIR?=build/$(BUILD_TYPE)
VCPKG?=~/.vcpkg/
ARGS?=

.PHONY: all run clean format


all: $(OUTPUT_DIR)/build.ninja
	ninja -C$(OUTPUT_DIR)

run: all 
	./$(OUTPUT_DIR)/ToyRenderer/ToyRenderer $(ARGS)

build/shaders:
	mkdir -p build/shaders

clean:
	rm -rf ./$(OUTPUT_DIR)

format:
	find ToyRenderer/ -iname '*.h' -o -iname '*.cpp' | xargs clang-format -i
	find utils/ -iname '*.h' -o -iname '*.cpp' | xargs clang-format -i

$(OUTPUT_DIR)/build.ninja:
	cmake -B$(OUTPUT_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -GNinja -DCMAKE_TOOLCHAIN_FILE=$(VCPKG)/scripts/buildsystems/vcpkg.cmake
