CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
LDFLAGS  =

# 检测是否有X11库，有则开启窗口预览
HAVE_X11 := $(shell pkg-config --exists x11 2>/dev/null && echo yes)
ifeq ($(HAVE_X11), yes)
    CXXFLAGS += -DUSE_X11
    LDFLAGS  += $(shell pkg-config --libs x11)
    $(info X11 found: window preview enabled)
else
    $(info X11 not found: image output only)
endif

TARGET = rasterizer
SRCS   = main.cpp

.PHONY: all clean run run_anim run_obj help

all: $(TARGET)

$(TARGET): $(SRCS) src/math_utils.h src/framebuffer.h src/texture.h src/model.h src/rasterizer.h
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS) $(LDFLAGS)
	@echo "编译完成: ./$(TARGET)"

# 单帧渲染
run: $(TARGET)
	mkdir -p output
	./$(TARGET) --width 800 --height 600

# 多帧动画 (36帧，每10度一帧)
run_anim: $(TARGET)
	mkdir -p output
	./$(TARGET) --width 800 --height 600 --frames 36

# 加载OBJ模型
run_obj: $(TARGET)
	mkdir -p output
	./$(TARGET) --width 800 --height 600 --obj $(OBJ)

# X11窗口预览
run_window: $(TARGET)
	mkdir -p output
	./$(TARGET) --width 800 --height 600 --window

help:
	./$(TARGET) --help

clean:
	rm -f $(TARGET)
	rm -f output/*.tga
