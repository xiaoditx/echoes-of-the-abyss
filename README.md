# 深渊回响 Echoes of the Abyss

**本项目为纯AI实验性产物，特此提醒**

Windows 横屏 2D 肉鸽动作游戏（垂直切片）。核心循环：主菜单 → 回声熔炉（meta 升级）→ 开局 → 三层战斗 + Boss → 死亡/胜利结算 → 重开。单局约 10~15 分钟。

## 运行

- 安装包：`dist/EchoesOfTheAbyss-Setup-0.1.0.exe`（Inno Setup，支持 Unicode 路径、开始菜单、桌面图标、卸载）。
- 绿色运行：直接执行 `build/release/EchoesOfTheAbyss.exe`（资源已平铺在 exe 旁：gfx/tables/rooms/sfx/bgm/strings_zh.csv）。
- 仅支持 Windows x64；无需安装任何运行库（C++ 运行时静态链接）。
- 输入：仅键鼠。
  - 移动 A/D 或 ←/→，跳跃 Space/W/↑，翻滚 Shift，攻击 J / 鼠标左键，喝药 Q，交互/传送门 E，暂停 Esc，菜单确认 Enter，菜单选择 ↑/↓。

## 日志与存档

- 日志：`%LOCALAPPDATA%\EchoesOfTheAbyss\logs\EchoesOfTheAbyss.log`（滚动保留，见 logger.cpp）。
- 崩溃日志：同目录 `crash-YYYYMMDD-HHMMSS.log`，含版本、时间、异常码、寄存器与最后状态。
- 存档：`%LOCALAPPDATA%\EchoesOfTheAbyss\saves\save.txt`（魂晶、升级等级、统计数据）。
- Debug 构建日志级别 Trace；Release 保留 Info 及以上与崩溃日志、关闭断言。

## 构建

需要 MinGW-w64（MSYS2 ucrt64）、make、Rust 工具链（仅借用 rust-lld）、Inno Setup 6、Python3（素材脚本）。

```bat
make debug     :: 构建 build\debug\（含资源拷贝）
make release   :: 构建 build\release\
make package   :: release + Inno Setup 安装包到 dist\
make clean     :: 清理 build/ dist/
make update    :: 强制更新 yumo audio 库到最新 Release
```

yumo audio 的二进制与头文件不在版本库中（`develop/` 已忽略）。首次构建时若 `develop/libs/audioPlayer_x64.a` 缺失，`make` 会调用根目录的 `download_audio_lib.ps1` 自动从 GitHub Release 下载、解压并用 `ar t` 校验；也可手动执行 `make update` 更新。

注意：本机的腾讯电脑管家实时防护会把 GNU ld(bfd) 输出的 PE 头清为零字节，导致 exe 无法启动；因此链接统一使用 lld（`tools/ld.lld.exe`，由 `make` 自动从本机 Rust 工具链复制 rust-lld，无第三方下载）。详见 `installer/../工单.md`。

## 音频

使用 yumo audio（`develop/libs/audioPlayer_x64.a`），WAV=SFX、MP3=BGM。yumo 的 `preloadAudio` 为异步；`addAudio(preloadId)` 在偶发竞态下抛 `InvalidData`，已在 `src/audio/audio.cpp` 中做逐帧重试处理。

## 许可

代码 WTFPL。素材（Kenney / HaelDB / cynicmusic，均 CC0）见 `assets/LICENSES.txt`。

## 设计文档

见 `design/`：core-loop.md、combat.md、level-gen.md、upgrades.md、ui-flow.md、story.md、art-direction.md、data-schema.md。
