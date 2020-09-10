#!/bin/bash

mkdir -p ../java/xcrash/xcrash_lib/src/main/jniLibs/armeabi
mkdir -p ../java/xcrash/xcrash_lib/src/main/jniLibs/armeabi-v7a
mkdir -p ../java/xcrash/xcrash_lib/src/main/jniLibs/arm64-v8a
mkdir -p ../java/xcrash/xcrash_lib/src/main/jniLibs/x86
mkdir -p ../java/xcrash/xcrash_lib/src/main/jniLibs/x86_64

#cp -f ./libxcrash/libs/armeabi/libxcrash.so     ../java/xcrash/xcrash_lib/src/main/jniLibs/armeabi/libxcrash.so
cp -f ./libxcrash/libs/armeabi-v7a/libxcrash.so ../java/xcrash/xcrash_lib/src/main/jniLibs/armeabi-v7a/libxcrash.so
cp -f ./libxcrash/libs/arm64-v8a/libxcrash.so   ../java/xcrash/xcrash_lib/src/main/jniLibs/arm64-v8a/libxcrash.so
cp -f ./libxcrash/libs/x86/libxcrash.so         ../java/xcrash/xcrash_lib/src/main/jniLibs/x86/libxcrash.so
cp -f ./libxcrash/libs/x86_64/libxcrash.so      ../java/xcrash/xcrash_lib/src/main/jniLibs/x86_64/libxcrash.so

#cp -f ./libxcrash_dumper/libs/armeabi/xcrash_dumper     ../java/xcrash/xcrash_lib/src/main/jniLibs/armeabi/libxcrash_dumper.so
cp -f ./libxcrash_dumper/libs/armeabi-v7a/xcrash_dumper ../java/xcrash/xcrash_lib/src/main/jniLibs/armeabi-v7a/libxcrash_dumper.so
cp -f ./libxcrash_dumper/libs/arm64-v8a/xcrash_dumper   ../java/xcrash/xcrash_lib/src/main/jniLibs/arm64-v8a/libxcrash_dumper.so
cp -f ./libxcrash_dumper/libs/x86/xcrash_dumper         ../java/xcrash/xcrash_lib/src/main/jniLibs/x86/libxcrash_dumper.so
cp -f ./libxcrash_dumper/libs/x86_64/xcrash_dumper      ../java/xcrash/xcrash_lib/src/main/jniLibs/x86_64/libxcrash_dumper.so

version=`strings ./libxcrash_dumper/obj/local/armeabi-v7a/xcrash_dumper | grep 'Tombstone maker' | awk -F "\'" '{print $2}' | awk '{print $2}'`
folder=$version
tarfile=v${version}.tar.gz
rm -rf $folder $tarfile
mkdir -p ./$folder/armeabi-v7a
mkdir -p ./$folder/arm64-v8a


cp -f ./libxcrash/obj/local/armeabi-v7a/libxcrash.so ./$folder/armeabi-v7a/libxcrash.so
cp -f ./libxcrash/obj/local/arm64-v8a/libxcrash.so   ./$folder/arm64-v8a/libxcrash.so
cp -f ./libxcrash_dumper/obj/local/armeabi-v7a/xcrash_dumper ./$folder/armeabi-v7a/libxcrash_dumper.so
cp -f ./libxcrash_dumper/obj/local/arm64-v8a/xcrash_dumper   ./$folder/arm64-v8a/libxcrash_dumper.so

tar cvzf $tarfile $folder

