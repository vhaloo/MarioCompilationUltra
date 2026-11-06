package com.vhaloo.mariocompilationultra;

import android.content.Context;
import java.io.*;
import java.util.*;

/** Optional ADB/file-manager inbox. Copy to private storage; preserve both originals. */
public final class CartridgeInbox {
    private CartridgeInbox() {}
    public static List<File> collect(Context context,File root)throws IOException {
        ArrayList<File> imported=new ArrayList<>();
        File external=context.getExternalFilesDir(null);if(external==null)return imported;
        File inbox=new File(external,"CartridgeInbox");
        if(!inbox.isDirectory()&&!inbox.mkdirs())return imported;
        File[] incoming=inbox.listFiles();if(incoming==null)return imported;
        File roms=new File(root,"ROMs");if(!roms.isDirectory()&&!roms.mkdirs())throw new IOException("Cannot create cartridge storage");
        Arrays.sort(incoming,Comparator.comparing(File::getName));int considered=0;
        for(File source:incoming) {
            String name=source.getName(),lower=name.toLowerCase(Locale.ROOT);
            if(!source.isFile()||!source.getCanonicalFile().getParentFile().equals(inbox.getCanonicalFile())
                ||!(lower.endsWith(".sfc")||lower.endsWith(".smc")||lower.endsWith(".zip")))continue;
            if(++considered>32)break;
            if(source.length()<=0||source.length()>32L*1024*1024)continue;
            File target=new File(roms,name);if(target.exists())continue;
            File temporary=new File(target.getPath()+".importing");
            try {
                try(InputStream input=new FileInputStream(source);OutputStream output=new FileOutputStream(temporary)) {
                    byte[] buffer=new byte[65536];int count;long total=0;
                    while((count=input.read(buffer))!=-1){total+=count;if(total>32L*1024*1024)throw new IOException("Cartridge exceeds 32 MB");output.write(buffer,0,count);}
                }
                if(!temporary.renameTo(target))throw new IOException("Cannot finish cartridge transfer");
                imported.add(target);
            }finally{if(temporary.exists())temporary.delete();}
        }
        return imported;
    }
}
