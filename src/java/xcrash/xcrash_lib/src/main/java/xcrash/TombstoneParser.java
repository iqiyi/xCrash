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

import android.os.Build;
import android.text.TextUtils;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.io.StringReader;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Tombstone (crash) log file parser.
 */
@SuppressWarnings("unused")
public class TombstoneParser {

    /**
     * The tombstone file maker's library name and version.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyTombstoneMaker = "Tombstone maker";

    /**
     * Crash type. ("java" or "native")
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyCrashType = "Crash type";

    /**
     * APP Start time (xCrash initialized time). (Format: "yyyy-MM-dd'T'HH:mm:ss.SSSZ")
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyStartTime = "Start time";

    /**
     * Crash time. (Format: "yyyy-MM-dd'T'HH:mm:ss.SSSZ")
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyCrashTime = "Crash time";

    /**
     * The name of this application's package. (From: {@link android.content.Context#getPackageName()})
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyAppId = "App ID";

    /**
     * The version name of this package. (From: {@link android.content.pm.PackageInfo#versionName})
     * Your can override it by {@link xcrash.XCrash.InitParameters#setLogDir(String)}.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyAppVersion = "App version";

    /**
     * CPU load average. (From: /proc/loadavg)
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyCpuLoadavg = "CPU loadavg";

    /**
     * Online CPU. (From: /sys/devices/system/cpu/online)
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyCpuOnline = "CPU online";

    /**
     * Offline CPU. (From: /sys/devices/system/cpu/offline)
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyCpuOffline = "CPU offline";

    /**
     * Total physical memory size. (From: /proc/meminfo)
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keySystemMemoryTotal = "System memory total";

    /**
     * Memory used by the entire system. (From: /proc/meminfo)
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keySystemMemoryUsed = "System memory used";

    /**
     * The number of threads in the current process. (From: /proc/PID/task)
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyNumberOfThreads = "Number of threads";

    /**
     * Whether this device has been rooted(jailbroken). ("Yes" or "No")
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyRooted = "Rooted";

    /**
     * Android API level. (From: {@link android.os.Build.VERSION#SDK_INT})
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyApiLevel = "API level";

    /**
     * Android OS version. (From: {@link android.os.Build.VERSION#RELEASE})
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyOsVersion = "OS version";

    /**
     * Linux kernel version.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyKernelVersion = "Kernel version";

    /**
     * Supported ABI list. (From: {@link android.os.Build#SUPPORTED_ABIS})
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyAbiList = "ABI list";

    /**
     * Manufacturer. (From: {@link android.os.Build#MANUFACTURER})
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyManufacturer = "Manufacturer";

    /**
     * Brand. (From: {@link android.os.Build#BRAND})
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyBrand = "Brand";

    /**
     * Model. (From: {@link android.os.Build#MODEL})
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyModel = "Model";

    /**
     * Build fingerprint.  (From: {@link android.os.Build#FINGERPRINT})
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyBuildFingerprint = "Build fingerprint";

    /**
     * Revision. (From: ro.revision)
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyRevision = "Revision";

    /**
     * Current ABI. ("arm" or "arm64" or "x86" or "x86_64")
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyAbi = "ABI";

    /**
     * Process ID.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyProcessId = "pid";

    /**
     * Thread ID.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyThreadId = "tid";

    /**
     * Process name.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyProcessName = "pname";

    /**
     * Thread name.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyThreadName = "tname";

    /**
     * Native crash signal name.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keySignal = "signal";

    /**
     * Native crash signal code.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyCode = "code";

    /**
     * Native crash fault address.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyFaultAddr = "fault addr";

    /**
     * Native crash registers values.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyRegisters = "registers";

    /**
     * Native crash backtrace.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyBacktrace = "backtrace";

    /**
     * Native crash backtrace's library build-id.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyBuildId = "build id";

    /**
     * Native crash stack.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyStack = "stack";

    /**
     * Native crash memory near information.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyMemoryNear = "memory near";

    /**
     * Native crash memory map.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyMemoryMap = "memory map";

    /**
     * Native crash logcat.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyLogcat = "logcat";

    /**
     * Native crash FD list.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyOpenFiles = "open files";

    /**
     * Memory info. (From: /proc/PID/smaps)
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyMemoryInfo = "memory info";

    /**
     * Native crash other threads information.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyOtherThreads = "other threads";

    /**
     * Native crash thread's Java stacktrace from JVM, or Java exception stacktrace.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyJavaStacktrace = "java stacktrace";

    /**
     * Error code from xCrash itself.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyXCrashError = "xcrash error";

    /**
     * Error message from xCrash itself.
     */
    @SuppressWarnings("WeakerAccess")
    public static final String keyXCrashErrorDebug = "xcrash error debug";

    private static final Pattern patHeadItem = Pattern.compile("^(.*):\\s'(.*?)'$");
    private static final Pattern patProcessThread = Pattern.compile("^pid:\\s(.*),\\stid:\\s(.*),\\sname:\\s(.*)\\s+>>>\\s(.*)\\s<<<$");
    private static final Pattern patSignalCode = Pattern.compile("^signal\\s(.*),\\scode\\s(.*),\\sfault\\saddr\\s(.*)$");
    private static final Pattern patAppVersionProcessName = Pattern.compile("^(\\d{20})_(.*)__(.*)$");

    private static final Set<String> keyHeadItems = new HashSet<String>(Arrays.asList(
        keyTombstoneMaker,
        keyCrashType,
        keyStartTime,
        keyCrashTime,
        keyAppId,
        keyAppVersion,
        keyCpuLoadavg,
        keyCpuOnline,
        keyCpuOffline,
        keySystemMemoryTotal,
        keySystemMemoryUsed,
        keyNumberOfThreads,
        keyRooted,
        keyApiLevel,
        keyOsVersion,
        keyKernelVersion,
        keyAbiList,
        keyManufacturer,
        keyBrand,
        keyModel,
        keyBuildFingerprint,
        keyRevision,
        keyAbi
    ));

    private static final Set<String> keySections = new HashSet<String>(Arrays.asList(
        keyBacktrace,
        keyBuildId,
        keyStack,
        keyMemoryMap,
        keyLogcat,
        keyOpenFiles,
        keyJavaStacktrace,
        keyXCrashError,
        keyXCrashErrorDebug
    ));

    private enum Status {
        UNKNOWN,
        HEAD,
        SECTION
    }

    private TombstoneParser() {
    }

    /**
     * Parse a crash log file into an instance of {@link java.util.Map}.
     * Map's string keys are defined in {@link xcrash.TombstoneParser}.
     *
     * @param log Object of the crash log file.
     * @return The parsed map.
     */
    @SuppressWarnings("unused")
    public static Map<String, String> parse(File log) {
        return parse(log.getAbsolutePath(), null);
    }

    /**
     * Parse a crash log file into an instance of {@link java.util.Map}.
     * Map's string keys are defined in {@link xcrash.TombstoneParser}.
     *
     * @param logPath Absolute path of the crash log file.
     * @return The parsed map.
     */
    @SuppressWarnings("unused")
    public static Map<String, String> parse(String logPath) {
        return parse(logPath, null);
    }

    /**
     * Parse a crash log file (with an emergency buffer) into an instance of {@link java.util.Map}.
     * Map's string keys are defined in {@link xcrash.TombstoneParser}.
     *
     * <p>Note: This method is generally used in {@link xcrash.ICrashCallback#onCrash(String, String)}.
     *
     * @param logPath Absolute path of the crash log file.
     * @param emergency A buffer that holds basic crash information when disk exhausted.
     * @return The parsed map.
     */
    @SuppressWarnings("unused")
    public static Map<String, String> parse(String logPath, String emergency) {

        Map<String, String> map = new HashMap<String, String>();
        BufferedReader br = null;

        //parse content from log file
        if (logPath != null) {
            try {
                br = new BufferedReader(new FileReader(logPath));
                parseFromReader(map, br);
            } catch (Exception e) {
                e.printStackTrace();
            } finally {
                if (br != null) {
                    try {
                        br.close();
                    } catch (Exception ignored) {
                    }
                    br = null;
                }
            }
        }

        //parse content from emergency buffer
        if (emergency != null) {
            try {
                br = new BufferedReader(new StringReader(emergency));
                parseFromReader(map, br);
            } catch (Exception e) {
                e.printStackTrace();
            } finally {
                if (br != null) {
                    try {
                        br.close();
                    } catch (Exception ignored) {
                    }
                }
            }
        }

        //try to parse APP version, process name, crash type, start time and crash time from log path
        parseFromLogPath(map, logPath);

        //always try to set APP version
        String appVersion = map.get(keyAppVersion);
        if (TextUtils.isEmpty(appVersion)) {
            appVersion = XCrash.getAppVersion();
            map.put(keyAppVersion, TextUtils.isEmpty(appVersion) ? "unknown" : appVersion);
        }

        //add system info if there were missing
        addSystemInfo(map);

        return map;
    }

    private static void parseFromLogPath(Map<String, String> map, String logPath) {
        if (logPath == null) {
            return;
        }

        //add crash time
        if (TextUtils.isEmpty(map.get(keyCrashTime))) {
            DateFormat timeFormatter = new SimpleDateFormat(Util.timeFormatterStr, Locale.US);
            map.put(keyCrashTime, timeFormatter.format(new Date(new File(logPath).lastModified())));
        }

        String startTime = map.get(keyStartTime);
        String appVersion = map.get(keyAppVersion);
        String processName = map.get(keyProcessName);
        String crashType = map.get(keyCrashType);

        if (TextUtils.isEmpty(startTime)
                || TextUtils.isEmpty(appVersion)
                || TextUtils.isEmpty(processName)
                || TextUtils.isEmpty(crashType)) {

            //get file name
            String filename = logPath.substring(logPath.lastIndexOf('/') + 1);
            if (filename.isEmpty()) return;

            //ignore prefix
            if (!filename.startsWith(Util.logPrefix + "_")) return;
            filename = filename.substring(Util.logPrefix.length() + 1);

            //ignore suffix, save crash type
            if (filename.endsWith(Util.javaLogSuffix)) {
                if (TextUtils.isEmpty(crashType)) {
                    map.put(keyCrashType, Util.javaCrashType);
                }
                filename = filename.substring(0, filename.length() - Util.javaLogSuffix.length());
            } else if (filename.endsWith(Util.nativeLogSuffix)) {
                if (TextUtils.isEmpty(crashType)) {
                    map.put(keyCrashType, Util.nativeCrashType);
                }
                filename = filename.substring(0, filename.length() - Util.nativeLogSuffix.length());
            } else {
                return;
            }

            //get APP version and/or process name
            if (TextUtils.isEmpty(startTime) || TextUtils.isEmpty(appVersion) || TextUtils.isEmpty(processName)) {
                Matcher matcher = patAppVersionProcessName.matcher(filename);
                if (matcher.find() && matcher.groupCount() == 3) {
                    if (TextUtils.isEmpty(startTime)) {
                        long crashTimeLong = Long.parseLong(matcher.group(1), 10) / 1000;
                        DateFormat timeFormatter = new SimpleDateFormat(Util.timeFormatterStr, Locale.US);
                        map.put(keyStartTime, timeFormatter.format(new Date(crashTimeLong)));
                    }
                    if (TextUtils.isEmpty(appVersion)) {
                        map.put(keyAppVersion, matcher.group(2));
                    }
                    if (TextUtils.isEmpty(processName)) {
                        map.put(keyProcessName, matcher.group(3));
                    }
                }
            }
        }
    }

    private static void addSystemInfo(Map<String, String> map) {

        if (TextUtils.isEmpty(map.get(keyAppId))) {
            map.put(keyAppId, XCrash.getAppId());
        }

        if (TextUtils.isEmpty(map.get(keyTombstoneMaker))) {
            map.put(keyTombstoneMaker, Version.fullVersion);
        }

        if (TextUtils.isEmpty(map.get(keyRooted))) {
            map.put(keyRooted, Util.isRoot() ? "Yes" : "No");
        }

        if (TextUtils.isEmpty(map.get(keyApiLevel))) {
            map.put(keyApiLevel, String.valueOf(Build.VERSION.SDK_INT));
        }

        if (TextUtils.isEmpty(map.get(keyOsVersion))) {
            map.put(keyOsVersion, Build.VERSION.RELEASE);
        }

        if (TextUtils.isEmpty(map.get(keyBuildFingerprint))) {
            map.put(keyModel, Build.FINGERPRINT);
        }

        if (TextUtils.isEmpty(map.get(keyManufacturer))) {
            map.put(keyManufacturer, Build.MANUFACTURER);
        }

        if (TextUtils.isEmpty(map.get(keyBrand))) {
            map.put(keyBrand, Build.BRAND);
        }

        if (TextUtils.isEmpty(map.get(keyModel))) {
            map.put(keyModel, Build.MODEL);
        }

        if (TextUtils.isEmpty(map.get(keyAbiList))) {
            map.put(keyAbiList, Util.getAbiList());
        }
    }

    private static void parseFromReader(Map<String, String> map, BufferedReader br) throws IOException {
        String next, line;
        String sectionTitle = null;
        StringBuilder sectionContent = new StringBuilder();
        String sectionContentEnding = "";
        boolean sectionContentOutdent = false;
        boolean sectionContentAppend = false;
        Matcher matcher;
        Status status = Status.UNKNOWN;

        line = br.readLine();
        for (boolean last = (line == null); !last; line = next) {
            last = ((next = br.readLine()) == null);
            switch (status) {
                case UNKNOWN:
                    if (line.equals(Util.sepHead)) {
                        status = Status.HEAD;
                    } else if (line.equals(Util.sepOtherThreads)) {
                        //special case
                        status = Status.SECTION;
                        sectionTitle = keyOtherThreads;
                        sectionContentEnding = Util.sepOtherThreadsEnding;
                        sectionContentOutdent = false;
                        sectionContentAppend = false;
                        sectionContent.append(line).append('\n');
                    } else if (line.length() > 1 && line.endsWith(":")) {
                        status = Status.SECTION;
                        sectionTitle = line.substring(0, line.length() - 1);
                        sectionContentEnding = "";
                        if (keySections.contains(sectionTitle)) {
                            sectionContentOutdent = (sectionTitle.equals(keyBacktrace)
                                || sectionTitle.equals(keyBuildId)
                                || sectionTitle.equals(keyStack)
                                || sectionTitle.equals(keyMemoryMap)
                                || sectionTitle.equals(keyOpenFiles)
                                || sectionTitle.equals(keyJavaStacktrace)
                                || sectionTitle.equals(keyXCrashErrorDebug));
                            sectionContentAppend = sectionTitle.equals(keyXCrashError);
                        } else if (sectionTitle.equals(keyMemoryInfo)) {
                            sectionContentOutdent = false;
                            sectionContentAppend = true;
                        } else if (sectionTitle.startsWith("memory near ")) {
                            //special case
                            sectionTitle = keyMemoryNear;
                            sectionContentOutdent = false;
                            sectionContentAppend = true;
                            sectionContent.append(line).append('\n');
                        } else {
                            //additional information section attached by users
                            sectionContentOutdent = false;
                            sectionContentAppend = false;
                        }
                    }
                    break;
                case HEAD:
                    if (line.startsWith("pid: ")) {
                        matcher = patProcessThread.matcher(line);
                        if (matcher.find() && matcher.groupCount() == 4) {
                            //pid, process name, tid, thread name
                            putKeyValue(map, keyProcessId, matcher.group(1));
                            putKeyValue(map, keyThreadId, matcher.group(2));
                            putKeyValue(map, keyThreadName, matcher.group(3));
                            putKeyValue(map, keyProcessName, matcher.group(4));
                        }
                    } else if (line.startsWith("signal ")) {
                        matcher = patSignalCode.matcher(line);
                        if (matcher.find() && matcher.groupCount() == 3) {
                            //signal, code, fault address
                            putKeyValue(map, keySignal, matcher.group(1));
                            putKeyValue(map, keyCode, matcher.group(2));
                            putKeyValue(map, keyFaultAddr, matcher.group(3));

                            //special case
                            if (next != null && (next.startsWith("    r0 ") || next.startsWith("    x0 ") || next.startsWith("    eax ") || next.startsWith("    rax "))) {
                                status = Status.SECTION;
                                sectionTitle = keyRegisters;
                                sectionContentEnding = "";
                                sectionContentOutdent = true;
                                sectionContentAppend = false;
                            }
                        }
                    } else {
                        //other items in head section
                        matcher = patHeadItem.matcher(line);
                        if (matcher.find() && matcher.groupCount() == 2) {
                            if (keyHeadItems.contains(matcher.group(1))) {
                                putKeyValue(map, matcher.group(1), matcher.group(2));
                            }
                        }
                    }

                    if (next == null || next.isEmpty()) {
                        //the end of head
                        status = Status.UNKNOWN;
                    }
                    break;
                case SECTION:
                    if (line.equals(sectionContentEnding) || last) {
                        putKeyValue(map, sectionTitle, sectionContent.toString(), sectionContentAppend);
                        sectionContent.setLength(0);
                        status = Status.UNKNOWN;
                    } else {
                        if (sectionContentOutdent && line.startsWith("    ")) {
                            line = line.substring(4);
                        }
                        sectionContent.append(line).append('\n');
                    }
                    break;
                default:
                    break;
            }
        }
    }

    private static void putKeyValue(Map<String, String> map, String k, String v) {
        putKeyValue(map, k, v, false);
    }

    private static void putKeyValue(Map<String, String> map, String k, String v, boolean append) {
        if (k == null || k.isEmpty() || v == null) return;

        String oldValue = map.get(k);

        if (append) {
            map.put(k, (oldValue == null ? v : oldValue + v));
        } else {
            if (oldValue == null || (oldValue.isEmpty() && !v.isEmpty())) {
                map.put(k, v);
            }
        }
    }
}
