package com.blazefm.blazesystems;

import android.app.Activity;
import android.graphics.*;
import android.util.LruCache;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.*;
import java.io.File;
import java.text.SimpleDateFormat;
import java.util.*;
import java.util.concurrent.*;

final class FileListAdapter extends BaseAdapter {
    private static final LruCache<String,Bitmap> THUMBS=new LruCache<String,Bitmap>(4096){@Override protected int sizeOf(String key,Bitmap value){return Math.max(1,value.getByteCount()/1024);}};
    private static final ExecutorService POOL=Executors.newFixedThreadPool(2);
    private static final Set<String> PENDING=Collections.synchronizedSet(new HashSet<String>());

    private final Activity a;private final List<File> files;private final Set<String> selected;private final boolean grid;
    private final SimpleDateFormat date=new SimpleDateFormat("MMM d · HH:mm",Locale.getDefault());
    FileListAdapter(Activity activity,List<File> items,Set<String> sel){this(activity,items,sel,false);}
    FileListAdapter(Activity activity,List<File> items,Set<String> sel,boolean gridMode){a=activity;files=items;selected=sel;grid=gridMode;}
    public int getCount(){return files.size();}
    public File getItem(int p){return files.get(p);}
    public long getItemId(int p){return p;}

    public View getView(int p,View reuse,ViewGroup parent){return grid?gridView(getItem(p)):listView(getItem(p));}

    private View listView(File f){
        boolean sel=selected.contains(AppPrefs.canon(f));LinearLayout outer=new LinearLayout(a);outer.setPadding(Ui.dp(a,4),Ui.dp(a,3),Ui.dp(a,4),Ui.dp(a,3));
        LinearLayout row=new LinearLayout(a);row.setOrientation(LinearLayout.HORIZONTAL);row.setGravity(Gravity.CENTER_VERTICAL);row.setPadding(Ui.dp(a,12),Ui.dp(a,10),Ui.dp(a,10),Ui.dp(a,10));row.setBackground(Ui.rounded(a,sel?Ui.ACCENT_SOFT:Ui.SURFACE,16,sel?Ui.ORANGE:Ui.BORDER,1));row.setElevation(Ui.dp(a,1));outer.addView(row,new LinearLayout.LayoutParams(-1,-2));
        FrameLayout thumb=thumb(f,sel,48);row.addView(thumb,new LinearLayout.LayoutParams(Ui.dp(a,48),Ui.dp(a,48)));
        LinearLayout info=new LinearLayout(a);info.setOrientation(LinearLayout.VERTICAL);info.setPadding(Ui.dp(a,12),0,Ui.dp(a,8),0);TextView name=Ui.text(a,f.getName(),14);name.setTextColor(Ui.TEXT);name.setTypeface(Typeface.DEFAULT_BOLD);name.setSingleLine(true);name.setEllipsize(android.text.TextUtils.TruncateAt.MIDDLE);name.setPadding(0,0,0,0);info.addView(name);TextView meta=Ui.text(a,meta(f),11);meta.setTextColor(Ui.MUTED);meta.setSingleLine(true);meta.setPadding(0,Ui.dp(a,3),0,0);info.addView(meta);row.addView(info,new LinearLayout.LayoutParams(0,-2,1));
        TextView chevron=Ui.text(a,f.isDirectory()?"›":"⋮",22);chevron.setTextColor(Ui.MUTED);chevron.setGravity(Gravity.CENTER);chevron.setPadding(0,0,0,0);row.addView(chevron,new LinearLayout.LayoutParams(Ui.dp(a,32),Ui.dp(a,44)));return outer;
    }

    private View gridView(File f){
        boolean sel=selected.contains(AppPrefs.canon(f));LinearLayout outer=new LinearLayout(a);outer.setPadding(Ui.dp(a,3),Ui.dp(a,3),Ui.dp(a,3),Ui.dp(a,3));
        LinearLayout card=new LinearLayout(a);card.setOrientation(LinearLayout.VERTICAL);card.setGravity(Gravity.CENTER_HORIZONTAL);card.setPadding(Ui.dp(a,8),Ui.dp(a,9),Ui.dp(a,8),Ui.dp(a,9));card.setBackground(Ui.rounded(a,sel?Ui.ACCENT_SOFT:Ui.SURFACE,16,sel?Ui.ORANGE:Ui.BORDER,1));card.setElevation(Ui.dp(a,1));outer.addView(card,new LinearLayout.LayoutParams(-1,-2));
        FrameLayout thumb=thumb(f,sel,72);card.addView(thumb,new LinearLayout.LayoutParams(Ui.dp(a,72),Ui.dp(a,72)));
        TextView name=Ui.text(a,f.getName(),12);name.setTypeface(Typeface.DEFAULT_BOLD);name.setGravity(Gravity.CENTER);name.setMaxLines(2);name.setEllipsize(android.text.TextUtils.TruncateAt.END);name.setPadding(Ui.dp(a,2),Ui.dp(a,7),Ui.dp(a,2),0);card.addView(name,new LinearLayout.LayoutParams(-1,Ui.dp(a,42)));
        TextView meta=Ui.text(a,f.isDirectory()?"Folder":size(f.length()),10);meta.setTextColor(Ui.MUTED);meta.setGravity(Gravity.CENTER);meta.setPadding(0,Ui.dp(a,2),0,0);card.addView(meta);return outer;
    }

    private FrameLayout thumb(File f,boolean sel,int dp){
        FrameLayout box=new FrameLayout(a);TextView badge=Ui.text(a,sel?"✓":symbol(f),f.isDirectory()?20:17);badge.setGravity(Gravity.CENTER);badge.setTypeface(Typeface.DEFAULT_BOLD);badge.setTextColor(sel?Ui.ORANGE:badgeColor(f));badge.setPadding(0,0,0,0);badge.setBackground(Ui.rounded(a,sel?Color.WHITE:badgeBg(f),14,Color.TRANSPARENT,0));box.addView(badge,new FrameLayout.LayoutParams(-1,-1));
        if(FileEngine.isImage(f)&&!sel){ImageView image=new ImageView(a);image.setScaleType(ImageView.ScaleType.CENTER_CROP);image.setBackground(Ui.rounded(a,0xFFF0F2F5,14,Color.TRANSPARENT,0));image.setClipToOutline(true);box.addView(image,new FrameLayout.LayoutParams(-1,-1));bindThumb(image,f,grid?220:160);}return box;
    }

    private void bindThumb(ImageView v,File f,int target){
        final String key=f.getAbsolutePath()+":"+f.lastModified()+":"+f.length()+":"+target;v.setTag(key);Bitmap cached=THUMBS.get(key);if(cached!=null){v.setImageBitmap(cached);return;}if(!PENDING.add(key))return;
        POOL.execute(()->{Bitmap bm=null;try{bm=decode(f,target);}catch(Throwable ignored){}if(bm!=null)THUMBS.put(key,bm);PENDING.remove(key);final Bitmap out=bm;a.runOnUiThread(()->{Object tag=v.getTag();if(out!=null&&key.equals(tag))v.setImageBitmap(out);});});
    }

    private Bitmap decode(File f,int target){
        BitmapFactory.Options o=new BitmapFactory.Options();o.inJustDecodeBounds=true;BitmapFactory.decodeFile(f.getAbsolutePath(),o);if(o.outWidth<=0||o.outHeight<=0)return null;int max=Math.max(o.outWidth,o.outHeight);o.inSampleSize=1;while(max/o.inSampleSize>target*2)o.inSampleSize*=2;o.inJustDecodeBounds=false;o.inPreferredConfig=Bitmap.Config.RGB_565;Bitmap src=BitmapFactory.decodeFile(f.getAbsolutePath(),o);if(src==null)return null;int side=Math.min(src.getWidth(),src.getHeight());int x=(src.getWidth()-side)/2,y=(src.getHeight()-side)/2;Bitmap crop=Bitmap.createBitmap(src,x,y,side,side);Bitmap out=Bitmap.createScaledBitmap(crop,target,target,true);if(src!=out&&src!=crop)src.recycle();if(crop!=out)crop.recycle();return out;
    }

    private String meta(File f){if(f.isDirectory())return "Folder  ·  "+date.format(new Date(f.lastModified()));return type(f)+"  ·  "+size(f.length())+"  ·  "+date.format(new Date(f.lastModified()));}
    private String type(File f){if(FileEngine.isImage(f))return "Image";if(FileEngine.isVideo(f))return "Video";if(FileEngine.isAudio(f))return "Audio";if(FileEngine.isApk(f))return "APK";if(FileEngine.isArchive(f))return "Archive";if(FileEngine.isText(f))return "Text";String n=f.getName();int i=n.lastIndexOf('.');return i>0&&i<n.length()-1?n.substring(i+1).toUpperCase(Locale.US):"File";}
    private String symbol(File f){if(f.isDirectory())return "▰";if(FileEngine.isImage(f))return "◩";if(FileEngine.isVideo(f))return "▶";if(FileEngine.isAudio(f))return "♪";if(FileEngine.isApk(f))return "A";if(FileEngine.isArchive(f))return "Z";if(FileEngine.isText(f))return "T";return "•";}
    private int badgeColor(File f){if(f.isDirectory())return Ui.ORANGE;if(FileEngine.isImage(f))return 0xFF7C3AED;if(FileEngine.isVideo(f))return 0xFFDC2626;if(FileEngine.isAudio(f))return 0xFF2563EB;if(FileEngine.isApk(f))return 0xFF0F766E;if(FileEngine.isArchive(f))return 0xFF9A6700;return 0xFF596273;}
    private int badgeBg(File f){if(f.isDirectory())return 0xFFFFF1E6;if(FileEngine.isImage(f))return 0xFFF2ECFF;if(FileEngine.isVideo(f))return 0xFFFFEAEA;if(FileEngine.isAudio(f))return 0xFFEAF2FF;if(FileEngine.isApk(f))return 0xFFE7F6F3;if(FileEngine.isArchive(f))return 0xFFFFF5D8;return 0xFFF0F2F5;}
    private String size(long n){String[]u={"B","KB","MB","GB","TB"};double v=n;int i=0;while(v>=1024&&i<u.length-1){v/=1024;i++;}return String.format(Locale.US,v>=10?"%.0f %s":"%.1f %s",v,u[i]);}
}
