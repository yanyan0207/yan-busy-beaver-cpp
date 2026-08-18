N        ?= 4
BUILD    := build
CMAKE_FLAGS := -S . -B $(BUILD) -G Ninja \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Release

.PHONY: configure build run test format tidy clean

configure:
	cmake $(CMAKE_FLAGS)

# CMakeLists.txt が変わったときだけ再 configure
$(BUILD)/build.ninja: CMakeLists.txt
	cmake $(CMAKE_FLAGS)

build: $(BUILD)/build.ninja
	cmake --build $(BUILD) --config Release

run: build
	./$(BUILD)/bb_search.exe $(N)

test: build
	ctest --test-dir $(BUILD) --output-on-failure

format:
	uv run pre-commit run clang-format --all-files

tidy:
	uv run pre-commit run clang-tidy --all-files

clean:
	cmake --build $(BUILD) --target clean

quest: build
	./$(BUILD)/bb_simple_search.exe $(N)
