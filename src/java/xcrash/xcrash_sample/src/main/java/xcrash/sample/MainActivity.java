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

import android.content.Intent;
import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;

import xcrash.XCrash;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
    }

    public void testNativeCrashInMainThread_onClick(View view) {
        XCrash.testNativeCrash(false);
    }

    public void testNativeCrashInAnotherJavaThread_onClick(View view) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                XCrash.testNativeCrash(false);
            }
        }, "java_thread_with_a_very_long_name").start();
    }

    public void testNativeCrashInAnotherNativeThread_onClick(View view) {
        XCrash.testNativeCrash(true);
    }

    public void testNativeCrashInAnotherActivity_onClick(View view) {
        startActivity(new Intent(this, SecondActivity.class).putExtra("type", "native"));
    }

    public void testNativeCrashInAnotherProcess_onClick(View view) {
        startService(new Intent(this, MyService.class).putExtra("type", "native"));
    }

    public void testJavaCrashInMainThread_onClick(View view) {
        XCrash.testJavaCrash(false);
    }

    public void testJavaCrashInAnotherThread_onClick(View view) {
        XCrash.testJavaCrash(true);
    }

    public void testJavaCrashInAnotherActivity_onClick(View view) {
        startActivity(new Intent(this, SecondActivity.class).putExtra("type", "java"));
    }

    public void testJavaCrashInAnotherProcess_onClick(View view) {
        startService(new Intent(this, MyService.class).putExtra("type", "java"));
    }
    public void testAnrInput_onClick(View view) {
        while (true) {
            try {
                Thread.sleep(1000);
            } catch (Exception ignored) {
            }
        }
    }
}
