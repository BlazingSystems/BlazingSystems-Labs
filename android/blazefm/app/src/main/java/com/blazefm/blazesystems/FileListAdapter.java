package com.blazefm.blazesystems;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Typeface;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.File;
import java.text.SimpleDateFormat;
import java.util.*;

final class FileListAdapter extends BaseAdapter {
    private final Activity a;private final List<File> files;private final Set<String> selected;
    private final SimpleDateFormat date=new SimpleDateFormat("MMM d · HH:mm",Locale.getDefault());
    FileListAdapter(Activity activity,List<File> items,Set<String> sel){a=activity;files=items;selected=sel;}
    public int getCount(){return files.size();}
    public File getItem(int p){return files.get(p);}
    public long getItemId(int p){return p;}

    public View getView(int p,View reuse,ViewGroup parent){
        File f=getItem(p);boolean sel=selected.contains(AppPrefs.canon(f));
        LinearLayout outer=new LinearLayout(a);outer.setPadding(Ui.dp(a,4),Ui.dp(a,3),Ui.dp(a,4),Ui.dp(a,3));

        LinearLayout row=new LinearLayout(a);row.setOrientation(LinearLayout.HORIZONTAL);row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(Ui.dp(a,12),Ui.dp(a,10),Ui.dp(a,10),Ui.dp(a,10));
        row.setBackground(Ui.rounded(a,sel?Ui.ACCENT_SOFT:Ui.SURFACE,16,sel?Ui.ORANGE:Ui.BORDER,1));
        row.setElevation(Ui.dp(a,1));outer.addView(row,new LinearLayout.LayoutParams(-1,-2));

        TextView badge=Ui.text(a,sel?"✓":symbol(f),f.isDirectory()?20:17);badge.setGravity(Gravity.CENTER);badge.setTypeface(Typeface.DEFAULT_BOLD);badge.setTextColor(sel?Ui.ORANGE:badgeColor(f));badge.setPadding(0,0,0,0);
        badge.setBackground(Ui.rounded(a,sel?Color.WHITE:badgeBg(f),14,Color.TRANSPARENT,0));
        row.addView(badge,new LinearLayout.LayoutParams(Ui.dp(a,46),Ui.dp(a,46)));

        LinearLayout info=new LinearLayout(a);info.setOrientation(LinearLayout.VERTICAL);info.setPadding(Ui.dp(a,12),0,Ui.dp(a,8),0);
        TextView name=Ui.text(a,f.getName(),14);name.setTextColor(Ui.TEXT);name.setTypeface(Typeface.DEFAULT_BOLD);name.setSingleLine(true);name.setEllipsize(android.text.TextUtils.TruncateAt.MIDDLE);name.setPadding(0,0,0,0);info.addView(name);
        TextView meta=Ui.text(a,meta(f),11);meta.setTextColor(Ui.MUTED);meta.setSingleLine(true);meta.setPadding(0,Ui.dp(a,3),0,0);info.addView(meta);
        row.addView(info,new LinearLayout.LayoutParams(0,-2,1));

        TextView chevron=Ui.text(a,f.isDirectory()?"›":"⋮",22);chevron.setTextColor(Ui.MUTED);chevron.setGravity(Gravity.CENTER);chevron.setPadding(0,0,0,0);
        row.addView(chevron,new LinearLayout.LayoutParams(Ui.dp(a,32),Ui.dp(a,44)));
        return outer;
    }

    private String meta(File f){
        if(f.isDirectory())return "Folder  ·  "+date.format(new Date(f.lastModified()));
        return type(f)+"  ·  "+size(f.length())+"  ·  "+date.format(new Date(f.lastModified()));
    }
    private String type(File f){
        if(FileEngine.isImage(f))return "Image";if(FileEngine.isVideo(f))return "Video";if(FileEngine.isAudio(f))return "Audio";
        if(FileEngine.isApk(f))return "APK";if(FileEngine.isArchive(f))return "Archive";if(FileEngine.isText(f))return "Text";
        String n=f.getName();int i=n.lastIndexOf('.');return i>0&&i<n.length()-1?n.substring(i+1).toUpperCase(Locale.US):"File";
    }
    private String symbol(File f){if(f.isDirectory())return "▰";if(FileEngine.isImage(f))return "◩";if(FileEngine.isVideo(f))return "▶";if(FileEngine.isAudio(f))return "♪";if(FileEngine.isApk(f))return "A";if(FileEngine.isArchive(f))return "Z";if(FileEngine.isText(f))return "T";return "•";}
    private int badgeColor(File f){if(f.isDirectory())return Ui.ORANGE;if(FileEngine.isImage(f))return 0xFF7C3AED;if(FileEngine.isVideo(f))return 0xFFDC2626;if(FileEngine.isAudio(f))return 0xFF2563EB;if(FileEngine.isApk(f))return 0xFF0F766E;if(FileEngine.isArchive(f))return 0xFF9A6700;return 0xFF596273;}
    private int badgeBg(File f){if(f.isDirectory())return 0xFFFFF1E6;if(FileEngine.isImage(f))return 0xFFF2ECFF;if(FileEngine.isVideo(f))return 0xFFFFEAEA;if(FileEngine.isAudio(f))return 0xFFEAF2FF;if(FileEngine.isApk(f))return 0xFFE7F6F3;if(FileEngine.isArchive(f))return 0xFFFFF5D8;return 0xFFF0F2F5;}
    private String size(long n){String[]u={"B","KB","MB","GB","TB"};double v=n;int i=0;while(v>=1024&&i<u.length-1){v/=1024;i++;}return String.format(Locale.US,v>=10?"%.0f %s":"%.1f %s",v,u[i]);}
}
