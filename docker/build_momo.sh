#!/bin/bash

set -e

# ヘルプ表示
function show_help() {
  echo ""
  echo "$0 <パッケージ名>"
  echo ""
}

# 引数のチェック
if [ $# -ne 1 ]; then
  show_help
  exit 1
fi

PACKAGE_NAME="$1"
WORKSPACE_DIR=$(cd $(dirname $0)/..; pwd)

source /tool/webrtc/host/VERSIONS
mkdir -p ${WORKSPACE_DIR}/_build/$PACKAGE_NAME
pushd ${WORKSPACE_DIR}/_build/$PACKAGE_NAME
  cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DMOMO_PACKAGE_NAME=$PACKAGE_NAME \
      -DMOMO_VERSION=$MOMO_VERSION \
      -DMOMO_COMMIT=$MOMO_COMMIT \
      -DWEBRTC_BUILD_VERSION=$WEBRTC_BUILD_VERSION \
      -DWEBRTC_READABLE_VERSION=$WEBRTC_READABLE_VERSION \
      -DWEBRTC_COMMIT=$WEBRTC_COMMIT \
      ../..
  if [ -n "$VERBOSE" ]; then
      export VERBOSE=$VERBOSE
  fi
  cmake --build . -j$(nproc)
popd
