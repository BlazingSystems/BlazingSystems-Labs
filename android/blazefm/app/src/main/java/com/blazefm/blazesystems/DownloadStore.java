package com.blazefm.blazesystems;

import android.annotation.TargetApi;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;
import java.io.*;

final class DownloadStore {
    interface Writer { void write(OutputStream out)throws Exception; }
    private DownloadStore(){}

    static String save(Context c,String folder,String name,String mime,Writer writer)throws Exception{
        String safe=safeName(name);
        if(Build.VERSION.SDK_INT>=29)return saveModern(c,folder,safe,mime,writer);
        File dir=new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS),"BlazeFM/"+folder);
        if(!dir.exists()&&!dir.mkdirs())throw new IOException("Cannot create "+dir);
        File out=FileEngine.unique(dir,safe);
        try(OutputStream o=new BufferedOutputStream(new FileOutputStream(out))){writer.write(o);}
        catch(Exception e){FileEngine.delete(out);throw e;}
        return out.getAbsolutePath();
    }

    @TargetApi(29)
    private static String saveModern(Context c,String folder,String name,String mime,Writer writer)throws Exception{
        ContentResolver r=c.getContentResolver();ContentValues v=new ContentValues();
        v.put(MediaStore.MediaColumns.DISPLAY_NAME,name);
        v.put(MediaStore.MediaColumns.MIME_TYPE,mime==null?"application/octet-stream":mime);
        v.put(MediaStore.MediaColumns.RELATIVE_PATH,Environment.DIRECTORY_DOWNLOADS+"/BlazeFM/"+folder);
        v.put(MediaStore.MediaColumns.IS_PENDING,1);
        Uri uri=r.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI,v);if(uri==null)throw new IOException("Android Downloads provider refused the file");
        try{
            try(OutputStream out=r.openOutputStream(uri,"w")){if(out==null)throw new IOException("Cannot open Downloads output");writer.write(out);}
            ContentValues done=new ContentValues();done.put(MediaStore.MediaColumns.IS_PENDING,0);r.update(uri,done,null,null);
            return Environment.DIRECTORY_DOWNLOADS+"/BlazeFM/"+folder+"/"+name;
        }catch(Exception e){try{r.delete(uri,null,null);}catch(Exception ignored){}throw e;}
    }

    private static String safeName(String n){String s=n==null?"download.bin":n.trim().replace('/','_').replace('\\','_');return s.isEmpty()?"download.bin":s;}
}
