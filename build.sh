#!/usr/bin/env bash
# =====================================================================
# SNESticle Revive 一键编译脚本 (Linux / WSL / Docker 容器内通用)
#
# 用法:
#   ./build.sh                    # 构建 ISO (默认, 含 ELF + 打包)
#   ./build.sh --elf              # 只构建 ELF (不制作 ISO)
#   ./build.sh --clean            # 构建前先清理 build/ 目录
#   ./build.sh --roms /路径       # 把 ROM 目录打进 ISO (可选)
#   ./build.sh --out /路径        # 额外复制 ELF/ISO 到该目录 (可选)
#   ./build.sh --turbo            # 4 线程并行编译
#   ./build.sh --jobs N           # 自定义 N 线程
#   ./build.sh --no-install       # 缺依赖时只提示、不自动安装
#
# 说明:
#   - 首次运行会通过 Makefile 自动下载 ps2dev 交叉工具链 (~1GB) 到
#     ~/.local/ps2dev, 只下载一次, 之后增量编译很快。
#   - 产物: build/ 目录下的 SNESticle_Revive.elf / .iso (及 packed 版)。
# =====================================================================
set -euo pipefail
cd "$(dirname "$0")"

TARGET="iso"
CLEAN=0
JOBS=""
ROMS=""
OUT=""
AUTO_INSTALL="yes"

while [ $# -gt 0 ]; do
    case "$1" in
        --elf)        TARGET="elf" ;;
        --iso)        TARGET="iso" ;;
        --clean)      CLEAN=1 ;;
        --roms)       ROMS="${2:?--roms 需要路径参数}"; shift ;;
        --out)        OUT="${2:?--out 需要路径参数}"; shift ;;
        --turbo)      JOBS="4" ;;
        --serial)     JOBS="1" ;;
        --jobs)       JOBS="${2:?--jobs 需要数字参数}"; shift ;;
        --no-install) AUTO_INSTALL="no" ;;
        -h|--help)
            grep -E '^#   (\.\/|\s+-)' "$0" | sed 's/^#//'
            exit 0 ;;
        *)
            echo "未知参数: $1 (用 --help 查看用法)" >&2
            exit 1 ;;
    esac
    shift
done

# ---------------------------------------------------------------------
# 1. 检查宿主依赖 (make/wget/tar + ISO 工具)
#    ps2dev 工具链和 xorriso 的安装由 Makefile 的 AUTO_INSTALL 机制处理,
#    这里只负责 make 启动之前就必须存在的东西。
# ---------------------------------------------------------------------
MISSING=()
for t in make wget tar; do
    command -v "$t" >/dev/null 2>&1 || MISSING+=("$t")
done
if ! command -v mkisofs >/dev/null 2>&1 \
   && ! command -v genisoimage >/dev/null 2>&1 \
   && ! command -v xorriso >/dev/null 2>&1; then
    MISSING+=("xorriso")
fi

if [ "${#MISSING[@]}" -gt 0 ]; then
    echo "[SETUP] 缺少宿主工具: ${MISSING[*]}"
    if [ "$AUTO_INSTALL" = "yes" ] && command -v apt-get >/dev/null 2>&1; then
        SUDO=""
        [ "$(id -u)" != "0" ] && SUDO="sudo"
        echo "[SETUP] 尝试通过 apt-get 安装 (需要网络)..."
        $SUDO apt-get update
        $SUDO apt-get install -y build-essential wget ca-certificates xorriso
    else
        echo "[SETUP] 请手动安装后重试:"
        echo "        Debian/Ubuntu/WSL: sudo apt-get install build-essential wget xorriso"
        exit 1
    fi
fi

# ---------------------------------------------------------------------
# 2. 编译
# ---------------------------------------------------------------------
[ "$CLEAN" = "1" ] && { echo "[BUILD] 清理旧产物..."; make clean; }

MAKE_ARGS=(AUTO_INSTALL="$AUTO_INSTALL")
[ -n "$JOBS" ] && MAKE_ARGS+=("JOBS=$JOBS")
[ -n "$ROMS" ] && MAKE_ARGS+=("ROMS=$ROMS")
[ -n "$OUT" ]  && MAKE_ARGS+=("OUT=$OUT")

echo "[BUILD] make ${MAKE_ARGS[*]} $TARGET"
make "${MAKE_ARGS[@]}" "$TARGET"

# ---------------------------------------------------------------------
# 3. 结果提示
# ---------------------------------------------------------------------
echo
echo "================================================================="
echo "[OK] 编译完成!"
if [ "$TARGET" = "iso" ]; then
    echo "  ISO : build/SNESticle_Revive.iso"
fi
echo "  ELF : build/SNESticle_Revive.elf (未压缩) / .packed.elf (压缩)"
if [ -n "$OUT" ]; then
    echo "  副本已复制到: $OUT"
fi
echo "================================================================="
