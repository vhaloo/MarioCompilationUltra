package com.vhaloo.mariocompilationultra;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.SystemClock;
import android.util.AtomicFile;
import android.util.Log;
import java.io.*;
import java.nio.charset.StandardCharsets;

/** Refresh static guides once per APK install; never replace existing owner data. */
public final class AssetPreparation {
    private AssetPreparation() {}

    @SuppressWarnings("deprecation") // Android 8 uses the integer version code.
    public static void prepare(Context context, File directory) throws IOException {
        long began = SystemClock.elapsedRealtime();
        File root = directory.getCanonicalFile();
        ensureDirectory(root);
        String version;
        try {
            PackageInfo info = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
            long code = Build.VERSION.SDK_INT >= 28 ? info.getLongVersionCode() : info.versionCode;
            version = "2/" + code + "/" + info.lastUpdateTime;
        } catch (PackageManager.NameNotFoundException failure) {
            throw new IOException("Cannot identify this app installation", failure);
        }
        AtomicFile stamp = new AtomicFile(new File(root, ".mcu-assets-ready"));
        String previous = "";
        try (InputStream in = stamp.openRead()) {
            byte[] bytes = new byte[192];
            int length = in.read(bytes);
            if (length > 0) previous = new String(bytes, 0, length, StandardCharsets.UTF_8);
        } catch (IOException ignored) { /* A first or interrupted install needs a refresh. */ }
        boolean refresh = !version.equals(previous)
                || !new File(root, "assets/Interface.otf").isFile()
                || !new File(root, "docs/en/guide.html").isFile();
        if (refresh) stamp.delete(); // An interrupted repair must retry all static files.
        copyTree(context, root, "mcu", root, refresh);
        if (refresh) {
            FileOutputStream out = null;
            try {
                out = stamp.startWrite();
                out.write(version.getBytes(StandardCharsets.UTF_8));
                stamp.finishWrite(out);
            } catch (IOException failure) {
                stamp.failWrite(out);
                throw failure;
            }
        }
        Log.i("MCU", "Assets " + (refresh ? "refreshed" : "reused") + " in "
                + (SystemClock.elapsedRealtime() - began) + " ms");
    }

    private static void ensureDirectory(File path) throws IOException {
        if (!path.isDirectory() && !path.mkdirs()) throw new IOException("Cannot create bundled directory");
    }
    private static boolean staticTree(String path) {
        return path.equals("mcu/assets") || path.equals("mcu/docs") || path.equals("mcu/licences");
    }
    private static void copyTree(Context context, File root, String source, File destination,
                                 boolean refresh) throws IOException {
        if (!refresh && staticTree(source)) return;
        String[] children = context.getAssets().list(source);
        if (children == null) return;
        ensureDirectory(destination);
        String prefix = root.getPath() + File.separator;
        for (String child : children) {
            String asset = source + "/" + child;
            File target = new File(destination, child);
            if (!target.getCanonicalPath().startsWith(prefix)) throw new IOException("Invalid bundled path");
            String[] nested = context.getAssets().list(asset);
            if (nested != null && nested.length > 0) copyTree(context, root, asset, target, refresh);
            else if (!target.exists() || asset.startsWith("mcu/assets/")
                    || asset.startsWith("mcu/docs/") || asset.startsWith("mcu/licences/")) {
                File temporary = new File(target.getPath() + ".installing");
                if (!temporary.getCanonicalPath().startsWith(prefix)) throw new IOException("Invalid bundled path");
                try (InputStream in = context.getAssets().open(asset);
                     OutputStream out = new FileOutputStream(temporary)) {
                    byte[] buffer = new byte[65536]; int count;
                    while ((count = in.read(buffer)) != -1) out.write(buffer, 0, count);
                }
                if (!temporary.renameTo(target)) throw new IOException("Cannot finish bundled file");
            }
        }
    }
}
