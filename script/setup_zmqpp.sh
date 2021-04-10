#!/bin/bash

# 指定した zmqpp のバージョンをダウンロードして、make を実行できる状態にする
#
# ソースコードは $2/source に配置される
#
# 引数:
#   $1: zmqpp のバージョン
#   $2: 出力ディレクトリ
if [ $# -lt 2 ]; then
  echo "$0 <zmqpp_version> <output_dir>"
  exit 1
fi

ZMQPP_VERSION=$1
OUTPUT_DIR=$2

mkdir -p $OUTPUT_DIR
pushd $OUTPUT_DIR
  if [ ! -e $ZMQPP_VERSION.tar.gz ]; then
    curl -LO https://github.com/zeromq/zmqpp/archive/refs/tags/$ZMQPP_VERSION.tar.gz
  fi
  rm -rf source
  rm -rf zmqpp-$ZMQPP_VERSION
  tar xf $ZMQPP_VERSION.tar.gz
  mv zmqpp-$ZMQPP_VERSION source
popd
