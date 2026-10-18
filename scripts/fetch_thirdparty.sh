#!/usr/bin/env bash
# Re-download vendored dependencies (ImGui + FastNoiseLite).
# GitHub's git/raw endpoints are blocked from some environments, codeload tarballs work.
set -euo pipefail

cd "$(dirname "$0")/.."
mkdir -p third_party
cd third_party

IMGUI_TAG="${IMGUI_TAG:-v1.92.4}"

if [ ! -d imgui ]; then
    echo "fetching Dear ImGui ${IMGUI_TAG}"
    curl -sSL "https://codeload.github.com/ocornut/imgui/tar.gz/refs/tags/${IMGUI_TAG}" -o imgui.tgz
    tar xzf imgui.tgz
    mv "imgui-${IMGUI_TAG#v}" imgui
    rm imgui.tgz
fi

if [ ! -f FastNoiseLite.h ]; then
    echo "fetching FastNoiseLite"
    curl -sSL "https://codeload.github.com/Auburn/FastNoiseLite/tar.gz/refs/heads/master" -o fnl.tgz
    tar xzf fnl.tgz
    cp FastNoiseLite-master/Cpp/FastNoiseLite.h .
    rm -rf FastNoiseLite-master fnl.tgz
fi

echo "third_party ready"
