package xcrash.sample;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.os.Bundle;

import xcrash.XCrash;

public class SecondActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_second);

        Intent intent = getIntent();
        String type = intent.getStringExtra("type");
        if (type != null) {
            if (type.equals("native")) {
                XCrash.testNativeCrash(false);
            } else if (type.equals("java")) {
                XCrash.testJavaCrash(false);
            }
        }
    }
}
