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
}
