package com.vhaloo.mariocompilationultra.android;

import org.libsdl.app.SDLActivity;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.database.Cursor;
import android.graphics.Insets;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.provider.OpenableColumns;
import android.view.DisplayCutout;
import android.view.KeyEvent;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.window.OnBackInvokedDispatcher;
import android.widget.Toast;
import java.io.*;
import java.util.ArrayList;

/** Ordinary Android application: no headset, network or storage-wide permission. */
public final class HandheldActivity extends SDLActivity {
    private File root;
    private String preparationError;
    private volatile int insetLeft,insetTop,insetRight,insetBottom;
    private static final String[] LANGUAGES={"en","fr","es","ja","de","it","pt","ru","zh","ko"};
    private static final int IMPORT_ROM=73;
    @Override protected void onCreate(Bundle state) {
        root=getFilesDir();
        try {
            copyTree("mcu",root);
            File data=new File(root,"UserData");ensureDirectory(data);ensureDirectory(new File(root,"ROMs"));
            File settings=new File(data,"settings.ini");
            if(!settings.exists()) {
                int language=0;String system=java.util.Locale.getDefault().getLanguage();
                for(int i=0;i<LANGUAGES.length;i++)if(LANGUAGES[i].equals(system))language=i;
                try(Writer out=new OutputStreamWriter(new FileOutputStream(settings),"UTF-8")) {
                    out.write("language "+language+"\nview 4\ninset 1\nlocator 1\nminimap 0\nquality 0\nrewindMemory 0\n"
                        +"fullscreen 0\nspan 0\nvsync 1\nautoResume 1\ntouchMode 0\ntouchPlacement 0\n");
                }
            }
        } catch(IOException error) {preparationError="Could not prepare local game data: "+error.getMessage();}
        super.onCreate(state);
        if(Build.VERSION.SDK_INT>=33)getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
            OnBackInvokedDispatcher.PRIORITY_DEFAULT,this::sendBackToGame);
        getWindow().getDecorView().setOnApplyWindowInsetsListener((view,insets)->{readInsets(insets);return insets;});
        getWindow().getDecorView().requestApplyInsets();
    }
    private void sendBackToGame() {
        if(!mBrokenLibraries){SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_BACK);SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_BACK);}
    }
    // Legacy Android 8-12 dispatch; API 33+ uses the callback registered above.
    @android.annotation.SuppressLint("GestureBackNavigation")
    @Override public void onBackPressed(){sendBackToGame();}
    @android.annotation.SuppressLint("GestureBackNavigation") // Gestures use the registered callback; this path handles hardware keys only.
    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        if(event.getKeyCode()==KeyEvent.KEYCODE_BACK) {
            // Consume hardware/ADB Back before SDL's view listener and the platform
            // callback can both dispatch it. Gestures use OnBackInvokedCallback.
            if(event.getAction()==KeyEvent.ACTION_UP&&!event.isCanceled())sendBackToGame();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }
    private static void ensureDirectory(File path)throws IOException {
        if(!path.isDirectory()&&!path.mkdirs())throw new IOException("Cannot create "+path.getName());
    }
    private void copyTree(String source,File destination)throws IOException {
        String[] children=getAssets().list(source);if(children==null)return;
        ensureDirectory(destination);
        for(String child:children) {
            String asset=source+"/"+child;File target=new File(destination,child);
            if(!target.getCanonicalPath().startsWith(root.getCanonicalPath()+File.separator))throw new IOException("Invalid bundled path");
            String[] nested=getAssets().list(asset);
            if(nested!=null&&nested.length>0)copyTree(asset,target);
            else if(!target.exists()||asset.startsWith("mcu/assets/")||asset.startsWith("mcu/docs/")||asset.startsWith("mcu/licences/")) {
                try(InputStream in=getAssets().open(asset);OutputStream out=new FileOutputStream(target)) {
                    byte[] buffer=new byte[65536];int count;while((count=in.read(buffer))!=-1)out.write(buffer,0,count);
                }
            }
        }
    }
    private void readInsets(WindowInsets insets) {
        int l,t,r,b;
        if(Build.VERSION.SDK_INT>=30) {
            Insets v=insets.getInsets(WindowInsets.Type.systemBars()|WindowInsets.Type.displayCutout());
            l=v.left;t=v.top;r=v.right;b=v.bottom;
            Insets gestures=insets.getMandatorySystemGestureInsets();b=Math.max(b,gestures.bottom);
        } else {
            l=insets.getSystemWindowInsetLeft();t=insets.getSystemWindowInsetTop();r=insets.getSystemWindowInsetRight();b=insets.getSystemWindowInsetBottom();
            if(Build.VERSION.SDK_INT>=28) {
                DisplayCutout cutout=insets.getDisplayCutout();
                if(cutout!=null){l=Math.max(l,cutout.getSafeInsetLeft());t=Math.max(t,cutout.getSafeInsetTop());r=Math.max(r,cutout.getSafeInsetRight());b=Math.max(b,cutout.getSafeInsetBottom());}
            }
        }
        insetLeft=l;insetTop=t;insetRight=r;insetBottom=b;
    }
    public int[] getLayoutMetrics() {
        return new int[]{Math.round(getResources().getDisplayMetrics().density*1000),insetLeft,insetTop,insetRight,insetBottom};
    }
    @Override public void onConfigurationChanged(Configuration configuration) {
        super.onConfigurationChanged(configuration);getWindow().getDecorView().requestApplyInsets();
    }
    public void configurePresentation(int orientation,boolean awake) {
        runOnUiThread(()->{
            if(!isInMultiWindowMode()) {
                int wanted=orientation==1?ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT:orientation==2?ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE:ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR;
                if(getRequestedOrientation()!=wanted)setRequestedOrientation(wanted);
            }
            if(awake)getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            else getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        });
    }
    public void tapFeedback() {
        Vibrator vibrator=getSystemService(Vibrator.class);
        if(vibrator!=null&&vibrator.hasVibrator())vibrator.vibrate(VibrationEffect.createOneShot(10,50));
    }
    public void openGuide(String language,boolean mobile) {
        boolean allowed=false;for(String code:LANGUAGES)allowed|=code.equals(language);if(!allowed)language="en";
        final String code=language;runOnUiThread(()->startActivity(new Intent(this,GuideActivity.class).putExtra("language",code)));
    }
    @Override protected String[] getLibraries(){return new String[]{"c++_shared","SDL2","main"};}
    @Override protected String[] getArguments() {
        ArrayList<String> args=new ArrayList<>();
        args.add("--data-dir");args.add(new File(root,"UserData").getAbsolutePath());
        args.add("--import");args.add(new File(root,"ROMs").getAbsolutePath());
        String game=getIntent().getStringExtra("mcu_game");
        if(game!=null&&java.util.Arrays.asList("smb1","lostlevels","smb2","smb3","world","yoshi").contains(game)){args.add("--game");args.add(game);}
        String language=getIntent().getStringExtra("mcu_language");
        if(language!=null&&java.util.Arrays.asList(LANGUAGES).contains(language)){args.add("--locale");args.add(language);}
        if(getIntent().getBooleanExtra("mcu_menu_only",false))args.add("--menu-only");
        if(getIntent().getBooleanExtra("mcu_no_resume",false))args.add("--no-resume");
        if(getIntent().getBooleanExtra("mcu_start_paused",false))args.add("--menu");
        if(getIntent().getBooleanExtra("mcu_diagnostics",false)) {
            File folder=getExternalFilesDir("diagnostics");
            if(folder!=null){args.add("--live-report");args.add(new File(folder,"runtime.json").getAbsolutePath());}
        }
        if(preparationError!=null){args.add("--startup-error");args.add(preparationError);}
        return args.toArray(new String[0]);
    }
    public void chooseRom() {
        runOnUiThread(()->{
            Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT);intent.addCategory(Intent.CATEGORY_OPENABLE);intent.setType("*/*");startActivityForResult(intent,IMPORT_ROM);
        });
    }
    @Override protected void onActivityResult(int request,int result,Intent data) {
        super.onActivityResult(request,result,data);
        if(request!=IMPORT_ROM||result!=RESULT_OK||data==null||data.getData()==null)return;
        Uri uri=data.getData();
        new Thread(()->{
            File temporary=null;
            try {
                String name="cartridge.sfc";
                try(Cursor cursor=getContentResolver().query(uri,new String[]{OpenableColumns.DISPLAY_NAME},null,null,null)) {
                    if(cursor!=null&&cursor.moveToFirst())name=cursor.getString(0);
                }
                name=name.replaceAll("[\\\\/\\t\\r\\n]","_");String lower=name.toLowerCase(java.util.Locale.ROOT);
                if(!(lower.endsWith(".zip")||lower.endsWith(".sfc")||lower.endsWith(".smc")))throw new IOException("Choose a cartridge file or ZIP.");
                File folder=new File(root,"ROMs");ensureDirectory(folder);File target=new File(folder,name);
                if(target.exists())target=new File(folder,System.currentTimeMillis()+"-"+name);
                temporary=new File(target.getPath()+".importing");
                try(InputStream in=getContentResolver().openInputStream(uri);OutputStream out=new FileOutputStream(temporary)) {
                    if(in==null)throw new IOException("Cannot read this document.");byte[] buffer=new byte[65536];int count;long total=0;
                    while((count=in.read(buffer))!=-1){total+=count;if(total>32L*1024*1024)throw new IOException("Cartridge archive exceeds 32 MB.");out.write(buffer,0,count);}
                }
                if(!temporary.renameTo(target))throw new IOException("Cannot finish the cartridge copy.");temporary=null;SDLActivity.onNativeDropFile(target.getAbsolutePath());
            }catch(Exception error){if(temporary!=null)temporary.delete();String message=error.getMessage();runOnUiThread(()->Toast.makeText(this,message,Toast.LENGTH_LONG).show());}
        },"MCU-cartridge-import").start();
    }
}
