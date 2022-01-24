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

// Created by caikelun on 2019-09-03.
package xcrash;

import android.content.Context;
import android.os.Build;
import android.os.FileObserver;
import android.text.TextUtils;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.RandomAccessFile;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import static android.os.FileObserver.CLOSE_WRITE;

@SuppressWarnings("StaticFieldLeak")
class AnrHandler {

    private static final AnrHandler instance = new AnrHandler();

    private final Date startTime = new Date();
    private final Pattern patPidTime = Pattern.compile("^-----\\spid\\s(\\d+)\\sat\\s(.*)\\s-----$");
    private final Pattern patProcessName = Pattern.compile("^Cmd\\sline:\\s+(.*)$");
    private final long anrTimeoutMs = 15 * 1000;

    private Context ctx;
    private int pid;
    private String processName;
    private String appId;
    private String appVersion;
    private String logDir;
    private boolean checkProcessState;
    private int logcatSystemLines;
    private int logcatEventsLines;
    private int logcatMainLines;
    private boolean dumpFds;
    private boolean dumpNetworkInfo;
    private ICrashCallback callback;
    private ICrashCallback anrFastCallback;
    private long lastTime = 0;
    private FileObserver fileObserver = null;

    private AnrHandler() {
    }

    static AnrHandler getInstance() {
        return instance;
    }

    @SuppressWarnings("deprecation")
    void initialize(Context ctx, int pid, String processName, String appId, String appVersion, String logDir,
                    boolean checkProcessState, int logcatSystemLines, int logcatEventsLines, int logcatMainLines,
                    boolean dumpFds, boolean dumpNetworkInfo, ICrashCallback callback, ICrashCallback anrFastCallback) {

        //check API level
        if (Build.VERSION.SDK_INT >= 21) {
            return;
        }

        this.ctx = ctx;
        this.pid = pid;
        this.processName = (TextUtils.isEmpty(processName) ? "unknown" : processName);
        this.appId = appId;
        this.appVersion = appVersion;
        this.logDir = logDir;
        this.checkProcessState = checkProcessState;
        this.logcatSystemLines = logcatSystemLines;
        this.logcatEventsLines = logcatEventsLines;
        this.logcatMainLines = logcatMainLines;
        this.dumpFds = dumpFds;
        this.dumpNetworkInfo = dumpNetworkInfo;
        this.callback = callback;
        this.anrFastCallback = anrFastCallback;

        fileObserver = new FileObserver("/data/anr/", CLOSE_WRITE) {
            public void onEvent(int event, String path) {
                try {
                    if (path != null) {
                        String filepath = "/data/anr/" + path;
                        if (filepath.contains("trace")) {
                            handleAnr(filepath);
                        }
                    }
                } catch (Exception e) {
                    XCrash.getLogger().e(Util.TAG, "AnrHandler fileObserver onEvent failed", e);
                }
            }
        };

        try {
            fileObserver.startWatching();
        } catch (Exception e) {
            fileObserver = null;
            XCrash.getLogger().e(Util.TAG, "AnrHandler fileObserver startWatching failed", e);
        }
    }

    void notifyJavaCrashed() {
        if (fileObserver != null) {
            try {
                fileObserver.stopWatching();
            } catch (Exception e) {
                XCrash.getLogger().e(Util.TAG, "AnrHandler fileObserver stopWatching failed", e);
            } finally {
                fileObserver = null;
            }
        }
    }

    private void handleAnr(String filepath) {
        Date anrTime = new Date();

        //check ANR time interval
        if (anrTime.getTime() - lastTime < anrTimeoutMs) {
            return;
        }

        if(anrFastCallback != null) {
            try {
                anrFastCallback.onCrash(null, null);
            } catch (Exception ignored) {
            }
        }

        //check process error state
        if (this.checkProcessState) {
            if (!Util.checkProcessAnrState(this.ctx, anrTimeoutMs)) {
                return;
            }
        }

        //get trace
        String trace = getTrace(filepath, anrTime.getTime());
        if (TextUtils.isEmpty(trace)) {
            return;
        }

        //captured ANR
        lastTime = anrTime.getTime();

        //delete extra ANR log files
        if (!FileManager.getInstance().maintainAnr()) {
            return;
        }

        //get emergency
        String emergency = null;
        try {
            emergency = getEmergency(anrTime, trace);
        } catch (Exception e) {
            XCrash.getLogger().e(Util.TAG, "AnrHandler getEmergency failed", e);
        }

        //create log file
        File logFile = null;
        try {
            String logPath = String.format(Locale.US, "%s/%s_%020d_%s__%s%s", logDir, Util.logPrefix, anrTime.getTime() * 1000, appVersion, processName, Util.anrLogSuffix);
            logFile = FileManager.getInstance().createLogFile(logPath);
        } catch (Exception e) {
            XCrash.getLogger().e(Util.TAG, "AnrHandler createLogFile failed", e);
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
            } catch (Exception e) {
                XCrash.getLogger().e(Util.TAG, "AnrHandler write log file failed", e);
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

    private String getEmergency(Date anrTime, String trace) {
        return Util.getLogHeader(startTime, anrTime, Util.anrCrashType, appId, appVersion)
            + "pid: " + pid + "  >>> " + processName + " <<<\n"
            + "\n"
            + Util.sepOtherThreads
            + "\n"
            + trace
            + "\n"
            + Util.sepOtherThreadsEnding
            + "\n\n";
    }

    private String getTrace(String filepath, long anrTime) {

        // "\n\n----- pid %d at %04d-%02d-%02d %02d:%02d:%02d -----\n"
        // "Cmd line: %s\n"
        // "......"
        // "----- end %d -----\n"

        BufferedReader br = null;
        String line;
        Matcher matcher;
        SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US);
        StringBuilder sb = new StringBuilder();
        boolean found = false;

        try {
            br = new BufferedReader(new FileReader(filepath));
            while ((line = br.readLine()) != null) {
                if (!found && line.startsWith("----- pid ")) {

                    //check current line for PID and log time
                    matcher = patPidTime.matcher(line);
                    if (!matcher.find() || matcher.groupCount() != 2) {
                        continue;
                    }
                    String sPid = matcher.group(1);
                    String sLogTime = matcher.group(2);
                    if (sPid == null || sLogTime == null) {
                        continue;
                    }
                    if (pid != Integer.parseInt(sPid)) {
                        continue; //check PID
                    }
                    Date dLogTime = dateFormat.parse(sLogTime);
                    if (dLogTime == null) {
                        continue;
                    }
                    long logTime = dLogTime.getTime();
                    if (Math.abs(logTime - anrTime) > anrTimeoutMs) {
                        continue; //check log time
                    }

                    //check next line for process name
                    line = br.readLine();
                    if (line == null) {
                        break;
                    }
                    matcher = patProcessName.matcher(line);
                    if (!matcher.find() || matcher.groupCount() != 1) {
                        continue;
                    }
                    String pName = matcher.group(1);
                    if (pName == null || !(pName.equals(this.processName))) {
                        continue; //check process name
                    }

                    found = true;

                    sb.append(line).append('\n');
                    sb.append("Mode: Watching /data/anr/*\n");

                    continue;
                }

                if (found) {
                    if (line.startsWith("----- end ")) {
                        break;
                    } else {
                        sb.append(line).append('\n');
                    }
                }
            }
            return sb.toString();
        } catch (Exception ignored) {
            return null;
        } finally {
            if (br != null) {
                try {
                    br.close();
                } catch (Exception ignored) {
                }
            }
        }
    }
}
