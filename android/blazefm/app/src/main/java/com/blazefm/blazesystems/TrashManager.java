package com.blazefm.blazesystems;

import android.content.Context;
import android.net.Uri;
import android.os.Environment;
import java.io.File;
import java.io.IOException;
import java.util.*;

final class TrashManager {
    static final class Item { final File stored; final String original; Item(File s,String o){stored=s;original=o;} }
    private static File dir(){File d=new File(Environment.getExternalStorageDirectory(),".BlazeFM_Trash");if(!d.exists())d.mkdirs();return d;}
    static void moveToTrash(Context c,File f)throws IOException{
        File d=dir();File out=FileEngine.unique(d,System.currentTimeMillis()+"__"+f.getName());String original=AppPrefs.canon(f);FileEngine.move(f,out);addIndex(c,out,original);
    }
    static List<Item> list(Context c){
        Map<String,String>idx=index(c);ArrayList<Item>out=new ArrayList<>();File[]a=dir().listFiles();if(a!=null)for(File f:a){String o=idx.get(AppPrefs.canon(f));if(o==null)o=f.getName();out.add(new Item(f,o));}Collections.sort(out,(x,y)->Long.compare(y.stored.lastModified(),x.stored.lastModified()));return out;
    }
    static File restore(Context c,Item i)throws IOException{
        File target=new File(i.original);File p=target.getParentFile();if(p==null||(!p.exists()&&!p.mkdirs()))target=FileEngine.unique(Environment.getExternalStorageDirectory(),target.getName());else if(target.exists())target=FileEngine.unique(p,target.getName());FileEngine.move(i.stored,target);removeIndex(c,i.stored);return target;
    }
    static void purge(Context c,Item i){FileEngine.delete(i.stored);removeIndex(c,i.stored);}
    static void empty(Context c){for(Item i:list(c))FileEngine.delete(i.stored);AppPrefs.p(c).edit().remove("trash_index").apply();}
    private static Map<String,String>index(Context c){LinkedHashMap<String,String>m=new LinkedHashMap<>();String raw=AppPrefs.p(c).getString("trash_index","");for(String line:raw.split("\\n")){if(line.isEmpty())continue;int k=line.indexOf('|');if(k>0)m.put(Uri.decode(line.substring(0,k)),Uri.decode(line.substring(k+1)));}return m;}
    private static void addIndex(Context c,File stored,String original){Map<String,String>m=index(c);m.put(AppPrefs.canon(stored),original);save(c,m);}
    private static void removeIndex(Context c,File stored){Map<String,String>m=index(c);m.remove(AppPrefs.canon(stored));save(c,m);}
    private static void save(Context c,Map<String,String>m){StringBuilder b=new StringBuilder();for(Map.Entry<String,String>e:m.entrySet())b.append(Uri.encode(e.getKey())).append('|').append(Uri.encode(e.getValue())).append('\n');AppPrefs.p(c).edit().putString("trash_index",b.toString()).apply();}
}
