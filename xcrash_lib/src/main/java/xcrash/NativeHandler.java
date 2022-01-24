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

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Build;
import android.text.TextUtils;
import android.util.Log;

import java.io.File;
import java.util.Map;

@SuppressLint("StaticFieldLeak")
class NativeHandler {

    private static final String TAG = "xcrash";
    private static final NativeHandler instance = new NativeHandler();
    private long anrTimeoutMs = 25 * 1000;

    private Context ctx;
    private boolean crashRethrow;
    private ICrashCallback crashCallback;
    private boolean anrEnable;
    private boolean anrCheckProcessState;
    private ICrashCallback anrCallback;
    private ICrashCallback anrFastCallback;

    private boolean initNativeLibOk = false;

    private NativeHandler() {
    }

    static NativeHandler getInstance() {
        return instance;
    }

    int initialize(Context ctx,
                   ILibLoader libLoader,
                   String appId,
                   String appVersion,
                   String logDir,
                   boolean crashEnable,
                   boolean crashRethrow,
                   int crashLogcatSystemLines,
                   int crashLogcatEventsLines,
                   int crashLogcatMainLines,
                   boolean crashDumpElfHash,
                   boolean crashDumpMap,
                   boolean crashDumpFds,
                   boolean crashDumpNetworkInfo,
                   boolean crashDumpAllThreads,
                   int crashDumpAllThreadsCountMax,
                   String[] crashDumpAllThreadsWhiteList,
                   ICrashCallback crashCallback,
                   boolean anrEnable,
                   boolean anrRethrow,
                   boolean anrCheckProcessState,
                   int anrLogcatSystemLines,
                   int anrLogcatEventsLines,
                   int anrLogcatMainLines,
                   boolean anrDumpFds,
                   boolean anrDumpNetworkInfo,
                   ICrashCallback anrCallback,
                   ICrashCallback anrFastCallback) {
        //load lib
        if (libLoader == null) {
            try {
                System.loadLibrary("xcrash");
            } catch (Throwable e) {
                XCrash.getLogger().e(Util.TAG, "NativeHandler System.loadLibrary failed", e);
                return Errno.LOAD_LIBRARY_FAILED;
            }
        } else {
            try {
                libLoader.loadLibrary("xcrash");
            } catch (Throwable e) {
                XCrash.getLogger().e(Util.TAG, "NativeHandler ILibLoader.loadLibrary failed", e);
                return Errno.LOAD_LIBRARY_FAILED;
            }
        }

        this.ctx = ctx;
        this.crashRethrow = crashRethrow;
        this.crashCallback = crashCallback;
        this.anrEnable = anrEnable;
        this.anrCheckProcessState = anrCheckProcessState;
        this.anrCallback = anrCallback;
        this.anrFastCallback = anrFastCallback;
        this.anrTimeoutMs = anrRethrow ? 25 * 1000 : 45 * 1000; //setting rethrow to "false" is NOT recommended

        //init native lib
        try {
            int r = nativeInit(
                Build.VERSION.SDK_INT,
                Build.VERSION.RELEASE,
                Util.getAbiList(),
                Build.MANUFACTURER,
                Build.BRAND,
                Util.getMobileModel(),
                Build.FINGERPRINT,
                appId,
                appVersion,
                ctx.getApplicationInfo().nativeLibraryDir,
                logDir,
                crashEnable,
                crashRethrow,
                crashLogcatSystemLines,
                crashLogcatEventsLines,
                crashLogcatMainLines,
                crashDumpElfHash,
                crashDumpMap,
                crashDumpFds,
                crashDumpNetworkInfo,
                crashDumpAllThreads,
                crashDumpAllThreadsCountMax,
                crashDumpAllThreadsWhiteList,
                anrEnable,
                anrRethrow,
                anrLogcatSystemLines,
                anrLogcatEventsLines,
                anrLogcatMainLines,
                anrDumpFds,
                anrDumpNetworkInfo);
            if (r != 0) {
                XCrash.getLogger().e(Util.TAG, "NativeHandler init failed");
                return Errno.INIT_LIBRARY_FAILED;
            }
            initNativeLibOk = true;
            return 0; //OK
        } catch (Throwable e) {
            XCrash.getLogger().e(Util.TAG, "NativeHandler init failed", e);
            return Errno.INIT_LIBRARY_FAILED;
        }
    }

    void notifyJavaCrashed() {
        if (initNativeLibOk && anrEnable) {
            NativeHandler.nativeNotifyJavaCrashed();
        }
    }

    void testNativeCrash(boolean runInNewThread) {
        if (initNativeLibOk) {
            NativeHandler.nativeTestCrash(runInNewThread ? 1 : 0);
        }
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
            XCrash.getLogger().e(Util.TAG, "NativeHandler getStacktraceByThreadName failed", e);
        }
        return null;
    }

    // do NOT obfuscate this method
    @SuppressWarnings("unused")
    private static void crashCallback(String logPath, String emergency, boolean dumpJavaStacktrace, boolean isMainThread, String threadName) {

        if (!TextUtils.isEmpty(logPath)) {

            //append java stacktrace
            if (dumpJavaStacktrace) {
                String stacktrace = getStacktraceByThreadName(isMainThread, threadName);
                if (!TextUtils.isEmpty(stacktrace)) {
                    TombstoneManager.appendSection(logPath, "java stacktrace", stacktrace);
                }
            }

            //append memory info
            TombstoneManager.appendSection(logPath, "memory info", Util.getProcessMemoryInfo());

            //append background / foreground
            TombstoneManager.appendSection(logPath, "foreground", ActivityMonitor.getInstance().isApplicationForeground() ? "yes" : "no");
        }

        ICrashCallback callback = NativeHandler.getInstance().crashCallback;
        if (callback != null) {
            try {
                callback.onCrash(logPath, emergency);
            } catch (Exception e) {
                XCrash.getLogger().w(Util.TAG, "NativeHandler native crash callback.onCrash failed", e);
            }
        }

        if (!NativeHandler.getInstance().crashRethrow) {
            ActivityMonitor.getInstance().finishAllActivities();
        }
    }

    // do NOT obfuscate this method
    @SuppressWarnings("unused")
    private static void traceCallbackBeforeDump() {
        Log.i(TAG, "trace fast callback time: " + System.currentTimeMillis());
        ICrashCallback anrFastCallback = NativeHandler.getInstance().anrFastCallback;
        if (anrFastCallback != null) {
            try {
                anrFastCallback.onCrash(null, null);
            } catch (Exception e) {
                XCrash.getLogger().w(Util.TAG, "NativeHandler ANR callback.onCrash failed", e);
            }
        }
    }

    // do NOT obfuscate this method
    @SuppressWarnings("unused")
    private static void traceCallback(String logPath, String emergency) {
        Log.i(TAG, "trace slow callback time: " + System.currentTimeMillis());

        if (TextUtils.isEmpty(logPath)) {
            return;
        }

        //append memory info
        TombstoneManager.appendSection(logPath, "memory info", Util.getProcessMemoryInfo());

        //append background / foreground
        TombstoneManager.appendSection(logPath, "foreground", ActivityMonitor.getInstance().isApplicationForeground() ? "yes" : "no");

        //check process ANR state
        if (NativeHandler.getInstance().anrCheckProcessState) {
            if (!Util.checkProcessAnrState(NativeHandler.getInstance().ctx, NativeHandler.getInstance().anrTimeoutMs)) {
                FileManager.getInstance().recycleLogFile(new File(logPath));
                return; //not an ANR
            }
        }

        //delete extra ANR log files
        if (!FileManager.getInstance().maintainAnr()) {
            return;
        }

        //rename trace log file to ANR log file
        String anrLogPath = logPath.substring(0, logPath.length() - Util.traceLogSuffix.length()) + Util.anrLogSuffix;
        File traceFile = new File(logPath);
        File anrFile = new File(anrLogPath);
        if (!traceFile.renameTo(anrFile)) {
            FileManager.getInstance().recycleLogFile(traceFile);
            return;
        }

        ICrashCallback callback = NativeHandler.getInstance().anrCallback;
        if (callback != null) {
            try {
                callback.onCrash(anrLogPath, emergency);
            } catch (Exception e) {
                XCrash.getLogger().w(Util.TAG, "NativeHandler ANR callback.onCrash failed", e);
            }
        }
    }

    private static native int nativeInit(
            int apiLevel,
            String osVersion,
            String abiList,
            String manufacturer,
            String brand,
            String model,
            String buildFingerprint,
            String appId,
            String appVersion,
            String appLibDir,
            String logDir,
            boolean crashEnable,
            boolean crashRethrow,
            int crashLogcatSystemLines,
            int crashLogcatEventsLines,
            int crashLogcatMainLines,
            boolean crashDumpElfHash,
            boolean crashDumpMap,
            boolean crashDumpFds,
            boolean crashDumpNetworkInfo,
            boolean crashDumpAllThreads,
            int crashDumpAllThreadsCountMax,
            String[] crashDumpAllThreadsWhiteList,
            boolean traceEnable,
            boolean traceRethrow,
            int traceLogcatSystemLines,
            int traceLogcatEventsLines,
            int traceLogcatMainLines,
            boolean traceDumpFds,
            boolean traceDumpNetworkInfo);

    private static native void nativeNotifyJavaCrashed();

    private static native void nativeTestCrash(int runInNewThread);
}
