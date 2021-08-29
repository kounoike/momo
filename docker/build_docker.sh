#!/bin/bash

set -e

cd $(dirname $0)

. ../VERSION

docker build \
  --build-arg VARIANT=ubuntu-18.04 \
  --build-arg WEBRTC_BUILD_VERSION=$WEBRTC_BUILD_VERSION \
  --build-arg BOOST_VERSION=$BOOST_VERSION \
  --build-arg CLI11_VERSION=$CLI11_VERSION \
  --build-arg SDL2_VERSION=$SDL2_VERSION \
  --build-arg CMAKE_VERSION=$CMAKE_VERSION \
  --build-arg CUDA_VERSION=$CUDA_VERSION \
  --build-arg http_proxy=$http_proxy \
  --build-arg https_proxy=$https_proxy \
  . --target=host_momo_builder -t momo_builder:host-18.04

docker build \
  --build-arg VARIANT=ubuntu-18.04 \
  --build-arg WEBRTC_BUILD_VERSION=$WEBRTC_BUILD_VERSION \
  --build-arg BOOST_VERSION=$BOOST_VERSION \
  --build-arg CLI11_VERSION=$CLI11_VERSION \
  --build-arg SDL2_VERSION=$SDL2_VERSION \
  --build-arg CMAKE_VERSION=$CMAKE_VERSION \
  --build-arg CUDA_VERSION=$CUDA_VERSION \
  --build-arg http_proxy=$http_proxy \
  --build-arg https_proxy=$https_proxy \
  . --target=cross_momo_builder -t momo_builder:cross-18.04

docker build \
  --build-arg VARIANT=ubuntu-20.04 \
  --build-arg WEBRTC_BUILD_VERSION=$WEBRTC_BUILD_VERSION \
  --build-arg BOOST_VERSION=$BOOST_VERSION \
  --build-arg CLI11_VERSION=$CLI11_VERSION \
  --build-arg SDL2_VERSION=$SDL2_VERSION \
  --build-arg CMAKE_VERSION=$CMAKE_VERSION \
  --build-arg CUDA_VERSION=$CUDA_VERSION \
  --build-arg http_proxy=$http_proxy \
  --build-arg https_proxy=$https_proxy \
  . --target=host_momo_builder -t momo_builder:host-20.04
