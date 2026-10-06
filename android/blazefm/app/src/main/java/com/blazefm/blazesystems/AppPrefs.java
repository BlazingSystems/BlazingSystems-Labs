package com.blazefm.blazesystems;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import java.io.File;
import java.util.*;

final class AppPrefs {
    private static final String P="blazefm";
    private AppPrefs(){}
    static SharedPreferences p(Context c){return c.getSharedPreferences(P,Context.MODE_PRIVATE);}
    static boolean showHidden(Context c){return p(c).getBoolean("show_hidden",false);}
    static void setShowHidden(Context c,boolean b){p(c).edit().putBoolean("show_hidden",b).apply();}
    static LinkedHashSet<String> favorites(Context c){return decode(p(c).getString("favorites",""));}
    static boolean isFavorite(Context c,File f){return favorites(c).contains(canon(f));}
    static void toggleFavorite(Context c,File f){LinkedHashSet<String>s=favorites(c);String k=canon(f);if(!s.remove(k))s.add(k);save(c,"favorites",s,100);}
    static void addRecent(Context c,File f){LinkedHashSet<String>s=decode(p(c).getString("recents",""));String k=canon(f);s.remove(k);LinkedHashSet<String>n=new LinkedHashSet<>();n.add(k);n.addAll(s);save(c,"recents",n,60);}
    static LinkedHashSet<String> recents(Context c){return decode(p(c).getString("recents",""));}
    static void addCloud(Context c,String uri){LinkedHashSet<String>s=decode(p(c).getString("cloud_trees",""));s.remove(uri);s.add(uri);save(c,"cloud_trees",s,20);}
    static void removeCloud(Context c,String uri){LinkedHashSet<String>s=decode(p(c).getString("cloud_trees",""));s.remove(uri);save(c,"cloud_trees",s,20);}
    static LinkedHashSet<String> cloud(Context c){return decode(p(c).getString("cloud_trees",""));}
    private static void save(Context c,String key,LinkedHashSet<String>s,int max){StringBuilder b=new StringBuilder();int n=0;for(String x:s){if(n++>=max)break;b.append(Uri.encode(x)).append('\n');}p(c).edit().putString(key,b.toString()).apply();}
    private static LinkedHashSet<String> decode(String raw){LinkedHashSet<String>s=new LinkedHashSet<>();if(raw==null)return s;for(String l:raw.split("\\n"))if(!l.isEmpty())s.add(Uri.decode(l));return s;}
    static String canon(File f){try{return f.getCanonicalPath();}catch(Exception e){return f.getAbsolutePath();}}
}
