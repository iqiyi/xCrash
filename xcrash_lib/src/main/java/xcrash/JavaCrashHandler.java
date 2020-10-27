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

import java.io.File;
import java.io.PrintWriter;
import java.io.RandomAccessFile;
import java.io.StringWriter;
import java.lang.Thread.UncaughtExceptionHandler;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.regex.Pattern;

import android.annotation.SuppressLint;
import android.text.TextUtils;
import android.os.Process;

@SuppressLint("StaticFieldLeak")
class JavaCrashHandler implements UncaughtExceptionHandler {

    private static final JavaCrashHandler instance = new JavaCrashHandler();

    private final Date startTime = new Date();

    private int pid;
    private String processName;
    private String appId;
    private String appVersion;
    private boolean rethrow;
    private String logDir;
    private int logcatSystemLines;
    private int logcatEventsLines;
    private int logcatMainLines;
    private boolean dumpFds;
    private boolean dumpNetworkInfo;
    private boolean dumpAllThreads;
    private int dumpAllThreadsCountMax;
    private String[] dumpAllThreadsWhiteList;
    private ICrashCallback callback;
    private UncaughtExceptionHandler defaultHandler = null;

    private JavaCrashHandler() {
    }

    static JavaCrashHandler getInstance() {
        return instance;
    }

    void initialize(int pid, String processName, String appId, String appVersion, String logDir, boolean rethrow,
                    int logcatSystemLines, int logcatEventsLines, int logcatMainLines,
                    boolean dumpFds, boolean dumpNetworkInfo, boolean dumpAllThreads, int dumpAllThreadsCountMax, String[] dumpAllThreadsWhiteList,
                    ICrashCallback callback) {
        this.pid = pid;
        this.processName = (TextUtils.isEmpty(processName) ? "unknown" : processName);
        this.appId = appId;
        this.appVersion = appVersion;
        this.rethrow = rethrow;
        this.logDir = logDir;
        this.logcatSystemLines = logcatSystemLines;
        this.logcatEventsLines = logcatEventsLines;
        this.logcatMainLines = logcatMainLines;
        this.dumpFds = dumpFds;
        this.dumpNetworkInfo = dumpNetworkInfo;
        this.dumpAllThreads = dumpAllThreads;
        this.dumpAllThreadsCountMax = dumpAllThreadsCountMax;
        this.dumpAllThreadsWhiteList = dumpAllThreadsWhiteList;
        this.callback = callback;
        this.defaultHandler = Thread.getDefaultUncaughtExceptionHandler();

        try {
            Thread.setDefaultUncaughtExceptionHandler(this);
        } catch (Exception e) {
            XCrash.getLogger().e(Util.TAG, "JavaCrashHandler setDefaultUncaughtExceptionHandler failed", e);
        }
    }

    @Override
    public void uncaughtException(Thread thread, Throwable throwable) {
        if (defaultHandler != null) {
            Thread.setDefaultUncaughtExceptionHandler(defaultHandler);
        }

        try {
            handleException(thread, throwable);
        } catch (Exception e) {
            XCrash.getLogger().e(Util.TAG, "JavaCrashHandler handleException failed", e);
        }

        if (this.rethrow) {
            if (defaultHandler != null) {
                defaultHandler.uncaughtException(thread, throwable);
            }
        } else {
            ActivityMonitor.getInstance().finishAllActivities();
            Process.killProcess(this.pid);
            System.exit(10);
        }
    }

    private void handleException(Thread thread, Throwable throwable) {
        Date crashTime = new Date();

        //notify the java crash
        NativeHandler.getInstance().notifyJavaCrashed();
        AnrHandler.getInstance().notifyJavaCrashed();

        //create log file
        File logFile = null;
        try {
            String logPath = String.format(Locale.US, "%s/%s_%020d_%s__%s%s", logDir, Util.logPrefix, startTime.getTime() * 1000, appVersion, processName, Util.javaLogSuffix);
            logFile = FileManager.getInstance().createLogFile(logPath);
        } catch (Exception e) {
            XCrash.getLogger().e(Util.TAG, "JavaCrashHandler createLogFile failed", e);
        }

        //get emergency
        String emergency = null;
        try {
            emergency = getEmergency(crashTime, thread, throwable);
        } catch (Exception e) {
            XCrash.getLogger().e(Util.TAG, "JavaCrashHandler getEmergency failed", e);
        }

        //write info to log file
        if (logFile != null) {
            RandomAccessFile raf = null;
            try {
                raf = new RandomAccessFile(logFile, "rws");

                //write emergency info
                if (emergency != null) {
                    raf.write(emergency.getBytes("UTF-8"));
                }

                //If we wrote the emergency info successfully, we don't need to return it from callback again.
                emergency = null;

                //write logcat
                if (logcatMainLines > 0 || logcatSystemLines > 0 || logcatEventsLines > 0) {
                    raf.write(Util.getLogcat(logcatMainLines, logcatSystemLines, logcatEventsLines).getBytes("UTF-8"));
                }

                //write fds
                if (dumpFds) {
                    raf.write(Util.getFds().getBytes("UTF-8"));
                }

                //write network info
                if (dumpNetworkInfo) {
                    raf.write(Util.getNetworkInfo().getBytes("UTF-8"));
                }

                //write memory info
                raf.write(Util.getMemoryInfo().getBytes("UTF-8"));

                //write background / foreground
                raf.write(("foreground:\n" + (ActivityMonitor.getInstance().isApplicationForeground() ? "yes" : "no") + "\n\n").getBytes("UTF-8"));

                //write other threads info
                if (dumpAllThreads) {
                    raf.write(getOtherThreadsInfo(thread).getBytes("UTF-8"));
                }
            } catch (Exception e) {
                XCrash.getLogger().e(Util.TAG, "JavaCrashHandler write log file failed", e);
            } finally {
                if (raf != null) {
                    try {
                        raf.close();
                    } catch (Exception ignored) {
                    }
                }
            }
        }

        //callback
        if (callback != null) {
            try {
                callback.onCrash(logFile == null ? null : logFile.getAbsolutePath(), emergency);
            } catch (Exception ignored) {
            }
        }
    }

    private String getLibInfo(List<String> libPathList) {
        StringBuilder sb = new StringBuilder();
        for (String libPath : libPathList) {
            File libFile = new File(libPath);
            if (libFile.exists() && libFile.isFile()) {
                String md5 = Util.getFileMD5(libFile);

                DateFormat timeFormatter = new SimpleDateFormat(Util.timeFormatterStr, Locale.US);
                Date lastTime = new Date(libFile.lastModified());

                sb.append("    ").append(libPath).append("(BuildId: unknown. FileSize: ").append(libFile.length()).append(". LastModified: ")
                        .append(timeFormatter.format(lastTime)).append(". MD5: ").append(md5).append(")\n");
            } else {
                sb.append("    ").append(libPath).append(" (Not found)\n");
            }
        }

        String libInfo = sb.toString();
        return libInfo;
    }

    private String getBuildId(String stktrace) {
        String buildId = "";
        List<String> libPathList = new ArrayList<String>();
        if (stktrace.contains("UnsatisfiedLinkError")) {
            String libInfo = null;
            String[] tempLibPathStr;
            tempLibPathStr = stktrace.split("\""); // " is the delimiter
            for (String libPathStr :  tempLibPathStr) {
                if (libPathStr.isEmpty() || !libPathStr.endsWith(".so")) continue;
                libPathList.add(libPathStr);

                String libName = libPathStr.substring(libPathStr.lastIndexOf('/') + 1);

                libPathList.add(XCrash.nativeLibDir + "/" + libName);
                libPathList.add("/vendor/lib/" + libName);
                libPathList.add("/vendor/lib64/" + libName);
                libPathList.add("/system/lib/" + libName);
                libPathList.add("/system/lib64/" + libName);

                libInfo = getLibInfo(libPathList);
            }

            buildId = "build id:"
                    + "\n"
                    + libInfo
                    + "\n";
        }

        return buildId;
    }

    private String getEmergency(Date crashTime, Thread thread, Throwable throwable) {

        //stack stace
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        throwable.printStackTrace(pw);
        String stacktrace = sw.toString();

        return Util.getLogHeader(startTime, crashTime, Util.javaCrashType, appId, appVersion)
                + "pid: " + pid + ", tid: " + Process.myTid() + ", name: " + thread.getName() + "  >>> " + processName + " <<<\n"
                + "\n"
                + "java stacktrace:\n"
                + stacktrace
                + "\n"
                + getBuildId(stacktrace);
    }

    private String getOtherThreadsInfo(Thread crashedThread) {

        int thdMatchedRegex = 0;
        int thdIgnoredByLimit = 0;
        int thdDumped = 0;

        //build whitelist regex list
        ArrayList<Pattern> whiteList = null;
        if (dumpAllThreadsWhiteList != null) {
            whiteList = new ArrayList<Pattern>();
            for (String s : dumpAllThreadsWhiteList) {
                try {
                    whiteList.add(Pattern.compile(s));
                } catch (Exception e) {
                    XCrash.getLogger().w(Util.TAG, "JavaCrashHandler pattern compile failed", e);
                }
            }
        }

        StringBuilder sb = new StringBuilder();
        Map<Thread, StackTraceElement[]> map = Thread.getAllStackTraces();
        for (Map.Entry<Thread, StackTraceElement[]> entry : map.entrySet()) {

            Thread thd = entry.getKey();
            StackTraceElement[] stacktrace = entry.getValue();

            //skip the crashed thread
            if (thd.getName().equals(crashedThread.getName())) continue;

            //check regex for thread name
            if (whiteList != null && !matchThreadName(whiteList, thd.getName())) continue;
            thdMatchedRegex++;

            //check dump count limit
            if (dumpAllThreadsCountMax > 0 && thdDumped >= dumpAllThreadsCountMax) {
                thdIgnoredByLimit++;
                continue;
            }

            sb.append(Util.sepOtherThreads + "\n");
            sb.append("pid: ").append(pid).append(", tid: ").append(thd.getId()).append(", name: ").append(thd.getName()).append("  >>> ").append(processName).append(" <<<\n");
            sb.append("\n");
            sb.append("java stacktrace:\n");
            for (StackTraceElement element : stacktrace) {
                sb.append("    at ").append(element.toString()).append("\n");
            }
            sb.append("\n");

            thdDumped++;
        }

        if (map.size() > 1) {
            if (thdDumped == 0) {
                sb.append(Util.sepOtherThreads + "\n");
            }

            sb.append("total JVM threads (exclude the crashed thread): ").append(map.size() - 1).append("\n");
            if (whiteList != null) {
                sb.append("JVM threads matched whitelist: ").append(thdMatchedRegex).append("\n");
            }
            if (dumpAllThreadsCountMax > 0) {
                sb.append("JVM threads ignored by max count limit: ").append(thdIgnoredByLimit).append("\n");
            }
            sb.append("dumped JVM threads:").append(thdDumped).append("\n");
            sb.append(Util.sepOtherThreadsEnding + "\n");
        }

        return sb.toString();
    }

    private boolean matchThreadName(ArrayList<Pattern> whiteList, String threadName) {
        for (Pattern pat : whiteList) {
            if (pat.matcher(threadName).matches()) {
                return true;
            }
        }
        return false;
    }
}
