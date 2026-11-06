package com.vhaloo.mariocompilationultra;
import android.content.Context;
import java.io.*;
import java.util.Arrays;
import org.json.JSONObject;
/** Native Android messages use the same ten catalogs as the game UI. */
public final class LocalText {
    private LocalText() {}
    public static String text(Context context,String language,String english) {
        if(!Arrays.asList("en","fr","es","ja","de","it","pt","ru","zh","ko").contains(language))language="en";
        try(InputStream input=context.getAssets().open("mcu/assets/locales/"+language+".json");ByteArrayOutputStream bytes=new ByteArrayOutputStream()) {
            byte[] buffer=new byte[4096];int count;while((count=input.read(buffer))!=-1)bytes.write(buffer,0,count);
            return new JSONObject(bytes.toString("UTF-8")).optString(english,english);
        }catch(Exception unavailable){return english;}
    }
}
