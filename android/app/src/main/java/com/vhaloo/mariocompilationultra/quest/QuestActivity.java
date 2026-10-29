package com.vhaloo.mariocompilationultra.quest;

import org.libsdl.app.SDLActivity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.widget.Toast;
import java.io.*;

/** Local-only storage and Android's document picker. No camera frames or network access. */
public final class QuestActivity extends SDLActivity {
    private File root;
    private String preparationError;
    private static final int IMPORT_ROM = 73;

    @Override protected void onCreate(Bundle state) {
        // Runtime files belong to the app UID. Android 14 scoped-storage rules can
        // reject native stat/open on an external directory populated by ADB.
        root = getFilesDir();
        try {
            File old = getExternalFilesDir(null);
            File migration = new File(root, "external-data-migration-0400.done");
            if (!migration.exists() && old != null && !old.equals(root)) {
                copyExisting(new File(old, "UserData"), new File(root, "UserData"));
                copyExisting(new File(old, "ROMs"), new File(root, "ROMs"));
                try (Writer marker = new FileWriter(migration)) { marker.write("Existing readable external data preserved.\n"); }
            }
            copyTree("mcu", root);
            File data = new File(root, "UserData");
            ensureDirectory(data);
            ensureDirectory(new File(root, "ROMs"));
            File settings = new File(data, "settings.ini");
            if (!settings.exists()) {
                try (Writer out = new OutputStreamWriter(new FileOutputStream(settings), "UTF-8")) {
                    String[] languages = {"en","fr","es","ja","de","it","pt","ru","zh","ko"};
                    int language = 0;
                    String system = java.util.Locale.getDefault().getLanguage();
                    for (int i=0;i<languages.length;i++) if (languages[i].equals(system)) language=i;
                    out.write("language " + language + "\nview 4\nquality 0\nbackground 0\nblur 12\n"
                        + "fullscreen 0\nvsync 0\nrewind 1\nrewindMemory 0\n"
                        + "questResolution 1\nquestMode 2\nquestPassthrough 1\nquestTransparent 1\n"
                        + "questWallHeight 200\nquestScale 100\nquestRefresh 90\n"
                        + "questWallLocked 1\nquestCompanion 1\n");
                }
            }
        } catch (IOException failure) {
            // SDL still owns Activity initialization; native startup will report inaccessible data.
            android.util.Log.e("MCU", "Could not prepare local data", failure);
            preparationError = "Could not prepare private app storage: " + failure.getMessage();
        }
        super.onCreate(state);
    }
    private void copyExisting(File source, File destination) {
        if (!source.canRead()) return;
        try {
            if (source.isDirectory()) {
                ensureDirectory(destination);
                File[] children = source.listFiles();
                if (children != null) for (File child : children)
                    copyExisting(child, new File(destination, child.getName()));
            } else if (!destination.exists() && source.length() <= 64L * 1024 * 1024) {
                try (InputStream in = new FileInputStream(source); OutputStream out = new FileOutputStream(destination)) {
                    byte[] buffer = new byte[65536]; int count;
                    while ((count = in.read(buffer)) != -1) out.write(buffer, 0, count);
                }
            }
        } catch (IOException failure) {
            android.util.Log.w("MCU", "External data kept in place; APK assets remain available", failure);
        }
    }
    private static void ensureDirectory(File path) throws IOException {
        if (!path.isDirectory() && !path.mkdirs()) throw new IOException("Cannot create " + path.getName());
    }
    private void copyTree(String source, File destination) throws IOException {
        String[] children = getAssets().list(source);
        if (children == null) return;
        ensureDirectory(destination);
        for (String child : children) {
            String asset = source + "/" + child;
            File target = new File(destination, child);
            if (!target.getCanonicalPath().startsWith(root.getCanonicalPath() + File.separator))
                throw new IOException("Invalid bundled path");
            String[] nested = getAssets().list(asset);
            if (nested != null && nested.length > 0) copyTree(asset, target);
            else if (!target.exists() || asset.startsWith("mcu/assets/") || asset.startsWith("mcu/licences/") || asset.startsWith("mcu/docs/")) {
                try (InputStream in = getAssets().open(asset); OutputStream out = new FileOutputStream(target)) {
                    byte[] buffer = new byte[65536]; int count;
                    while ((count = in.read(buffer)) != -1) out.write(buffer, 0, count);
                }
            }
        }
    }
    @Override protected String[] getLibraries() { return new String[]{"c++_shared", "openxr_loader", "SDL2", "main"}; }
    @Override protected String[] getArguments() {
        java.util.ArrayList<String> args = new java.util.ArrayList<>();
        args.add("--data-dir"); args.add(new File(root, "UserData").getAbsolutePath());
        args.add("--import"); args.add(new File(root, "ROMs").getAbsolutePath());
        if (getIntent().getBooleanExtra("mcu_menu_only", false)) {
            args.add("--menu-only");
            if (getIntent().hasExtra("mcu_preview_mode")) {
                args.add("--xr-preview"); args.add(Integer.toString(getIntent().getIntExtra("mcu_preview_mode", 2)));
            }
            if (getIntent().getBooleanExtra("mcu_options", false)) args.add("--xr-options");
            String language = getIntent().getStringExtra("mcu_language");
            if ("en".equals(language) || "fr".equals(language) || "es".equals(language) || "ja".equals(language)) {
                args.add("--locale"); args.add(language);
            }
            if (getIntent().getBooleanExtra("mcu_capture", false)) {
                File folder = getExternalFilesDir("diagnostics");
                if (folder != null) {
                    args.add("--xr-capture"); args.add(new File(folder, "menu-scene.bmp").getAbsolutePath());
                }
            }
        }
        if (preparationError != null) { args.add("--startup-error"); args.add(preparationError); }
        return args.toArray(new String[0]);
    }

    public void chooseRom() {
        runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE); intent.setType("*/*");
            startActivityForResult(intent, IMPORT_ROM);
        });
    }
    public boolean scenePermissionGranted() {
        return checkSelfPermission("com.oculus.permission.USE_SCENE") == PackageManager.PERMISSION_GRANTED;
    }
    public void requestScenePermission() {
        runOnUiThread(() -> {
            if (!scenePermissionGranted())
                requestPermissions(new String[]{"com.oculus.permission.USE_SCENE"}, 74);
        });
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != IMPORT_ROM || result != RESULT_OK || data == null || data.getData() == null) return;
        Uri uri = data.getData();
        new Thread(() -> {
            File temporary = null;
            try {
                String name = "cartridge.sfc";
                try (Cursor cursor = getContentResolver().query(uri, new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
                    if (cursor != null && cursor.moveToFirst()) name = cursor.getString(0);
                }
                name = name.replaceAll("[\\\\/\\t\\r\\n]", "_");
                String lower = name.toLowerCase(java.util.Locale.ROOT);
                if (!(lower.endsWith(".zip") || lower.endsWith(".sfc") || lower.endsWith(".smc")))
                    throw new IOException("Choose a cartridge file or ZIP.");
                File folder = new File(root, "ROMs"); ensureDirectory(folder);
                File target = new File(folder, name);
                if (target.exists()) target = new File(folder, System.currentTimeMillis() + "-" + name);
                temporary = new File(target.getPath() + ".importing");
                try (InputStream in = getContentResolver().openInputStream(uri); OutputStream out = new FileOutputStream(temporary)) {
                    if (in == null) throw new IOException("Cannot read this document.");
                    byte[] buffer = new byte[65536]; int count; long total = 0;
                    while ((count = in.read(buffer)) != -1) {
                        total += count;
                        if (total > 32L * 1024 * 1024) throw new IOException("Cartridge archive exceeds 32 MB.");
                        out.write(buffer, 0, count);
                    }
                }
                if (!temporary.renameTo(target)) throw new IOException("Cannot finish the cartridge copy.");
                temporary = null;
                SDLActivity.onNativeDropFile(target.getAbsolutePath());
            } catch (Exception failure) {
                if (temporary != null) temporary.delete();
                final String message = failure.getMessage();
                runOnUiThread(() -> Toast.makeText(this, message, Toast.LENGTH_LONG).show());
            }
        }, "MCU-cartridge-import").start();
    }
}
