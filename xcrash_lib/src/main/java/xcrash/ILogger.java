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

// Created by caikelun on 2019-05-30.
package xcrash;

/**
 * Define the logger interface used by xCrash.
 */
public interface ILogger {

    /**
     * Log a VERBOSE message.
     *
     * @param tag Used to identify the source of a log message.
     * @param msg The message you would like logged.
     */
    @SuppressWarnings("unused")
    void v(String tag, String msg);

    /**
     * Log a VERBOSE message and the exception.
     *
     * @param tag Used to identify the source of a log message.
     * @param msg The message you would like logged.
     * @param tr An exception to log.
     */
    @SuppressWarnings("unused")
    void v(String tag, String msg, Throwable tr);

    /**
     * Log a DEBUG message.
     *
     * @param tag Used to identify the source of a log message.
     * @param msg The message you would like logged.
     */
    @SuppressWarnings("unused")
    void d(String tag, String msg);

    /**
     * Log a DEBUG message and the exception.
     *
     * @param tag Used to identify the source of a log message.
     * @param msg The message you would like logged.
     * @param tr An exception to log.
     */
    @SuppressWarnings("unused")
    void d(String tag, String msg, Throwable tr);

    /**
     * Log a INFO message.
     *
     * @param tag Used to identify the source of a log message.
     * @param msg The message you would like logged.
     */
    @SuppressWarnings("unused")
    void i(String tag, String msg);

    /**
     * Log a INFO message and the exception.
     *
     * @param tag Used to identify the source of a log message.
     * @param msg The message you would like logged.
     * @param tr An exception to log.
     */
    @SuppressWarnings("unused")
    void i(String tag, String msg, Throwable tr);

    /**
     * Log a WARN message.
     *
     * @param tag Used to identify the source of a log message.
     * @param msg The message you would like logged.
     */
    @SuppressWarnings("unused")
    void w(String tag, String msg);

    /**
     * Log a WARN message and the exception.
     *
     * @param tag Used to identify the source of a log message.
     * @param msg The message you would like logged.
     * @param tr An exception to log.
     */
    @SuppressWarnings("unused")
    void w(String tag, String msg, Throwable tr);

    /**
     * Log a ERROR message.
     *
     * @param tag Used to identify the source of a log message.
     * @param msg The message you would like logged.
     */
    @SuppressWarnings("unused")
    void e(String tag, String msg);

    /**
     * Log a ERROR message and the exception.
     *
     * @param tag Used to identify the source of a log message.
     * @param msg The message you would like logged.
     * @param tr An exception to log.
     */
    @SuppressWarnings("unused")
    void e(String tag, String msg, Throwable tr);
}
