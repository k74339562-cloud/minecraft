package com.engine.minecraftclone;

import android.app.NativeActivity;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;

public class MainActivity extends NativeActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        enableEdgeToEdgeImmersive();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            enableEdgeToEdgeImmersive();
        }
    }

    private void enableEdgeToEdgeImmersive() {
        // 1. تجاوز النوتش والكاميرا الأمامية (تتمدد الشاشة تحت النوتش بالكامل)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }

        // 2. إخفاء أزرار التنقل وشريط الحالة مع وضع الـ Immersive Sticky
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION      // إخفاء أزرار الرجوع والهوم
            | View.SYSTEM_UI_FLAG_FULLSCREEN           // إخفاء شريط الإشعارات
        );
    }
}
