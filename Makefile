# Makefile - Echoes of the Abyss (Windows x64, MinGW-w64, Unicode only)
# Targets: debug / release / clean / package / run
# Note: recipes use cmd.exe builtins (no POSIX shell on PATH).

CXX      := g++
WINDRES  := windres

SRC_DIR  := src
LIB_DIR  := develop/libs
ASSETS   := assets
RES_RC   := installer/resources.rc

# ---- audio library (fetched per develop/libs/docs/make_get.md) --------------
AUDIO_LIB_REPO := keybonk-org/audio-player
AUDIO_LIB      := $(LIB_DIR)/audioPlayer_x64.a

# ---- flags -----------------------------------------------------------------
COMMON_FLAGS := -std=c++17 -DUNICODE -D_UNICODE -Wall -Wextra -Wno-unused-parameter \
                -MMD -MP -I$(SRC_DIR) -I$(LIB_DIR)
DBG_FLAGS  := -O0 -g -DEOTA_DEBUG
REL_FLAGS  := -O2 -DNDEBUG

LD_COMMON  := -Btools -fuse-ld=lld -mwindows -municode -static -static-libgcc -static-libstdc++
LIBS       := $(AUDIO_LIB) \
              -lmsacm32 -ld2d1 -ldwrite -lwindowscodecs -lole32 -luuid -lwinmm

# ---- sources (explicit list) ------------------------------------------------
SRCS := \
  src/main.cpp \
  src/app/app.cpp \
  src/audio/audio.cpp \
  src/core/crash.cpp \
  src/core/logger.cpp \
  src/core/paths.cpp \
  src/game/enemies.cpp \
  src/game/fx.cpp \
  src/game/hud.cpp \
  src/game/items.cpp \
  src/game/level.cpp \
  src/game/physics.cpp \
  src/game/player.cpp \
  src/game/save.cpp \
  src/game/strings.cpp \
  src/game/tables.cpp \
  src/input/input.cpp \
  src/render/renderer.cpp \
  src/render/sprites.cpp \
  src/render/texture.cpp \
  src/ui/menu.cpp

OBJS_DBG := $(patsubst src/%.cpp,build/debug/obj/%.o,$(SRCS))
OBJS_REL := $(patsubst src/%.cpp,build/release/obj/%.o,$(SRCS))

BIN_DBG := build/debug/EchoesOfTheAbyss.exe
BIN_REL := build/release/EchoesOfTheAbyss.exe

RES_DBG := build/debug/obj/resources.o
RES_REL := build/release/obj/resources.o

# cmd helpers
MKDIR = if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
XCOPY_DIR = xcopy /E /I /Y /Q
COPY_FILE = copy /Y

# ---- lld toolchain shim ------------------------------------------------------
# GNU ld (bfd) output gets its PE header scrubbed by the local real-time
# protection software on this machine; lld output does not. rust-lld is
# reused from the installed Rust toolchain (no third-party download).
RUST_LLD := $(USERPROFILE)/.rustup/toolchains/stable-x86_64-pc-windows-gnu/lib/rustlib/x86_64-pc-windows-gnu/bin/rust-lld.exe
tools/ld.lld.exe:
	@if not exist tools mkdir tools
	$(COPY_FILE) "$(RUST_LLD)" "tools\ld.lld.exe" >nul

.PHONY: all debug release clean package run assets-dbg assets-rel update

all: release

# ---- yumo audio library (only fetched when missing) -------------------------
$(AUDIO_LIB):
	@echo 正在下载 audio-player 库 ...
	@powershell -NoProfile -ExecutionPolicy Bypass -File download_audio_lib.ps1 -Repo "$(AUDIO_LIB_REPO)" -Arch "x64" -DownloadDir "$(LIB_DIR)" -TargetFile "$(AUDIO_LIB)"
	@echo 库已就绪。

# Force an update to the latest release.
update:
	@echo 正在更新 audio-player 库 ...
	@powershell -NoProfile -ExecutionPolicy Bypass -File download_audio_lib.ps1 -Repo "$(AUDIO_LIB_REPO)" -Arch "x64" -DownloadDir "$(LIB_DIR)" -TargetFile "$(AUDIO_LIB)"
	@echo 库已就绪。

# ---- debug -----------------------------------------------------------------
debug: $(BIN_DBG) assets-dbg

$(BIN_DBG): $(OBJS_DBG) $(RES_DBG) tools/ld.lld.exe $(AUDIO_LIB)
	$(CXX) $(OBJS_DBG) $(RES_DBG) $(LD_COMMON) -o $@ $(LIBS)

build/debug/obj/%.o: src/%.cpp
	@$(MKDIR)
	$(CXX) $(COMMON_FLAGS) $(DBG_FLAGS) -c $< -o $@

$(RES_DBG): $(RES_RC)
	@$(MKDIR)
	$(WINDRES) -DUNICODE -D_UNICODE -i $(RES_RC) -o $@

assets-dbg: $(BIN_DBG)
	@$(XCOPY_DIR) "$(ASSETS)\gfx"    "build\debug\gfx\"
	@$(XCOPY_DIR) "$(ASSETS)\tables" "build\debug\tables\"
	@$(XCOPY_DIR) "$(ASSETS)\rooms"  "build\debug\rooms\"
	@$(XCOPY_DIR) "$(ASSETS)\sfx"    "build\debug\sfx\"
	@$(XCOPY_DIR) "$(ASSETS)\bgm"    "build\debug\bgm\"
	@$(COPY_FILE) "$(ASSETS)\strings_zh.csv" "build\debug\" >nul
	@$(COPY_FILE) "$(ASSETS)\LICENSES.txt"   "build\debug\" >nul

# ---- release ---------------------------------------------------------------
release: $(BIN_REL) assets-rel

$(BIN_REL): $(OBJS_REL) $(RES_REL) tools/ld.lld.exe $(AUDIO_LIB)
	$(CXX) $(OBJS_REL) $(RES_REL) $(LD_COMMON) -o $@ $(LIBS)

build/release/obj/%.o: src/%.cpp
	@$(MKDIR)
	$(CXX) $(COMMON_FLAGS) $(REL_FLAGS) -c $< -o $@

$(RES_REL): $(RES_RC)
	@$(MKDIR)
	$(WINDRES) -DUNICODE -D_UNICODE -i $(RES_RC) -o $@

assets-rel: $(BIN_REL)
	@$(XCOPY_DIR) "$(ASSETS)\gfx"    "build\release\gfx\"
	@$(XCOPY_DIR) "$(ASSETS)\tables" "build\release\tables\"
	@$(XCOPY_DIR) "$(ASSETS)\rooms"  "build\release\rooms\"
	@$(XCOPY_DIR) "$(ASSETS)\sfx"    "build\release\sfx\"
	@$(XCOPY_DIR) "$(ASSETS)\bgm"    "build\release\bgm\"
	@$(COPY_FILE) "$(ASSETS)\strings_zh.csv" "build\release\" >nul
	@$(COPY_FILE) "$(ASSETS)\LICENSES.txt"   "build\release\" >nul

# ---- run / clean / package -------------------------------------------------
-include $(OBJS_DBG:.o=.d) $(OBJS_REL:.o=.d)

run: debug
	"$(subst /,\,$(BIN_DBG))"

clean:
	-if exist build rmdir /S /Q build
	-if exist dist  rmdir /S /Q dist

package: release
	powershell -NoProfile -ExecutionPolicy Bypass -File installer/package.ps1
