CXX = g++
LD  = g++
OBJS_DIR = obj

TARGET = cadb_1075_final

# === 編譯模式 ===
ifeq ($(DEBUG), 1)
  CXXFLAGS = -g -O0 -DDEBUG -std=c++17
else ifeq ($(UNIT_TEST), 1)
  CXXFLAGS = -g -O0 -DUNIT_TEST -DDEBUG -std=c++17
else
  CXXFLAGS = -O3 -std=c++17
endif

# 自動相依檔 + pthread（建議同時放在編譯與連結）
CXXFLAGS += -MMD -MP -pthread

# === 來源檔 ===
SRCS = main.cpp \
       $(wildcard src/*.cpp) \
       $(wildcard src/Legalizer/*.cpp) \
       $(wildcard util/*.cpp) \
       $(wildcard parser/util/*.cpp) \
       $(wildcard parser/common/*.cpp)

# 扁平化輸出至 obj/
OBJS = $(addprefix $(OBJS_DIR)/, $(notdir $(SRCS:%.cpp=%.o)))
DEPS = $(OBJS:.o=.d)

# === Include 路徑 ===
INCLUDES = -Isrc -Iutil \
           -Iparser/include \
           -Iparser/include/lef \
           -Iparser/include/def \
           -Iparser/common \
           -Iparser/util \
           -I/usr/local/include

# === Library 路徑與連結（Linux 全靜態樣式） ===
# 需要提供靜態庫：parser/lib/linux/liblef.a、libdef.a（或 /usr/local/lib 下的 .a）
LDFLAGS = -static -no-pie -pthread \
          -L/usr/local/lib -Lparser/lib/linux \
          -Wl,--start-group
LIBS    = -llef -ldef \
          -Wl,--end-group \
          -static-libstdc++ -static-libgcc \
          -ldl -lm

.SUFFIXES :

#-------------------------------------------------------------------------------
# Build Rules
#-------------------------------------------------------------------------------
all: $(TARGET)

$(TARGET): $(OBJS)
	@echo -e "=\033[0;36m Creating \033[0;0m $@"
	$(LD) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

# obj/xxx.o 對應來源（支援根目錄與子資料夾）
$(OBJS_DIR)/%.o:
	@echo -e "=\033[0;32m Compiling \033[0;0m $*"
	@mkdir -p $(OBJS_DIR)
	$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -c $(firstword $(filter $*.cpp %/$*.cpp,$(SRCS))) -o $@

# 自動載入 .d（第一次沒有也不報錯）
-include $(DEPS)

#-------------------------------------------------------------------------------
# Utilities
#-------------------------------------------------------------------------------
.PHONY: clean strip
clean:
	rm -f $(TARGET)
	rm -rf $(OBJS_DIR)

# 可選：縮小可執行檔體積
strip:
	strip $(TARGET)
