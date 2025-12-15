# Makefile for CTP API project

CC = g++
CFLAGS = -std=c++11 -Ilib/ctpapi_v6.7.11 -Ilib/nlohmann_json_v3.12.0 -Wall -g
LDFLAGS = -L./lib/ctpapi_v6.7.11 -lthostmduserapi_se -lpthread

BUILD_DIR = build
TARGET = ctpapi-md-demo
SOURCES = main.cpp MyMdSpi.cpp config.cpp
HEADERS = MyMdSpi.h config.h
OBJECTS = $(addprefix $(BUILD_DIR)/, $(SOURCES:.cpp=.o))

$(BUILD_DIR)/$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJECTS): | $(BUILD_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

run: $(BUILD_DIR)/$(TARGET)
	cd $(BUILD_DIR) && LD_LIBRARY_PATH=../lib/ctpapi_v6.7.11 ./$(TARGET)

.PHONY: clean run