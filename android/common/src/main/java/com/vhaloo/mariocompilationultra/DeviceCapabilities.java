package com.vhaloo.mariocompilationultra;

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Build;
import android.os.PowerManager;
import android.view.InputDevice;

/** Local capabilities, not a list of vendor/model names. No extra permissions. */
public final class DeviceCapabilities {
    private DeviceCapabilities() {}
    private static volatile int thermalStatus;
    private static boolean listening;
    public static synchronized void initialize(Context context) {
        if(listening||Build.VERSION.SDK_INT<29)return;
        listening=true;
        PowerManager manager=context.getApplicationContext().getSystemService(PowerManager.class);
        if(manager!=null) {
            thermalStatus=manager.getCurrentThermalStatus();
            manager.addThermalStatusListener(context.getApplicationContext().getMainExecutor(),status->thermalStatus=status);
        }
    }
    public static boolean watch(Context context) {
        return context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_WATCH)
            ||(context.getResources().getConfiguration().uiMode&Configuration.UI_MODE_TYPE_MASK)==Configuration.UI_MODE_TYPE_WATCH;
    }
    public static boolean rotary() {
        for(int id:InputDevice.getDeviceIds()) {
            InputDevice device=InputDevice.getDevice(id);
            if(device!=null&&(device.getSources()&InputDevice.SOURCE_ROTARY_ENCODER)==InputDevice.SOURCE_ROTARY_ENCODER)return true;
        }
        return false;
    }
    public static int thermal(Context context) {
        return thermalStatus;
    }
}
