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

// Created by caikelun on 2019-09-25.
package xcrash;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;

import java.util.LinkedList;

class ActivityMonitor {

    private static final ActivityMonitor instance = new ActivityMonitor();

    private LinkedList<Activity> activities = null;
    private boolean isAppForeground = false;

    private static final int MAX_ACTIVITY_NUM = 100;

    private ActivityMonitor() {
    }

    static ActivityMonitor getInstance() {
        return instance;
    }

    void initialize(Application application) {
        activities = new LinkedList<Activity>();

        application.registerActivityLifecycleCallbacks(
            new Application.ActivityLifecycleCallbacks() {

                private int activityReferences = 0;
                private boolean isActivityChangingConfigurations = false;

                @Override
                public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
                    activities.addFirst(activity);
                    if (activities.size() > MAX_ACTIVITY_NUM) {
                        activities.removeLast();
                    }
                }

                @Override
                public void onActivityStarted(Activity activity) {
                    if (++activityReferences == 1 && !isActivityChangingConfigurations) {
                        isAppForeground = true;
                    }
                }

                @Override
                public void onActivityResumed(Activity activity) {
                }

                @Override
                public void onActivityPaused(Activity activity) {
                }

                @Override
                public void onActivityStopped(Activity activity) {
                    isActivityChangingConfigurations = activity.isChangingConfigurations();
                    if (--activityReferences == 0 && !isActivityChangingConfigurations) {
                        isAppForeground = false;
                    }
                }

                @Override
                public void onActivitySaveInstanceState(Activity activity, Bundle outState) {
                }

                @Override
                public void onActivityDestroyed(Activity activity) {
                    activities.remove(activity);
                }
            }
        );
    }

    void finishAllActivities() {
        if (activities != null) {
            for (Activity activity : activities) {
                activity.finish();
            }
            activities.clear();
        }
    }

    boolean isApplicationForeground() {
        return this.isAppForeground;
    }
}
