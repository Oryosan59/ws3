# コンパイラとフラグ
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -pedantic -g # デバッグ情報を含める

# --- ディレクトリ定義 ---
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
INC_DIR = include # プロジェクト自身のインクルードディレクトリ

# --- 外部ライブラリパス ---
# コマンドで指定された特定のパスを使用
NAVIGATOR_LIB_PATH = /home/pi/navigator-lib/target/debug

# --- GStreamer API のためのフラグとライブラリ ---
# pkg-config を使用して GStreamer のコンパイルフラグとリンクライブラリを取得
GSTREAMER_CFLAGS = $(shell pkg-config --cflags gstreamer-1.0)
GSTREAMER_LIBS = $(shell pkg-config --libs gstreamer-1.0)
CXXFLAGS += $(GSTREAMER_CFLAGS) # GStreamer のコンパイルフラグを追加

# pkg-configが成功したかチェック
ifeq ($(GSTREAMER_CFLAGS),)
    $(error "pkg-config could not find gstreamer-1.0. Make sure it is installed and PKG_CONFIG_PATH is set.")
endif

# --- インクルードディレクトリ ---
# プロジェクトのインクルードディレクトリと外部ライブラリのインクルードディレクトリを追加
INCLUDES = -I$(INC_DIR) -I$(NAVIGATOR_LIB_PATH)

# --- ライブラリディレクトリ ---
LDFLAGS = -L$(NAVIGATOR_LIB_PATH) \
          -Wl,-rpath=$(NAVIGATOR_LIB_PATH) # 実行時リンクのためのrpathを設定

# --- リンクするライブラリ ---
# コマンドで指定された特定のライブラリ名を使用
LIBS = -lbluerobotics_navigator -lpthread -lm
LIBS += $(GSTREAMER_LIBS) # GStreamer のリンクライブラリを追加

# --- ターゲット実行ファイル ---
TARGET_NAME = navigator_control
TARGET = $(BIN_DIR)/$(TARGET_NAME)

# --- ソースファイルとオブジェクトファイル ---
# SRC_DIR 内のすべての .cpp ファイルを検索
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
# ソースファイル名に基づいて OBJ_DIR 内のオブジェクトファイル名を生成
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

# --- デフォルトターゲット: 実行ファイルをビルド ---
all: $(TARGET)

# --- 実行ファイルをリンクするルール ---
$(TARGET): $(OBJS) | $(BIN_DIR) # リンク前に BIN_DIR が存在することを確認
	$(CXX) $(LDFLAGS) $^ -o $@ $(LIBS)
	@echo "Build complete: $(TARGET)"

# --- ソースファイルをオブジェクトファイルにコンパイルするルール ---
# SRC_DIR の .cpp ファイルを OBJ_DIR の .o ファイルにコンパイル
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR) # コンパイル前に OBJ_DIR が存在することを確認
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# --- ディレクトリ作成 ---
# これらのターゲットは、ディレクトリが存在しない場合に作成します
# これらは、順序のみの依存関係 (|) を使用するコンパイルおよびリンクルールの前提条件です
$(OBJ_DIR):
	@mkdir -p $@

$(BIN_DIR):
	@mkdir -p $@

# --- ビルド成果物をクリーンアップするターゲット ---
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Cleaned."

# --- Phony ターゲット (ファイルを表さないターゲット) ---
.PHONY: all clean $(OBJ_DIR) $(BIN_DIR)

# --- 中間ファイルが削除されるのを防ぐ ---
.SECONDARY: $(OBJS)