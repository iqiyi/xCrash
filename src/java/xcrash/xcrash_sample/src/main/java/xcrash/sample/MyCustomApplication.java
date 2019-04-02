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
package xcrash.sample;

import android.app.Application;
import android.content.Context;
import android.util.Log;

import org.json.JSONObject;

import java.io.File;
import java.io.FileWriter;
import java.util.Map;

import xcrash.TombstoneManager;
import xcrash.TombstoneParser;
import xcrash.XCrash;
import xcrash.ICrashCallback;

public class MyCustomApplication extends Application {
    private final String TAG = "xcrash_sample";

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);

        //The callback when App process crashed.
        ICrashCallback callback = new ICrashCallback() {
            @Override
            public void onCrash(String logPath, String emergency) {
                debug(logPath, emergency);

                if (emergency != null) {
                    //Disk is exhausted, send crash report immediately.
                    sendThenDeleteCrashLog(logPath, emergency);
                } else {
                    //Add some expanded sections. Send crash report at the next time APP startup.

                    //OK
                    TombstoneManager.appendSection(logPath, "expanded_key_1", "expanded_content");
                    TombstoneManager.appendSection(logPath, "expanded_key_2", "expanded_content_row_1\nexpanded_content_row_2");

                    //Invalid. (Do NOT include multiple consecutive newline characters ("\n\n") in the content string.)
                    //TombstoneManager.appendSection(logPath, "expanded_key_3", "expanded_content_row_1\n\nexpanded_content_row_2");
                }
            }
        };

        //Initialize xCrash.
        XCrash.init(this, new XCrash.InitParameters()
                .setAppVersion("1.2.3-beta456-patch789")
                .setJavaDumpAllThreadsWhiteList(new String[]{"^main$", "^Binder:.*", ".*Finalizer.*"})
                .setJavaDumpAllThreadsCountMax(10)
                .setNativeDumpAllThreadsWhiteList(new String[]{"^xcrash\\.sample$", "^Signal Catcher$", "^Jit thread pool$", ".*(R|r)ender.*"})
                .setNativeDumpAllThreadsCountMax(10)
                .setJavaCallback(callback)
                .setNativeCallback(callback));

        //Send all padding crash log files.
        new Thread(new Runnable() {
            @Override
            public void run() {
                for(File file : TombstoneManager.getAllTombstones()) {
                    sendThenDeleteCrashLog(file.getAbsolutePath(), null);
                }
            }
        }).start();
    }

    private void sendThenDeleteCrashLog(String logPath, String emergency) {
        Map<String, String> map = TombstoneParser.parse(logPath, emergency);
        String crashReport = new JSONObject(map).toString();

        //Send the crash report to server-side.
        //......

        //If the server-side receives successfully, delete the log file.
        //new File(logPath).delete();
    }

    private void debug(String logPath, String emergency) {
        Log.d(TAG, "logPath: " + (logPath != null ? logPath : "(null)") + ", emergency: " + (emergency != null ? emergency : "(null)"));

        //Parse and save the crash info to a JSON file for debugging.
        FileWriter writer = null;
        try {
            File debug = new File(getApplicationContext().getFilesDir() + "/tombstones/debug.json");
            debug.createNewFile();
            writer = new FileWriter(debug, false);
            writer.write(new JSONObject(TombstoneParser.parse(logPath, emergency)).toString());
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
}
