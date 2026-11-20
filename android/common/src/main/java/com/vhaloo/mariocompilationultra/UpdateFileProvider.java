package com.vhaloo.mariocompilationultra;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import java.io.File;
import java.io.FileNotFoundException;
import java.util.List;

/** Grants the system installer temporary read access to exactly one public release APK. */
public final class UpdateFileProvider extends ContentProvider {
    static final String MIME="application/vnd.android.package-archive";
    static File archive(Context context,String version) {
        if(version==null || !version.matches("(0|[1-9][0-9]{0,5})(\\.(0|[1-9][0-9]{0,5})){2}")) throw new IllegalArgumentException("Invalid version");
        String id=context.getPackageName();
        String edition=id.endsWith(".wear")?"Wear":id.endsWith(".quest")?"Quest":"Android";
        return new File(context.getFilesDir(),"UserData/.mcu-updates/"+edition+"/"+version+"/MarioCompilationUltra-"+version+"-"+edition+"-public.apk");
    }
    private File resolve(Uri uri) {
        List<String> segments=uri.getPathSegments();
        if(!("content".equals(uri.getScheme()) && (getContext().getPackageName()+".updates").equals(uri.getAuthority())
            && segments.size()==2 && "update".equals(segments.get(0)))) throw new IllegalArgumentException("Invalid update URI");
        return archive(getContext(),segments.get(1));
    }
    @Override public boolean onCreate(){return true;}
    @Override public String getType(Uri uri){resolve(uri);return MIME;}
    @Override public ParcelFileDescriptor openFile(Uri uri,String mode) throws FileNotFoundException {
        if(!"r".equals(mode))throw new SecurityException("Read only");
        return ParcelFileDescriptor.open(resolve(uri),ParcelFileDescriptor.MODE_READ_ONLY);
    }
    @Override public Cursor query(Uri uri,String[] projection,String selection,String[] args,String order) {
        File file=resolve(uri);
        String[] columns=projection==null?new String[]{OpenableColumns.DISPLAY_NAME,OpenableColumns.SIZE}:projection;
        MatrixCursor result=new MatrixCursor(columns);MatrixCursor.RowBuilder row=result.newRow();
        for(String column:columns) row.add(OpenableColumns.DISPLAY_NAME.equals(column)?file.getName():OpenableColumns.SIZE.equals(column)?Long.valueOf(file.length()):null);
        return result;
    }
    @Override public Uri insert(Uri uri,ContentValues v){throw new UnsupportedOperationException();}
    @Override public int delete(Uri uri,String where,String[] args){throw new UnsupportedOperationException();}
    @Override public int update(Uri uri,ContentValues v,String where,String[] args){throw new UnsupportedOperationException();}
}
