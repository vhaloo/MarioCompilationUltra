package com.vhaloo.mariocompilationultra;

import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.widget.Toast;
import org.libsdl.app.SDLActivity;
import java.io.*;
import java.net.URL;
import java.util.Arrays;
import javax.net.ssl.HttpsURLConnection;

/** Shared by the headset, handheld and Wear editions. Network work runs off the UI thread. */
public abstract class UpdateActivity extends SDLActivity {
    private static final int ALLOW_UPDATE = 8912;
    private String pendingVersion, updateLanguage = "en";

    public boolean fetchSoftwareUpdate(String address, String output, long limit) {
        boolean catalog = address.equals("https://api.github.com/repos/vhaloo/MarioCompilationUltra/releases?per_page=12");
        if (!catalog && !address.startsWith("https://github.com/vhaloo/MarioCompilationUltra/releases/download/v")) return false;
        if (limit < 1 || limit > 96L * 1024 * 1024) return false;
        try {
            URL url = new URL(address);
            for (int hops = 0; hops < 6; hops++) {
                if (!"https".equals(url.getProtocol())) return false;
                HttpsURLConnection c = (HttpsURLConnection) url.openConnection();
                try {
                    c.setRequestProperty("User-Agent","MarioCompilationUltra-Updater/1");
                    c.setConnectTimeout(5000); c.setReadTimeout(catalog ? 6000 : 15000);
                    c.setInstanceFollowRedirects(false);
                    int status = c.getResponseCode();
                    if (!catalog && status >= 300 && status <= 399) {
                        String next = c.getHeaderField("Location");
                        if (next == null) return false;
                        url = new URL(url, next); continue;
                    }
                    if (status != 200 || c.getContentLengthLong() > limit) return false;
                    try (InputStream in = c.getInputStream(); OutputStream out = new FileOutputStream(output)) {
                        byte[] buffer = new byte[32768]; long total = 0; int n;
                        while ((n = in.read(buffer)) != -1) {
                            total += n; if (total > limit) return false;
                            out.write(buffer,0,n);
                        }
                        return true;
                    }
                } finally { c.disconnect(); }
            }
        } catch (IOException | RuntimeException unavailable) { return false; }
        return false;
    }

    @SuppressWarnings("deprecation")
    private boolean samePublisher(File apk) throws PackageManager.NameNotFoundException {
        PackageManager pm = getPackageManager();
        PackageInfo old = pm.getPackageInfo(getPackageName(), PackageManager.GET_SIGNATURES);
        PackageInfo next = pm.getPackageArchiveInfo(apk.getPath(), PackageManager.GET_SIGNATURES);
        return next != null && old.packageName.equals(next.packageName) && next.versionCode > old.versionCode
            && old.signatures != null && old.signatures.length > 0 && Arrays.equals(old.signatures,next.signatures);
    }
    public boolean installSoftwareUpdate(String path, String language) {
        try {
            File apk = new File(path).getCanonicalFile();
            File parent = apk.getParentFile();
            if (parent == null) return false;
            String version = parent.getName();
            // Android may expose the same app directory through /data/user/0 and /data/data.
            File expected = UpdateFileProvider.archive(this,version).getCanonicalFile();
            if (!apk.equals(expected) || !samePublisher(apk)) return false;
            runOnUiThread(() -> {
                pendingVersion = version; updateLanguage = language;
                try {
                    if (getPackageManager().canRequestPackageInstalls()) openInstaller();
                    else startActivityForResult(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                        Uri.parse("package:" + getPackageName())), ALLOW_UPDATE);
                } catch (ActivityNotFoundException | SecurityException failure) { failed(); }
            });
            return true;
        } catch (IOException | PackageManager.NameNotFoundException | RuntimeException failure) { return false; }
    }
    private void openInstaller() {
        Uri uri = new Uri.Builder().scheme("content").authority(getPackageName()+".updates")
            .appendPath("update").appendPath(pendingVersion).build();
        Intent intent = new Intent(Intent.ACTION_VIEW).setDataAndType(uri,UpdateFileProvider.MIME)
            .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        startActivity(intent); pendingVersion = null;
    }
    private void failed() {
        pendingVersion = null;
        Toast.makeText(this,LocalText.text(this,updateLanguage,"Update could not be installed."),Toast.LENGTH_LONG).show();
    }
    @Override protected void onSaveInstanceState(Bundle out) {
        super.onSaveInstanceState(out); out.putString("mcuUpdateVersion", pendingVersion); out.putString("mcuUpdateLanguage",updateLanguage);
    }
    @Override protected void onCreate(Bundle state) {
        if (state != null) {pendingVersion=state.getString("mcuUpdateVersion");updateLanguage=state.getString("mcuUpdateLanguage","en");}
        super.onCreate(state);
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request,result,data);
        if (request == ALLOW_UPDATE && pendingVersion != null) {
            try {
                if (getPackageManager().canRequestPackageInstalls()) openInstaller();
                else failed();
            } catch (ActivityNotFoundException | SecurityException error) { failed(); }
        }
    }
}
