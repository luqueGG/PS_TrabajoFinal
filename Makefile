BUILD_DIR := build

.PHONY: all test clean run

all: $(BUILD_DIR)/build.ninja
	ninja -C $(BUILD_DIR)

$(BUILD_DIR)/build.ninja:
	cmake -G Ninja -B $(BUILD_DIR) -S .

test: all
	cd $(BUILD_DIR) && ctest --output-on-failure

run: all
	./$(BUILD_DIR)/psadmin

clean:
	rm -rf $(BUILD_DIR)
