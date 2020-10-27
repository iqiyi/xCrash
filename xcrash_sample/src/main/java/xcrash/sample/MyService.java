package xcrash.sample;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import xcrash.XCrash;

public class MyService extends Service {
    public MyService() {
    }

    @Override
    public void onCreate() {
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        String type = intent.getStringExtra("type");
        if (type != null) {
            if (type.equals("native")) {
                XCrash.testNativeCrash(false);
            } else if (type.equals("java")) {
                XCrash.testJavaCrash(false);
            }
        }
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
