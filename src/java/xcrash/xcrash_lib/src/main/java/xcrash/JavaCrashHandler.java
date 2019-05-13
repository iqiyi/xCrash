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

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.Thread.UncaughtExceptionHandler;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.regex.Pattern;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Build;

@SuppressLint("StaticFieldLeak")
class JavaCrashHandler implements UncaughtExceptionHandler {

    private static final JavaCrashHandler instance = new JavaCrashHandler();

    private final Date startTime = new Date();

    private Context ctx;
    private int pid;
    private String processName;
    private String appId;
    private String appVersion;
    private boolean rethrow;
    private String logDir;
    private int logCountMax;
    private int logcatSystemLines;
    private int logcatEventsLines;
    private int logcatMainLines;
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

    void initialize(Context ctx, String appId, String appVersion, String logDir, boolean rethrow, int logCountMax,
                    int logcatSystemLines, int logcatEventsLines, int logcatMainLines,
                    boolean dumpAllThreads, int dumpAllThreadsCountMax, String[] dumpAllThreadsWhiteList,
                    ICrashCallback callback) {
        this.ctx = ctx;
        this.pid = android.os.Process.myPid();
        this.processName = Util.getProcessName(ctx, this.pid);
        this.appId = appId;
        this.appVersion = appVersion;
        this.rethrow = rethrow;
        this.logDir = logDir;
        this.logCountMax = (logCountMax <= 0 ? 10 : logCountMax);
        this.logcatSystemLines = logcatSystemLines;
        this.logcatEventsLines = logcatEventsLines;
        this.logcatMainLines = logcatMainLines;
        this.dumpAllThreads = dumpAllThreads;
        this.dumpAllThreadsCountMax = dumpAllThreadsCountMax;
        this.dumpAllThreadsWhiteList = dumpAllThreadsWhiteList;
        this.callback = callback;
        this.defaultHandler = Thread.getDefaultUncaughtExceptionHandler();

        try {
            Thread.setDefaultUncaughtExceptionHandler(this);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @Override
    public void uncaughtException(Thread thread, Throwable throwable) {
        handleException(thread, throwable);

        if (this.rethrow && defaultHandler != null) {
            defaultHandler.uncaughtException(thread, throwable);
        } else {
            android.os.Process.killProcess(this.pid);
        }
    }

    @SuppressWarnings("TryFinallyCanBeTryWithResources")
    private void handleException(Thread thread, Throwable throwable) {
        Date crashTime = new Date();

        //get emergency
        String emergency = getEmergency(crashTime, thread, throwable);

        //create log file
        String logPath = createLogFile();

        //write info to log file
        if (logPath != null) {
            BufferedWriter writer = null;
            try {
                writer = new BufferedWriter(new FileWriter(logPath, false));

                //write & flush emergency info
                writer.write(emergency);
                writer.flush();

                //If we wrote the emergency info successfully, we don't need to return it from callback again.
                emergency = null;

                //write logcat
                if (logcatMainLines > 0 || logcatSystemLines > 0 || logcatEventsLines > 0) {
                    writer.write(getLogcat(pid));
                    writer.flush();
                }

                //write memory info
                writer.write("memory info:\n");
                writer.write(Util.getMemoryInfo());
                writer.write("\n");
                writer.flush();

                //write other threads info
                if (dumpAllThreads) {
                    writer.write(getOtherThreadsInfo(thread));
                    writer.flush();
                }
            } catch (Exception e) {
                e.printStackTrace();
            } finally {
                if (writer != null) {
                    try {
                        writer.close();
                    } catch (Exception ignored) {
                    }
                }
            }
        }

        //callback
        if (callback != null) {
            try {
                callback.onCrash(logPath, emergency);
            } catch (Exception ignored) {
            }
        }
    }

    @SuppressWarnings("ResultOfMethodCallIgnored")
    private String createLogFile() {
        //check and create dir
        File dir = new File(logDir);
        if (!dir.exists()) {
            dir.mkdirs();
        }
        if (!dir.exists() || !dir.isDirectory()) {
            return null;
        }

        //get all existing log files
        File[] files = dir.listFiles(new FilenameFilter() {
            @Override
            public boolean accept(File dir, String name) {
                return name.startsWith(Util.logPrefix + "_") && name.endsWith(Util.javaLogSuffix);
            }
        });

        //sort
        Arrays.sort(files, new Comparator<File>() {
            @Override
            public int compare(File f1, File f2) {
                return f1.getName().compareTo(f2.getName());
            }
        });

        //delete extra files
        if (files.length >= logCountMax) {
            for (int i = 0; i < files.length - logCountMax + 1; i++) {
                files[i].delete();
            }
        }

        //create new log file
        String path = String.format(Locale.US, "%s/%s_%020d_%s__%s%s", logDir, Util.logPrefix, startTime.getTime() * 1000, appVersion, processName, Util.javaLogSuffix);
        try {
            new File(path).createNewFile();
        } catch (Exception e) {
            e.printStackTrace();
        }

        return path;
    }

    private String getEmergency(Date crashTime, Thread thread, Throwable throwable) {
        //memory info
        Util.MemoryInfo mi = Util.getMemoryInfo(ctx);

        //stack stace
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        throwable.printStackTrace(pw);
        String stacktrace = sw.toString();

        DateFormat timeFormatter = new SimpleDateFormat(Util.timeFormatterStr, Locale.US);

        return Util.sepHead + "\n"
                + "Tombstone maker: '" + Version.fullVersion + "'\n"
                + "Crash type: '" + Util.javaCrashType + "'\n"
                + "Start time: '" + timeFormatter.format(startTime) + "'\n"
                + "Crash time: '" + timeFormatter.format(crashTime) + "'\n"
                + "App ID: '" + appId + "'\n"
                + "App version: '" + appVersion + "'\n"
                + "CPU loadavg: '" + Util.readFileLine("/proc/loadavg") + "'\n"
                + "CPU online: '" + Util.readFileLine("/sys/devices/system/cpu/online") + "'\n"
                + "CPU offline: '" + Util.readFileLine("/sys/devices/system/cpu/offline") + "'\n"
                + "System memory total: '" + mi.systemMemoryTotalKb + " kB'\n"
                + "System memory used: '" + mi.systemMemoryUsedKb + " kB'\n"
                + "Number of threads: '" + Util.getNumberOfThreads(pid) + "'\n"
                + "Rooted: '" + (Util.isRoot() ? "Yes" : "No") + "'\n"
                + "API level: '" + Build.VERSION.SDK_INT + "'\n"
                + "OS version: '" + Build.VERSION.RELEASE + "'\n"
                + "ABI list: '" + Util.getAbiList() + "'\n"
                + "Manufacturer: '" + Build.MANUFACTURER + "'\n"
                + "Brand: '" + Build.BRAND + "'\n"
                + "Model: '" + Build.MODEL + "'\n"
                + "Build fingerprint: '" + Build.FINGERPRINT + "'\n"
                + "pid: " + pid + ", tid: " + android.os.Process.myTid() + ", name: " + thread.getName() + "  >>> " + processName + " <<<\n"
                + "\n"
                + "java stacktrace:\n"
                + stacktrace
                + "\n";
    }

    private String getLogcat(int pid) {
        StringBuilder sb = new StringBuilder();

        sb.append("logcat:\n");

        if (logcatMainLines > 0) {
            getLogcatByBufferName(sb, pid, "main", logcatMainLines, 'D');
        }
        if (logcatSystemLines > 0) {
            getLogcatByBufferName(sb, pid, "system", logcatSystemLines, 'W');
        }
        if (logcatEventsLines > 0) {
            getLogcatByBufferName(sb, pid, "events", logcatSystemLines, 'I');
        }

        sb.append("\n");

        return sb.toString();
    }

    private void getLogcatByBufferName(StringBuilder sb, int pid, String bufferName, int lines, char priority) {
        boolean withPid = (android.os.Build.VERSION.SDK_INT >= 24);
        String pidString = Integer.toString(pid);
        String pidLabel = " " + pidString + " ";

        //command for ProcessBuilder
        List<String> command = new ArrayList<String>();
        command.add("/system/bin/logcat");
        command.add("-b");
        command.add(bufferName);
        command.add("-d");
        command.add("-v");
        command.add("threadtime");
        command.add("-t");
        command.add(Integer.toString(withPid ? lines : (int) (lines * 1.2)));
        if (withPid) {
            command.add("--pid");
            command.add(pidString);
        }
        command.add("*:" + priority);

        //append the command line
        Object[] commandArray = command.toArray();
        if (commandArray != null) {
            sb.append("--------- tail end of log ").append(bufferName);
            sb.append(" (").append(android.text.TextUtils.join(" ", commandArray)).append(")\n");
        }

        //append logs
        BufferedReader br = null;
        String line;
        try {
            Process process = new ProcessBuilder().command(command).start();
            br = new BufferedReader(new InputStreamReader(process.getInputStream()));
            while ((line = br.readLine()) != null) {
                if (withPid || line.contains(pidLabel)) {
                    sb.append(line).append("\n");
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            if (br != null) {
                try {
                    br.close();
                } catch (IOException ignored) {
                }
            }
        }
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
                    e.printStackTrace();
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
