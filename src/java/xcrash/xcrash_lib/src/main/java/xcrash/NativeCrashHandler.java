// Copyright (c) 2019-present, iQIYI, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// Created by caikelun on 2019-03-07.
package xcrash;

import android.content.Context;
import android.text.TextUtils;

import java.util.Map;

class NativeCrashHandler {

    private static final NativeCrashHandler instance = new NativeCrashHandler();

    private ICrashCallback callback = null;

    private NativeCrashHandler() {
    }

    static NativeCrashHandler getInstance() {
        return instance;
    }

    int initialize(Context ctx, String appId, String appVersion, String logDir, boolean rethrow,
                   int logcatSystemLines, int logcatEventsLines, int logcatMainLines,
                   boolean dumpElfHash, boolean dumpMap, boolean dumpFds, boolean dumpAllThreads,
                   int dumpAllThreadsCountMax, String[] dumpAllThreadsWhiteList,
                   ICrashCallback callback, ILibLoader libLoader) {
        //load lib
        if (libLoader == null) {
            try {
                System.loadLibrary("xcrash");
            } catch (Throwable e) {
                XCrash.getLogger().e(Util.TAG, "NativeCrashHandler System.loadLibrary failed", e);
                return Errno.LOAD_LIBRARY_FAILED;
            }
        } else {
            try {
                libLoader.loadLibrary("xcrash");
            } catch (Throwable e) {
                XCrash.getLogger().e(Util.TAG, "NativeCrashHandler ILibLoader.loadLibrary failed", e);
                return Errno.LOAD_LIBRARY_FAILED;
            }
        }

        this.callback = callback;

        //init native lib
        try {
            int r = init(rethrow,
                    appId,
                    appVersion,
                    ctx.getApplicationInfo().nativeLibraryDir,
                    logDir,
                    logcatSystemLines,
                    logcatEventsLines,
                    logcatMainLines,
                    dumpElfHash,
                    dumpMap,
                    dumpFds,
                    dumpAllThreads,
                    dumpAllThreadsCountMax,
                    dumpAllThreadsWhiteList);
            if (r != 0) {
                XCrash.getLogger().e(Util.TAG, "NativeCrashHandler init failed");
                return Errno.INIT_LIBRARY_FAILED;
            }
            return 0; //OK
        } catch (Throwable e) {
            XCrash.getLogger().e(Util.TAG, "NativeCrashHandler init failed", e);
            return Errno.INIT_LIBRARY_FAILED;
        }
    }

    void testNativeCrash(boolean runInNewThread) {
        NativeCrashHandler.test(runInNewThread ? 1 : 0);
    }

    private static String getStacktraceByThreadName(boolean isMainThread, String threadName) {
        try {
            for (Map.Entry<Thread, StackTraceElement[]> entry : Thread.getAllStackTraces().entrySet()) {
                Thread thd = entry.getKey();
                if ((isMainThread && thd.getName().equals("main")) || (!isMainThread && thd.getName().contains(threadName))) {
                    StringBuilder sb = new StringBuilder();
                    for (StackTraceElement element : entry.getValue()) {
                        sb.append("    at ").append(element.toString()).append("\n");
                    }
                    return sb.toString();
                }
            }
        } catch (Exception e) {
            XCrash.getLogger().e(Util.TAG, "NativeCrashHandler getStacktraceByThreadName failed", e);
        }
        return null;
    }

    // do NOT obfuscate this method
    @SuppressWarnings("unused")
    private static void callback(String logPath, String emergency, boolean isJavaThread, boolean isMainThread, String threadName) {
        if (!TextUtils.isEmpty(logPath)) {

            //append java stacktrace
            if (isJavaThread) {
                String stacktrace = getStacktraceByThreadName(isMainThread, threadName);
                if (!TextUtils.isEmpty(stacktrace)) {
                    TombstoneManager.appendSection(logPath, "java stacktrace", stacktrace);
                }
            }

            //append memory info
            TombstoneManager.appendSection(logPath, "memory info", Util.getProcessMemoryInfo());
        }

        ICrashCallback callback = NativeCrashHandler.getInstance().callback;
        if (callback != null) {
            try {
                callback.onCrash(logPath, emergency);
            } catch (Exception e) {
                XCrash.getLogger().w(Util.TAG, "NativeCrashHandler callback.onCrash failed", e);
            }
        }
    }

    private static native int init(
            boolean restoreSignalHandler,
            String appId,
            String appVersion,
            String appLibDir,
            String logDir,
            int logcatSystemLines,
            int logcatEventsLines,
            int logcatMainLines,
            boolean dumpElfHash,
            boolean dumpMap,
            boolean dumpFds,
            boolean dumpAllThreads,
            int dumpAllThreadsCountMax,
            String[] dumpAllThreadsWhiteList);

    private static native void test(int runInNewThread);
}
