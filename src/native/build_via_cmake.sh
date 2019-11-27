#!/bin/bash
if [ ! -d ${ANDROID_SDK_ROOT} -o ! -d ${ANDROID_NDK_ROOT} ]; then
  echo '# Please check the env ANDROID_SDK_ROOT & ANDROID_NDK_ROOT before run this script:
  # ANDROID_SDK_ROOT="${HOME}/Android/Sdk" # macOS
  # ANDROID_NDK_ROOT="${ANDROID_SDK_ROOT}/ndk-bundle" # deps'
  exit -1
fi
WORKSPACE="$(cd `dirname $0`;pwd)"
WORKSPACE="${WORKSPACE:-$(pwd)}" # double check
XCRASH_BUILD_DIRECTORY="${WORKSPACE}/build"
ANDROID_CMAKE_TOOLCHAIN_FILE="${ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake"
XCRASH_OUTPUT_DIRECTORY="${WORKSPACE/src\/native/src\/java\/xcrash\/xcrash_lib\/src\/main\/jniLibs}"

for abi in armeabi-v7a arm64-v8a x86 x86_64
do
  rm -rf ${XCRASH_BUILD_DIRECTORY}
  mkdir -p ${XCRASH_BUILD_DIRECTORY}
  pushd ${XCRASH_BUILD_DIRECTORY}
  EXEC_CMD="cmake ${WORKSPACE} \
    -DCMAKE_TOOLCHAIN_FILE=\"${ANDROID_CMAKE_TOOLCHAIN_FILE}\" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DANDROID_TOOLCHAIN=clang \
    -DANDROID_NATIVE_API_LEVEL=23 \
    -DANDROID_ABI=${abi} \
    -DANDROID_NDK=\"${ANDROID_NDK_ROOT}\" \
    -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=\"${XCRASH_OUTPUT_DIRECTORY}/${abi}\""
  echo "${EXEC_CMD[@]}"
  eval "${EXEC_CMD[@]}"
  make 
  popd
done
