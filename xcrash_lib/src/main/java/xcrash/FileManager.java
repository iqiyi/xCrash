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

// Created by caikelun on 2019-05-20.
package xcrash;

import java.io.File;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.RandomAccessFile;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Date;
import java.util.Locale;
import java.util.Timer;
import java.util.TimerTask;
import java.util.concurrent.atomic.AtomicInteger;

class FileManager {

    private String placeholderPrefix = "placeholder";
    private String placeholderCleanSuffix = ".clean.xcrash";
    private String placeholderDirtySuffix = ".dirty.xcrash";
    private String logDir = null;
    private int javaLogCountMax = 0;
    private int nativeLogCountMax = 0;
    private int anrLogCountMax = 0;
    private int traceLogCountMax = 1;
    private int placeholderCountMax = 0;
    private int placeholderSizeKb = 0;
    private int delayMs = 0;
    private AtomicInteger unique = new AtomicInteger();
    private static final FileManager instance = new FileManager();

    private FileManager() {
    }

    static FileManager getInstance() {
        return instance;
    }

    void initialize(String logDir, int javaLogCountMax, int nativeLogCountMax, int anrLogCountMax, int placeholderCountMax, int placeholderSizeKb, int delayMs) {
        this.logDir = logDir;
        this.javaLogCountMax = javaLogCountMax;
        this.nativeLogCountMax = nativeLogCountMax;
        this.anrLogCountMax = anrLogCountMax;
        this.placeholderCountMax = placeholderCountMax;
        this.placeholderSizeKb = placeholderSizeKb;
        this.delayMs = delayMs;

        try {
            File dir = new File(logDir);
            if (!dir.exists() || !dir.isDirectory()) {
                return;
            }
            File[] files = dir.listFiles();
            if (files == null) {
                return;
            }

            int javaLogCount = 0;
            int nativeLogCount = 0;
            int anrLogCount = 0;
            int traceLogCount = 0;
            int placeholderCleanCount = 0;
            int placeholderDirtyCount = 0;
            for (final File file : files) {
                if (file.isFile()) {
                    String name = file.getName();
                    if (name.startsWith(Util.logPrefix + "_")) {
                        if (name.endsWith(Util.javaLogSuffix)) {
                            javaLogCount++;
                        } else if (name.endsWith(Util.nativeLogSuffix)) {
                            nativeLogCount++;
                        } else if (name.endsWith(Util.anrLogSuffix)) {
                            anrLogCount++;
                        } else if (name.endsWith(Util.traceLogSuffix)) {
                            traceLogCount++;
                        }
                    } else if (name.startsWith(placeholderPrefix + "_")) {
                        if (name.endsWith(placeholderCleanSuffix)) {
                            placeholderCleanCount++;
                        } else if (name.endsWith(placeholderDirtySuffix)) {
                            placeholderDirtyCount++;
                        }
                    }
                }
            }

            if (javaLogCount <= this.javaLogCountMax
                && nativeLogCount <= this.nativeLogCountMax
                && anrLogCount <= this.anrLogCountMax
                && traceLogCount <= this.traceLogCountMax
                && placeholderCleanCount == this.placeholderCountMax
                && placeholderDirtyCount == 0) {
                //everything OK, need to do nothing
                this.delayMs = -1;
            } else if (javaLogCount > this.javaLogCountMax + 10
                || nativeLogCount > this.nativeLogCountMax + 10
                || anrLogCount > this.anrLogCountMax + 10
                || traceLogCount > this.traceLogCountMax + 10
                || placeholderCleanCount > this.placeholderCountMax + 10
                || placeholderDirtyCount > 10) {
                //too many unwanted files, clean up now
                doMaintain();
                this.delayMs = -1;
            } else if (javaLogCount > this.javaLogCountMax
                || nativeLogCount > this.nativeLogCountMax
                || anrLogCount > this.anrLogCountMax
                || traceLogCount > this.traceLogCountMax
                || placeholderCleanCount > this.placeholderCountMax
                || placeholderDirtyCount > 0) {
                //have some unwanted files, clean up as soon as possible
                this.delayMs = 0;
            }
        } catch (Exception e) {
            XCrash.getLogger().e(Util.TAG, "FileManager init failed", e);
        }
    }

    void maintain() {
        if (this.logDir == null || this.delayMs < 0) {
            return;
        }

        try {
            String threadName = "xcrash_file_mgr";
            if (delayMs == 0) {
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        doMaintain();
                    }
                }, threadName).start();
            } else {
                new Timer(threadName).schedule(
                    new TimerTask() {
                        @Override
                        public void run() {
                            doMaintain();
                        }
                    }, delayMs
                );
            }
        } catch (Exception e) {
            XCrash.getLogger().e(Util.TAG, "FileManager maintain start failed", e);
        }
    }

    boolean maintainAnr() {
        if (!Util.checkAndCreateDir(logDir)) {
            return false;
        }
        File dir = new File(logDir);

        try {
            return doMaintainTombstoneType(dir, Util.anrLogSuffix, anrLogCountMax);
        } catch (Exception e) {
            XCrash.getLogger().e(Util.TAG, "FileManager maintainAnr failed", e);
            return false;
        }
    }

    @SuppressWarnings("ResultOfMethodCallIgnored")
    File createLogFile(String filePath) {
        if (this.logDir == null) {
            return null;
        }

        if (!Util.checkAndCreateDir(logDir)) {
            return null;
        }

        File newFile = new File(filePath);

        //clean placeholder files
        File dir = new File(logDir);
        File[] cleanFiles = dir.listFiles(new FilenameFilter() {
            @Override
            public boolean accept(File dir, String name) {
                return name.startsWith(placeholderPrefix + "_") && name.endsWith(placeholderCleanSuffix);
            }
        });

        if (cleanFiles != null) {
            //try to rename from clean placeholder file
            int cleanFilesCount = cleanFiles.length;
            while (cleanFilesCount > 0) {
                File cleanFile = cleanFiles[cleanFilesCount - 1];
                try {
                    if (cleanFile.renameTo(newFile)) {
                        return newFile;
                    }
                } catch (Exception e) {
                    XCrash.getLogger().e(Util.TAG, "FileManager createLogFile by renameTo failed", e);
                }
                cleanFile.delete();
                cleanFilesCount--;
            }
        }

        //try to create new file
        try {
            if (newFile.createNewFile()) {
                return newFile;
            } else {
                XCrash.getLogger().e(Util.TAG, "FileManager createLogFile by createNewFile failed, file already exists");
                return null;
            }
        } catch (Exception e) {
            XCrash.getLogger().e(Util.TAG, "FileManager createLogFile by createNewFile failed", e);
            return null;
        }
    }

    boolean appendText(String logPath, String text) {
        RandomAccessFile raf = null;

        try {
            raf = new RandomAccessFile(logPath, "rws");

            //get the write position
            long pos = 0;
            if (raf.length() > 0) {
                FileChannel fc = raf.getChannel();
                MappedByteBuffer mbb = fc.map(FileChannel.MapMode.READ_ONLY, 0, raf.length());
                for (pos = raf.length(); pos > 0; pos--) {
                    if (mbb.get((int) pos - 1) != (byte) 0) {
                        break;
                    }
                }
            }

            //write text
            raf.seek(pos);
            raf.write(text.getBytes("UTF-8"));

            return true;
        } catch (Exception e) {
            XCrash.getLogger().e(Util.TAG, "FileManager appendText failed", e);
            return false;
        } finally {
            if (raf != null) {
                try {
                    raf.close();
                } catch (Exception ignored) {
                }
            }
        }
    }

    @SuppressWarnings({"unused"})
    boolean recycleLogFile(File logFile) {
        if (logFile == null) {
            return false;
        }

        if (this.logDir == null || this.placeholderCountMax <= 0) {
            try {
                return logFile.delete();
            } catch (Exception ignored) {
                return false;
            }
        }

        try {
            File dir = new File(logDir);
            File[] cleanFiles = dir.listFiles(new FilenameFilter() {
                @Override
                public boolean accept(File dir, String name) {
                    return name.startsWith(placeholderPrefix + "_") && name.endsWith(placeholderCleanSuffix);
                }
            });
            if (cleanFiles != null && cleanFiles.length >= this.placeholderCountMax) {
                try {
                    return logFile.delete();
                } catch (Exception ignored) {
                    return false;
                }
            }

            //rename to dirty file
            String dirtyFilePath = String.format(Locale.US, "%s/%s_%020d%s", logDir, placeholderPrefix, new Date().getTime() * 1000 + getNextUnique(), placeholderDirtySuffix);
            File dirtyFile = new File(dirtyFilePath);
            if (!logFile.renameTo(dirtyFile)) {
                try {
                    return logFile.delete();
                } catch (Exception ignored) {
                    return false;
                }
            }

            //clean the dirty file
            return cleanTheDirtyFile(dirtyFile);
        } catch (Exception e) {
            XCrash.getLogger().e(Util.TAG, "FileManager recycleLogFile failed", e);
            try {
                return logFile.delete();
            } catch (Exception ignored) {
                return false;
            }
        }
    }

    private void doMaintain() {
        if (!Util.checkAndCreateDir(logDir)) {
            return;
        }
        File dir = new File(logDir);

        try {
            doMaintainTombstone(dir);
        } catch (Exception e) {
            XCrash.getLogger().e(Util.TAG, "FileManager doMaintainTombstone failed", e);
        }

        try {
            doMaintainPlaceholder(dir);
        } catch (Exception e) {
            XCrash.getLogger().e(Util.TAG, "FileManager doMaintainPlaceholder failed", e);
        }
    }

    private void doMaintainTombstone(File dir) {
        doMaintainTombstoneType(dir, Util.nativeLogSuffix, nativeLogCountMax);
        doMaintainTombstoneType(dir, Util.javaLogSuffix, javaLogCountMax);
        doMaintainTombstoneType(dir, Util.anrLogSuffix, anrLogCountMax);
        doMaintainTombstoneType(dir, Util.traceLogSuffix, traceLogCountMax);
    }

    private boolean doMaintainTombstoneType(File dir, final String logSuffix, int logCountMax) {
        File[] files = dir.listFiles(new FilenameFilter() {
            @Override
            public boolean accept(File dir, String name) {
                return name.startsWith(Util.logPrefix + "_") && name.endsWith(logSuffix);
            }
        });

        boolean result = true;
        if (files != null && files.length > logCountMax) {
            if (logCountMax > 0) {
                Arrays.sort(files, new Comparator<File>() {
                    @Override
                    public int compare(File f1, File f2) {
                        return f1.getName().compareTo(f2.getName());
                    }
                });
            }
            for (int i = 0; i < files.length - logCountMax; i++) {
                if (!recycleLogFile(files[i])) {
                    result = false;
                }
            }
        }
        return result;
    }

    @SuppressWarnings("ResultOfMethodCallIgnored")
    private void doMaintainPlaceholder(File dir) {
        //get all existing placeholder files
        File[] cleanFiles = dir.listFiles(new FilenameFilter() {
            @Override
            public boolean accept(File dir, String name) {
                return name.startsWith(placeholderPrefix + "_") && name.endsWith(placeholderCleanSuffix);
            }
        });
        if (cleanFiles == null) {
            return;
        }
        File[] dirtyFiles = dir.listFiles(new FilenameFilter() {
            @Override
            public boolean accept(File dir, String name) {
                return name.startsWith(placeholderPrefix + "_") && name.endsWith(placeholderDirtySuffix);
            }
        });
        if (dirtyFiles == null) {
            return;
        }

        //create clean placeholder files from dirty placeholder files or new files
        int i = 0;
        int cleanFilesCount = cleanFiles.length;
        int dirtyFilesCount = dirtyFiles.length;
        while (cleanFilesCount < this.placeholderCountMax) {
            if (dirtyFilesCount > 0) {
                File dirtyFile = dirtyFiles[dirtyFilesCount - 1];
                if (cleanTheDirtyFile(dirtyFile)) {
                    cleanFilesCount++;
                }
                dirtyFilesCount--;
            } else {
                try {
                    File dirtyFile = new File(String.format(Locale.US, "%s/%s_%020d%s", logDir, placeholderPrefix, new Date().getTime() * 1000 + getNextUnique(), placeholderDirtySuffix));
                    if (dirtyFile.createNewFile()) {
                        if (cleanTheDirtyFile(dirtyFile)) {
                            cleanFilesCount++;
                        }
                    }
                } catch (Exception ignored) {
                }
            }

            //don't try it too many times
            if (++i > this.placeholderCountMax * 2) {
                break;
            }
        }

        //reload clean placeholder files list and dirty placeholder files list if needed
        if (i > 0) {
            cleanFiles = dir.listFiles(new FilenameFilter() {
                @Override
                public boolean accept(File dir, String name) {
                    return name.startsWith(placeholderPrefix + "_") && name.endsWith(placeholderCleanSuffix);
                }
            });
            dirtyFiles = dir.listFiles(new FilenameFilter() {
                @Override
                public boolean accept(File dir, String name) {
                    return name.startsWith(placeholderPrefix + "_") && name.endsWith(placeholderDirtySuffix);
                }
            });
        }

        //don't keep too many clean placeholder files
        if (cleanFiles != null && cleanFiles.length > this.placeholderCountMax) {
            for (i = 0; i < cleanFiles.length - this.placeholderCountMax; i++) {
                cleanFiles[i].delete();
            }
        }

        //delete all remaining dirty placeholder files
        if (dirtyFiles != null) {
            for (File dirtyFile : dirtyFiles) {
                dirtyFile.delete();
            }
        }
    }

    @SuppressWarnings("ResultOfMethodCallIgnored")
    private boolean cleanTheDirtyFile(File dirtyFile) {

        FileOutputStream stream = null;
        boolean succeeded = false;

        try {
            byte[] block = new byte[1024];
            Arrays.fill(block, (byte) 0);

            long blockCount = placeholderSizeKb;
            long dirtyFileSize = dirtyFile.length();
            if (dirtyFileSize > placeholderSizeKb * 1024) {
                blockCount = dirtyFileSize / 1024;
                if (dirtyFileSize % 1024 != 0) {
                    blockCount++;
                }
            }

            //clean the dirty file
            stream = new FileOutputStream(dirtyFile.getAbsoluteFile(), false);
            for (int i = 0; i < blockCount; i++) {
                if (i + 1 == blockCount && dirtyFileSize % 1024 != 0) {
                    //the last block
                    stream.write(block, 0, (int) (dirtyFileSize % 1024));
                } else {
                    stream.write(block);
                }
            }
            stream.flush();

            //rename the dirty file to clean file
            String newCleanFilePath = String.format(Locale.US, "%s/%s_%020d%s", logDir, placeholderPrefix, new Date().getTime() * 1000 + getNextUnique(), placeholderCleanSuffix);
            succeeded = dirtyFile.renameTo(new File(newCleanFilePath));
        } catch (Exception e) {
            XCrash.getLogger().e(Util.TAG, "FileManager cleanTheDirtyFile failed", e);
        } finally {
            if (stream != null) {
                try {
                    stream.close();
                } catch (Exception ignored) {
                }
            }
        }

        if (!succeeded) {
            try {
                dirtyFile.delete();
            } catch (Exception ignored) {
            }
        }

        return succeeded;
    }

    private int getNextUnique() {
        int i = unique.incrementAndGet();
        if (i >= 999) {
            unique.set(0);
        }
        return i;
    }
}
