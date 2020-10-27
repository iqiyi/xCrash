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

/**
 * Define the callback interface after the crash occurs.
 *
 * <p>Note: Strictly speaking, this interface should always be implemented.
 * When disk is exhausted, the crash information cannot be saved to a crash log file.
 * The only way you can get the crash information is through the second parameter of onCrash() method.
 * Then you should parse and post the information immediately, because the APP process is crashing,
 * it will be killed by the system at any time.
 */
public interface ICrashCallback {

    /**
     * When a Java exception or native crash occurs, xCrash first captures and logs
     * the crash information to a crash log file and then calls this method.
     *
     * @param logPath Absolute path to the crash log file.
     * @param emergency A buffer that holds basic crash information when disk exhausted.
     * @throws Exception xCrash will catch and ignore any exception throw by this method.
     */
    @SuppressWarnings("unused")
    void onCrash(String logPath, String emergency) throws Exception;
}
