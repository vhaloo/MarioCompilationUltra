package com.vhaloo.mariocompilationultra.android;
import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.webkit.WebResourceRequest;
import android.webkit.WebView;
import android.webkit.WebViewClient;

/** Offline guide in the app UID, without exposing private file URLs to another app. */
public final class GuideActivity extends Activity {
    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        String language=getIntent().getStringExtra("language");
        if(language==null||!java.util.Arrays.asList("en","fr","es","ja","de","it","pt","ru","zh","ko").contains(language))language="en";
        if(com.vhaloo.mariocompilationultra.DeviceCapabilities.watch(this)){watchGuide(language);return;}
        WebView view=new WebView(this);view.setBackgroundColor(0xff0d1620);view.getSettings().setAllowFileAccess(false);
        view.setWebViewClient(new WebViewClient(){
            @Override public boolean shouldOverrideUrlLoading(WebView web,WebResourceRequest request) {
                Uri uri=request.getUrl();String scheme=uri.getScheme();
                if("https".equals(scheme)||"http".equals(scheme)){startActivity(new Intent(Intent.ACTION_VIEW,uri));return true;}
                return !"file".equals(scheme);
            }
        });
        setContentView(view);view.loadUrl("file:///android_asset/mcu/docs/"+language+"/mobile.html");
    }
    private void watchGuide(String language) {
        android.widget.ScrollView scroll=new android.widget.ScrollView(this);
        android.widget.TextView text=new android.widget.TextView(this);
        text.setTextColor(0xffe7f4ef);text.setTextSize(14);text.setPadding(12,12,12,12);
        text.setTypeface(android.graphics.Typeface.createFromAsset(getAssets(),"mcu/assets/Interface.otf"));
        try(java.io.InputStream in=getAssets().open("mcu/docs/"+language+"/watch.txt");java.io.ByteArrayOutputStream bytes=new java.io.ByteArrayOutputStream()) {
            byte[] buffer=new byte[4096];int read;while((read=in.read(buffer))!=-1)bytes.write(buffer,0,read);
            text.setText(bytes.toString("UTF-8"));
        }catch(java.io.IOException error){text.setText(com.vhaloo.mariocompilationultra.LocalText.text(this,language,"The offline guide could not be opened."));}
        scroll.addView(text);scroll.setBackgroundColor(0xff0d1620);scroll.setFocusableInTouchMode(true);scroll.requestFocus();
        scroll.setOnGenericMotionListener((view,event)->{
            if(event.getAction()==android.view.MotionEvent.ACTION_SCROLL&&event.isFromSource(android.view.InputDevice.SOURCE_ROTARY_ENCODER)) {
                scroll.smoothScrollBy(0,Math.round(-event.getAxisValue(android.view.MotionEvent.AXIS_SCROLL)*android.view.ViewConfiguration.get(this).getScaledVerticalScrollFactor()));return true;
            }return false;
        });
        android.widget.FrameLayout root=new android.widget.FrameLayout(this);root.setBackgroundColor(0xff0d1620);
        android.widget.FrameLayout.LayoutParams box=new android.widget.FrameLayout.LayoutParams(-1,-1);
        int padding=Math.round(30*getResources().getDisplayMetrics().density);box.setMargins(padding,padding,padding,padding);
        root.addView(scroll,box);setContentView(root);
    }
}
