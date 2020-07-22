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

import android.app.ActivityManager;
import android.content.Context;
import android.os.Build;
import android.os.Debug;
import android.system.Os;
import android.text.TextUtils;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileReader;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.math.BigInteger;
import java.security.MessageDigest;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

class Util {

    private Util() {
    }

    private static final String memInfoFmt = "%21s %8s\n";
    private static final String memInfoFmt2 = "%21s %8s %21s %8s\n";

    static final String TAG = "xcrash";

    static final String sepHead = "*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***";
    static final String sepOtherThreads = "--- --- --- --- --- --- --- --- --- --- --- --- --- --- --- ---";
    static final String sepOtherThreadsEnding = "+++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++";

    static final String timeFormatterStr = "yyyy-MM-dd'T'HH:mm:ss.SSSZ";

    static final String javaCrashType = "java";
    static final String nativeCrashType = "native";
    static final String anrCrashType = "anr";

    static final String logPrefix = "tombstone";
    static final String javaLogSuffix = ".java.xcrash";
    static final String nativeLogSuffix = ".native.xcrash";
    static final String anrLogSuffix = ".anr.xcrash";
    static final String traceLogSuffix = ".trace.xcrash";

    static String getProcessName(Context ctx, int pid) {

        //get from ActivityManager
        try {
            ActivityManager manager = (ActivityManager) ctx.getSystemService(Context.ACTIVITY_SERVICE);
            if (manager != null) {
                List<ActivityManager.RunningAppProcessInfo> processInfoList = manager.getRunningAppProcesses();
                if (processInfoList != null) {
                    for (ActivityManager.RunningAppProcessInfo processInfo : processInfoList) {
                        if (processInfo.pid == pid && !TextUtils.isEmpty(processInfo.processName)) {
                            return processInfo.processName; //OK
                        }
                    }
                }
            }
        } catch (Exception ignored) {
        }

        //get from /proc/PID/cmdline
        BufferedReader br = null;
        try {
            br = new BufferedReader(new FileReader("/proc/" + pid + "/cmdline"));
            String processName = br.readLine();
            if (!TextUtils.isEmpty(processName)) {
                processName = processName.trim();
                if (!TextUtils.isEmpty(processName)) {
                    return processName; //OK
                }
            }
        } catch (Exception ignored) {
        } finally {
            try {
                if (br != null) {
                    br.close();
                }
            } catch (Exception ignored) {
            }
        }

        //failed
        return null;
    }

    private static final String[] suPathname = {
        "/data/local/su",
        "/data/local/bin/su",
        "/data/local/xbin/su",
        "/system/xbin/su",
        "/system/bin/su",
        "/system/bin/.ext/su",
        "/system/bin/failsafe/su",
        "/system/sd/xbin/su",
        "/system/usr/we-need-root/su",
        "/sbin/su",
        "/su/bin/su"};

    static boolean isRoot() {
        try {
            for (String path : suPathname) {
                File file = new File(path);
                if (file.exists()) {
                    return true;
                }
            }
        } catch (Exception ignored) {
        }
        return false;
    }

    @SuppressWarnings("deprecation")
    static String getAbiList() {
        if (android.os.Build.VERSION.SDK_INT >= 21) {
            return android.text.TextUtils.join(",", Build.SUPPORTED_ABIS);
        } else {
            String abi = Build.CPU_ABI;
            String abi2 = Build.CPU_ABI2;
            if (TextUtils.isEmpty(abi2)) {
                return abi;
            } else {
                return abi + "," + abi2;
            }
        }
    }

    static String getFileMD5(File file) {
        String s = null;

        if (!file.exists()) {
            return null;
        }

        FileInputStream in = null;
        byte[] buffer = new byte[1024];
        int len;

        try {
            MessageDigest md = MessageDigest.getInstance("MD5");
            in = new FileInputStream(file);
            while ((len = in.read(buffer, 0, 1024)) != -1) {
                md.update(buffer, 0, len);
            }
            in.close();
            BigInteger bigInt = new BigInteger(1, md.digest());
            s = String.format("%032x", bigInt);
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }

        return s;
    }

    static String getAppVersion(Context ctx) {
        String appVersion = null;

        try {
            appVersion = ctx.getPackageManager().getPackageInfo(ctx.getPackageName(), 0).versionName;
        } catch (Exception ignored) {
        }

        if (TextUtils.isEmpty(appVersion)) {
            appVersion = "unknown";
        }

        return appVersion;
    }

    static String getProcessMemoryInfo() {
        StringBuilder sb = new StringBuilder();
        sb.append(" Process Summary (From: android.os.Debug.MemoryInfo)\n");
        sb.append(String.format(Locale.US, memInfoFmt, "", "Pss(KB)"));
        sb.append(String.format(Locale.US, memInfoFmt, "", "------"));

        try {
            Debug.MemoryInfo mi = new Debug.MemoryInfo();
            Debug.getMemoryInfo(mi);

            if (Build.VERSION.SDK_INT >= 23) {
                sb.append(String.format(Locale.US, memInfoFmt, "Java Heap:", mi.getMemoryStat("summary.java-heap")));
                sb.append(String.format(Locale.US, memInfoFmt, "Native Heap:", mi.getMemoryStat("summary.native-heap")));
                sb.append(String.format(Locale.US, memInfoFmt, "Code:", mi.getMemoryStat("summary.code")));
                sb.append(String.format(Locale.US, memInfoFmt, "Stack:", mi.getMemoryStat("summary.stack")));
                sb.append(String.format(Locale.US, memInfoFmt, "Graphics:", mi.getMemoryStat("summary.graphics")));
                sb.append(String.format(Locale.US, memInfoFmt, "Private Other:", mi.getMemoryStat("summary.private-other")));
                sb.append(String.format(Locale.US, memInfoFmt, "System:", mi.getMemoryStat("summary.system")));
                sb.append(String.format(Locale.US, memInfoFmt2, "TOTAL:", mi.getMemoryStat("summary.total-pss"), "TOTAL SWAP:", mi.getMemoryStat("summary.total-swap")));
            } else {
                sb.append(String.format(Locale.US, memInfoFmt, "Java Heap:", "~ " + mi.dalvikPrivateDirty));
                sb.append(String.format(Locale.US, memInfoFmt, "Native Heap:", String.valueOf(mi.nativePrivateDirty)));
                sb.append(String.format(Locale.US, memInfoFmt, "Private Other:", "~ " + mi.otherPrivateDirty));
                if (Build.VERSION.SDK_INT >= 19) {
                    sb.append(String.format(Locale.US, memInfoFmt, "System:", String.valueOf(mi.getTotalPss() - mi.getTotalPrivateDirty() - mi.getTotalPrivateClean())));
                } else {
                    sb.append(String.format(Locale.US, memInfoFmt, "System:", "~ " + (mi.getTotalPss() - mi.getTotalPrivateDirty())));
                }
                sb.append(String.format(Locale.US, memInfoFmt, "TOTAL:", String.valueOf(mi.getTotalPss())));
            }
        } catch (Exception e) {
            XCrash.getLogger().i(Util.TAG, "Util getProcessMemoryInfo failed", e);
        }

        return sb.toString();
    }

    private static String getFileContent(String pathname) {
        return getFileContent(pathname, 0);
    }

    private static String getFileContent(String pathname, int limit) {
        StringBuilder sb = new StringBuilder();
        BufferedReader br = null;
        String line;
        int n = 0;
        try {
            br = new BufferedReader(new FileReader(pathname));
            while (null != (line = br.readLine())) {
                String p = line.trim();
                if (p.length() > 0) {
                    n++;
                    if (limit == 0 || n <= limit) {
                        sb.append("  ").append(p).append("\n");
                    }
                }
            }
            if (limit > 0 && n > limit) {
                sb.append("  ......\n").append("  (number of records: ").append(n).append(")\n");
            }
        } catch (Exception e) {
            XCrash.getLogger().i(Util.TAG, "Util getInfo(" + pathname + ") failed", e);
        } finally {
            if (br != null) {
                try {
                    br.close();
                } catch (Exception ignored) {
                }
            }
        }
        return sb.toString();
    }

    @SuppressWarnings({"ResultOfMethodCallIgnored", "BooleanMethodIsAlwaysInverted"})
    static boolean checkAndCreateDir(String path) {
        File dir = new File(path);
        try {
            if (!dir.exists()) {
                dir.mkdirs();
                return dir.exists() && dir.isDirectory();
            } else {
                return dir.isDirectory();
            }
        } catch (Exception ignored) {
            return false;
        }
    }

    @SuppressWarnings("BooleanMethodIsAlwaysInverted")
    static boolean checkProcessAnrState(Context ctx, long timeoutMs) {
        ActivityManager am = (ActivityManager) ctx.getSystemService(Context.ACTIVITY_SERVICE);
        if (am == null) return false;

        int pid = android.os.Process.myPid();
        long poll = timeoutMs / 500;
        for (int i = 0; i < poll; i++) {
            List<ActivityManager.ProcessErrorStateInfo> processErrorList = am.getProcessesInErrorState();
            if (processErrorList != null) {
                for (ActivityManager.ProcessErrorStateInfo errorStateInfo : processErrorList) {
                    if (errorStateInfo.pid == pid && errorStateInfo.condition == ActivityManager.ProcessErrorStateInfo.NOT_RESPONDING) {
                        return true;
                    }
                }
            }

            try {
                Thread.sleep(500);
            } catch (Exception ignored) {
            }
        }

        return false;
    }

    static String getLogHeader(Date startTime, Date crashTime, String crashType, String appId, String appVersion) {
        DateFormat timeFormatter = new SimpleDateFormat(Util.timeFormatterStr, Locale.US);

        return Util.sepHead + "\n"
            + "Tombstone maker: '" + Version.fullVersion + "'\n"
            + "Crash type: '" + crashType + "'\n"
            + "Start time: '" + timeFormatter.format(startTime) + "'\n"
            + "Crash time: '" + timeFormatter.format(crashTime) + "'\n"
            + "App ID: '" + appId + "'\n"
            + "App version: '" + appVersion + "'\n"
            + "Rooted: '" + (Util.isRoot() ? "Yes" : "No") + "'\n"
            + "API level: '" + Build.VERSION.SDK_INT + "'\n"
            + "OS version: '" + Build.VERSION.RELEASE + "'\n"
            + "ABI list: '" + Util.getAbiList() + "'\n"
            + "Manufacturer: '" + Build.MANUFACTURER + "'\n"
            + "Brand: '" + Build.BRAND + "'\n"
            + "Model: '" + Util.getMobileModel() + "'\n"
            + "Build fingerprint: '" + Build.FINGERPRINT + "'\n";
    }

    static String getMemoryInfo() {
        return "memory info:\n"
            + " System Summary (From: /proc/meminfo)\n"
            + Util.getFileContent("/proc/meminfo")
            + "-\n"
            + " Process Status (From: /proc/PID/status)\n"
            + Util.getFileContent("/proc/self/status")
            + "-\n"
            + " Process Limits (From: /proc/PID/limits)\n"
            + Util.getFileContent("/proc/self/limits")
            + "-\n"
            + Util.getProcessMemoryInfo()
            + "\n";
    }

    static String getNetworkInfo() {
        if (Build.VERSION.SDK_INT >= 29) {
            return "network info:\n"
                + "Not supported on Android Q (API level 29) and later.\n"
                + "\n";
        } else {
            return "network info:\n"
                + " TCP over IPv4 (From: /proc/PID/net/tcp)\n"
                + Util.getFileContent("/proc/self/net/tcp", 1024)
                + "-\n"
                + " TCP over IPv6 (From: /proc/PID/net/tcp6)\n"
                + Util.getFileContent("/proc/self/net/tcp6", 1024)
                + "-\n"
                + " UDP over IPv4 (From: /proc/PID/net/udp)\n"
                + Util.getFileContent("/proc/self/net/udp", 1024)
                + "-\n"
                + " UDP over IPv6 (From: /proc/PID/net/udp6)\n"
                + Util.getFileContent("/proc/self/net/udp6", 1024)
                + "-\n"
                + " ICMP in IPv4 (From: /proc/PID/net/icmp)\n"
                + Util.getFileContent("/proc/self/net/icmp", 256)
                + "-\n"
                + " ICMP in IPv6 (From: /proc/PID/net/icmp6)\n"
                + Util.getFileContent("/proc/self/net/icmp6", 256)
                + "-\n"
                + " UNIX domain (From: /proc/PID/net/unix)\n"
                + Util.getFileContent("/proc/self/net/unix", 256)
                + "\n";
        }
    }

    static String getFds() {
        StringBuilder sb = new StringBuilder("open files:\n");

        try {
            File dir = new File("/proc/self/fd");
            File[] fds = dir.listFiles(new FilenameFilter() {
                @Override
                public boolean accept(File dir, String name) {
                    return TextUtils.isDigitsOnly(name);
                }
            });

            int count = 0;
            if (fds != null) {
                for (File fd : fds) {
                    String path = null;
                    try {
                        if (Build.VERSION.SDK_INT >= 21) {
                            path = Os.readlink(fd.getAbsolutePath());
                        } else {
                            path = fd.getCanonicalPath();
                        }
                    } catch (Exception ignored) {
                    }
                    sb.append("    fd ").append(fd.getName()).append(": ")
                        .append(TextUtils.isEmpty(path) ? "???" : path.trim()).append('\n');

                    count++;
                    if (count > 1024) {
                        break;
                    }
                }

                if (fds.length > 1024) {
                    sb.append("    ......\n");
                }

                sb.append("    (number of FDs: ").append(fds.length).append(")\n");
            }
        } catch (Exception ignored) {
        }

        sb.append('\n');
        return sb.toString();
    }

    static String getLogcat(int logcatMainLines, int logcatSystemLines, int logcatEventsLines) {
        int pid = android.os.Process.myPid();
        StringBuilder sb = new StringBuilder();

        sb.append("logcat:\n");

        if (logcatMainLines > 0) {
            getLogcatByBufferName(pid, sb, "main", logcatMainLines, 'D');
        }
        if (logcatSystemLines > 0) {
            getLogcatByBufferName(pid, sb, "system", logcatSystemLines, 'W');
        }
        if (logcatEventsLines > 0) {
            getLogcatByBufferName(pid, sb, "events", logcatSystemLines, 'I');
        }

        sb.append("\n");

        return sb.toString();
    }

    private static void getLogcatByBufferName(int pid, StringBuilder sb, String bufferName, int lines, char priority) {
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
        sb.append("--------- tail end of log ").append(bufferName);
        sb.append(" (").append(android.text.TextUtils.join(" ", commandArray)).append(")\n");

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
            XCrash.getLogger().w(Util.TAG, "Util run logcat command failed", e);
        } finally {
            if (br != null) {
                try {
                    br.close();
                } catch (IOException ignored) {
                }
            }
        }
    }


    public static String getSystemProperty(String key, String defaultValue) {
        try {
            Class<?> clz = Class.forName("android.os.SystemProperties");
            Method get = clz.getMethod("get", String.class, String.class);
            return (String)get.invoke(clz, key, defaultValue);
        } catch (NoSuchMethodException var4) {
            var4.printStackTrace();
        } catch (IllegalAccessException var5) {
            var5.printStackTrace();
        } catch (InvocationTargetException var6) {
            var6.printStackTrace();
        } catch (ClassNotFoundException var7) {
            var7.printStackTrace();
        }

        return defaultValue;
    }

    public static boolean isMIUI() {
        String property = getSystemProperty("ro.miui.ui.version.name", "");
        return !TextUtils.isEmpty(property);
    }

    public static String getMobileModel() {
        String mobileModel = null;
        if (isMIUI()) {
            String deviceName = "";

            try {
                Class SystemProperties = Class.forName("android.os.SystemProperties");
                Method get = SystemProperties.getDeclaredMethod("get", String.class, String.class);
                deviceName = (String) get.invoke(SystemProperties, "ro.product.marketname", "");
                if (TextUtils.isEmpty(deviceName)) {
                    deviceName = (String) get.invoke(SystemProperties, "ro.product.model", "");
                }
            } catch (InvocationTargetException var3) {
                var3.printStackTrace();
            } catch (NoSuchMethodException var4) {
                var4.printStackTrace();
            } catch (IllegalAccessException var5) {
                var5.printStackTrace();
            } catch (ClassNotFoundException var6) {
                var6.printStackTrace();
            }

            mobileModel = deviceName;
        } else {
            mobileModel = Build.MODEL;
        }

        if (mobileModel == null) {
            mobileModel = "";
        }

        return mobileModel;
    }
}
