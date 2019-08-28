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

import android.text.TextUtils;

import java.io.File;
import java.io.FilenameFilter;
import java.util.Arrays;
import java.util.Comparator;

/**
 * Tombstone (crash) log file manager.
 */
@SuppressWarnings("unused")
public class TombstoneManager {

    private TombstoneManager() {
    }

    /**
     * Append a customs section (a key-content pair) to the crash log file. After parsing the crash log file
     * by {@link xcrash.TombstoneParser}, you can retrieve the content with the key specified here.
     *
     * <p>Note: This method is generally used in {@link xcrash.ICrashCallback#onCrash(String, String)}.
     *
     * <p>Warning: Do NOT include multiple consecutive newline characters ("\n\n") in the content string.
     * This will break the parsing rules.
     *
     * @param logPath Absolute path of the crash log file.
     * @param key Section key.
     * @param content Section content.
     * @return Return true if successful, false otherwise.
     */
    @SuppressWarnings({"unused", "UnusedReturnValue"})
    public static boolean appendSection(String logPath, String key, String content) {
        if (TextUtils.isEmpty(logPath) || TextUtils.isEmpty(key) || content == null) {
            return false;
        }

        return FileManager.getInstance().appendText(logPath, "\n\n" + key + ":\n" + content + "\n\n");
    }

    /**
     * Determines if the current log file recorded a Java exception.
     *
     * @param log Object of the log file.
     * @return Return true if YES, false otherwise.
     */
    @SuppressWarnings("unused")
    public static boolean isJavaCrash(File log) {
        return log.getName().endsWith(Util.javaLogSuffix);
    }

    /**
     * Determines if the current log file recorded a native crash.
     *
     * @param log Object of the log file.
     * @return Return true if YES, false otherwise.
     */
    @SuppressWarnings("unused")
    public static boolean isNativeCrash(File log) {
        return log.getName().endsWith(Util.nativeLogSuffix);
    }

    /**
     * Determines if the current log file recorded an ANR.
     *
     * @param log Object of the log file.
     * @return Return true if YES, false otherwise.
     */
    @SuppressWarnings("unused")
    public static boolean isAnr(File log) {
        return log.getName().endsWith(Util.anrLogSuffix);
    }

    /**
     * Get all Java exception log files.
     *
     * @return An array of File objects of the Java exception log files.
     */
    @SuppressWarnings("unused")
    public static File[] getJavaTombstones() {
        return getTombstones(new String[]{Util.javaLogSuffix});
    }

    /**
     * Get all native crash log files.
     *
     * @return An array of File objects of the native crash log files.
     */
    @SuppressWarnings("unused")
    public static File[] getNativeTombstones() {
        return getTombstones(new String[]{Util.nativeLogSuffix});
    }

    /**
     * Get all ANR log files.
     *
     * @return An array of File objects of the ANR log files.
     */
    @SuppressWarnings("unused")
    public static File[] getAnrTombstones() {
        return getTombstones(new String[]{Util.anrLogSuffix});
    }

    /**
     * Get all Java exception, native crash and ANR log files.
     *
     * @return An array of File objects of the Java exception, native crash and ANR log files.
     */
    @SuppressWarnings("unused")
    public static File[] getAllTombstones() {
        return getTombstones(new String[]{Util.javaLogSuffix, Util.nativeLogSuffix, Util.anrLogSuffix});
    }

    /**
     * Delete the tombstone file.
     *
     * <p>Note: When you use the placeholder file feature by {@link XCrash.InitParameters#setPlaceholderCountMax(int)},
     * please always use this method to delete tombstone files.
     *
     * @param file The tombstone file object.
     * @return Return true if successful, false otherwise.
     */
    public static boolean deleteTombstone(File file) {
        return FileManager.getInstance().recycleLogFile(file);
    }

    /**
     * Delete the tombstone file.
     *
     * <p>Note: When you use the placeholder file feature by {@link XCrash.InitParameters#setPlaceholderCountMax(int)},
     * please always use this method to delete tombstone files.
     *
     * @param path The path of the tombstone file.
     * @return Return true if successful, false otherwise.
     */
    public static boolean deleteTombstone(String path) {
        return FileManager.getInstance().recycleLogFile(new File(path));
    }

    /**
     * Delete all Java exception log files.
     *
     * @return Return true if successful, false otherwise.
     */
    @SuppressWarnings("unused")
    public static boolean clearJavaTombstones() {
        return clearTombstones(new String[]{Util.javaLogSuffix});
    }

    /**
     * Delete all native crash log files.
     *
     * @return Return true if successful, false otherwise.
     */
    @SuppressWarnings("unused")
    public static boolean clearNativeTombstones() {
        return clearTombstones(new String[]{Util.nativeLogSuffix});
    }

    /**
     * Delete all ANR log files.
     *
     * @return Return true if successful, false otherwise.
     */
    @SuppressWarnings("unused")
    public static boolean clearAnrTombstones() {
        return clearTombstones(new String[]{Util.anrLogSuffix});
    }

    /**
     * Delete all Java exception, native crash and ANR log files.
     *
     * @return Return true if successful, false otherwise.
     */
    @SuppressWarnings("unused")
    public static boolean clearAllTombstones() {
        return clearTombstones(new String[]{Util.javaLogSuffix, Util.nativeLogSuffix, Util.anrLogSuffix});
    }

    private static File[] getTombstones(final String[] logPrefixes) {
        String logDir = XCrash.getLogDir();
        if (logDir == null) {
            return new File[0];
        }

        File dir = new File(logDir);
        if (!dir.exists() || !dir.isDirectory()) {
            return new File[0];
        }

        File[] files = dir.listFiles(new FilenameFilter() {
            @Override
            public boolean accept(File dir, String name) {
                if (!name.startsWith(Util.logPrefix + "_")) {
                    return false;
                }
                for (String logPrefix : logPrefixes) {
                    if (name.endsWith(logPrefix)) {
                        return true;
                    }
                }
                return false;
            }
        });
        if (files == null) {
            return new File[0];
        }

        //sort
        Arrays.sort(files, new Comparator<File>() {
            @Override
            public int compare(File f1, File f2) {
                return f1.getName().compareTo(f2.getName());
            }
        });

        return files;
    }

    private static boolean clearTombstones(final String[] logPrefixes) {
        String logDir = XCrash.getLogDir();
        if (logDir == null) {
            return false;
        }

        File dir = new File(logDir);
        if (!dir.exists() || !dir.isDirectory()) {
            return false;
        }

        File[] files = dir.listFiles(new FilenameFilter() {
            @Override
            public boolean accept(File dir, String name) {
                if (!name.startsWith(Util.logPrefix + "_")) {
                    return false;
                }
                for (String logPrefix : logPrefixes) {
                    if (name.endsWith(logPrefix)) {
                        return true;
                    }
                }
                return false;
            }
        });
        if (files == null) {
            return false;
        }

        boolean success = true;
        for (File f : files) {
            if (!FileManager.getInstance().recycleLogFile(f)) {
                success = false;
            }
        }

        return success;
    }
}
