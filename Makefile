BUILD_TYPE?=Debug
OUTPUT_DIR?=build/$(BUILD_TYPE)
ARGS?=

.PHONY: all run


all: $(OUTPUT_DIR)/build.ninja
	ninja -C$(OUTPUT_DIR)

run: all
	./$(OUTPUT_DIR)/ToyRenderer/ToyRenderer $(ARGS)


clean:
	rm -rf ./$(OUTPUT_DIR)

$(OUTPUT_DIR)/build.ninja:
	cmake -B$(OUTPUT_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -GNinja
