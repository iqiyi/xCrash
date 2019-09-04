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

import android.app.ActivityManager;
import android.content.Context;
import android.os.Build;
import android.os.FileObserver;
import android.text.TextUtils;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FilenameFilter;
import java.io.RandomAccessFile;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Date;
import java.util.List;
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

    private Context ctx;
    private int pid;
    private String processName;
    private String appId;
    private String appVersion;
    private String logDir;
    private int logCountMax;
    private int logcatSystemLines;
    private int logcatEventsLines;
    private int logcatMainLines;
    private boolean dumpFds;
    private ICrashCallback callback;
    private long lastTime = 0;
    private FileObserver fileObserver = null;

    private AnrHandler() {
    }

    static AnrHandler getInstance() {
        return instance;
    }

    @SuppressWarnings("deprecation")
    void initialize(Context ctx, String appId, String appVersion, String logDir, int logCountMax,
                    int logcatSystemLines, int logcatEventsLines, int logcatMainLines,
                    boolean dumpFds, ICrashCallback callback) {

        //check API level
        if (Build.VERSION.SDK_INT >= 21) {
            return;
        }

        //check if you're in the main process
        int myPid = android.os.Process.myPid();
        String myProcessName = Util.getProcessName(ctx, myPid);
        String packageName = ctx.getPackageName();
        if (TextUtils.isEmpty(packageName) || !(packageName.equals(myProcessName))) {
            return;
        }

        this.ctx = ctx;
        this.pid = myPid;
        this.processName = myProcessName;
        this.appId = appId;
        this.appVersion = appVersion;
        this.logDir = logDir;
        this.logCountMax = logCountMax;
        this.logcatSystemLines = logcatSystemLines;
        this.logcatEventsLines = logcatEventsLines;
        this.logcatMainLines = logcatMainLines;
        this.dumpFds = dumpFds;
        this.callback = callback;

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
        if (anrTime.getTime() - lastTime < 10 * 1000) {
            return;
        }
        lastTime = anrTime.getTime();

        //check process error state
        if (!checkProcessErrorState()) {
            return;
        }

        //get trace
        String trace = getTrace(filepath, anrTime.getTime());
        if (TextUtils.isEmpty(trace)) {
            return;
        }

        //clean logs
        if (!logsClean()) {
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

    private boolean logsClean() {
        if (!Util.checkAndCreateDir(logDir)) {
            return false;
        }

        //get all ANR logs
        File dir = new File(logDir);
        File[] anrFiles = dir.listFiles(new FilenameFilter() {
            @Override
            public boolean accept(File dir, String name) {
                return name.startsWith(Util.logPrefix + "_") && name.endsWith(Util.anrLogSuffix);
            }
        });

        //check ANR logs count
        if (anrFiles == null || anrFiles.length < logCountMax) {
            return true;
        }

        //delete unwanted logs
        Arrays.sort(anrFiles, new Comparator<File>() {
            @Override
            public int compare(File f1, File f2) {
                return f1.getName().compareTo(f2.getName()); }
        });
        for (int i = 0; i < anrFiles.length - logCountMax + 1; i++) {
            FileManager.getInstance().recycleLogFile(anrFiles[i]);
        }

        //check ANR logs count again
        anrFiles = dir.listFiles(new FilenameFilter() {
            @Override
            public boolean accept(File dir, String name) {
                return name.startsWith(Util.logPrefix + "_") && name.endsWith(Util.anrLogSuffix);
            }
        });
        return (anrFiles == null || anrFiles.length < logCountMax);
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

    private boolean checkProcessErrorState() {
        ActivityManager am = (ActivityManager) ctx.getSystemService(Context.ACTIVITY_SERVICE);
        if (am != null) {
            for (int i = 0; i < 20; i++) {
                List<ActivityManager.ProcessErrorStateInfo> processErrorList = am.getProcessesInErrorState();
                if (processErrorList != null) {
                    for (ActivityManager.ProcessErrorStateInfo errorStateInfo : processErrorList) {
                        if (errorStateInfo.pid == this.pid && errorStateInfo.condition == ActivityManager.ProcessErrorStateInfo.NOT_RESPONDING) {
                            return true;
                        }
                    }
                }

                try {
                    Thread.sleep(500);
                } catch (Exception ignored) {
                }
            }
        }

        return false;
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
                    if (Math.abs(logTime - anrTime) > 10 * 1000) {
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
                    sb.append("Mode: Watching /data/anr/*.\n");

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
