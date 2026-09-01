#!/bin/sh
# Alpine(ps2dev 镜像)容器内构建包装器:
# 1. 安装宿主构建工具 (镜像只带交叉工具链, 没有 make/gcc/git 等)
# 2. 调用 build.sh, 参数透传 (如 --clean --turbo)
set -e
echo "[SETUP] 正在安装容器内宿主工具 (首次约 1-2 分钟)..."
if ! apk add --no-cache bash make gcc g++ musl-dev git xorriso zlib-dev >/tmp/apk.log 2>&1; then
    cat /tmp/apk.log
    exit 1
fi
echo "[SETUP] 宿主工具安装完成:"
for t in bash make gcc git xorriso; do
    echo "  $(command -v "$t")"
done
cd /src
exec bash build.sh "$@"
