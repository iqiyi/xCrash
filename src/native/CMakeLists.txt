CMAKE_MINIMUM_REQUIRED(VERSION 3.6)

PROJECT(xcrash_native)

FIND_LIBRARY(android-dl dl)
FIND_LIBRARY(android-log log)
ADD_SUBDIRECTORY(common)
ADD_SUBDIRECTORY(libxcrash/jni)
ADD_SUBDIRECTORY(libxcrash_dumper/jni)
