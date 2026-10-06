package com.blazefm.blazesystems;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.LruCache;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.button.MaterialButton;
import com.google.android.material.card.MaterialCardView;

import java.io.File;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

final class FileRecyclerAdapter extends RecyclerView.Adapter<FileRecyclerAdapter.Holder> {
    interface Listener {
        void onClick(File file);
        void onLongClick(File file);
        void onMore(File file);
    }

    private static final int LIST=1, GRID=2;
    private static final LruCache<String,Bitmap> THUMBS=new LruCache<String,Bitmap>(4096){
        @Override protected int sizeOf(String key,Bitmap value){return Math.max(1,value.getByteCount()/1024);}
    };
    private static final ExecutorService POOL=Executors.newFixedThreadPool(2);
    private static final Set<String> PENDING=java.util.Collections.synchronizedSet(new HashSet<String>());

    private final Activity activity;
    private final List<File> files;
    private final Set<String> selected;
    private final boolean grid;
    private final Listener listener;
    private final SimpleDateFormat date=new SimpleDateFormat("MMM d · HH:mm",Locale.getDefault());

    FileRecyclerAdapter(Activity a,List<File> items,Set<String> sel,boolean gridMode,Listener l){
        activity=a;files=items;selected=sel;grid=gridMode;listener=l;setHasStableIds(true);
    }

    @Override public long getItemId(int position){return AppPrefs.canon(files.get(position)).hashCode();}
    @Override public int getItemCount(){return files.size();}
    @Override public int getItemViewType(int position){return grid?GRID:LIST;}

    @Override public Holder onCreateViewHolder(ViewGroup parent,int type){
        int layout=type==GRID?R.layout.item_file_grid:R.layout.item_file_list;
        return new Holder(LayoutInflater.from(parent.getContext()).inflate(layout,parent,false),type==GRID);
    }

    @Override public void onBindViewHolder(Holder h,int position){
        File f=files.get(position);
        boolean isSelected=selected.contains(AppPrefs.canon(f));
        h.name.setText(f.getName());
        h.meta.setText(grid?(f.isDirectory()?"Folder":size(f.length())):meta(f));
        h.selected.setVisibility(isSelected?View.VISIBLE:View.GONE);
        h.icon.setVisibility(isSelected?View.GONE:View.VISIBLE);
        h.thumb.setVisibility(View.GONE);
        h.thumb.setImageDrawable(null);
        h.icon.setImageResource(iconFor(f));

        int stroke=ContextCompat.getColor(activity,isSelected?R.color.blaze_primary:R.color.blaze_outline);
        h.card.setStrokeColor(stroke);
        h.card.setStrokeWidth(dp(isSelected?2:1));

        if(FileEngine.isImage(f)&&!isSelected)bindThumb(h.thumb,h.icon,f,grid?220:160);

        h.card.setOnClickListener(v->listener.onClick(f));
        h.card.setOnLongClickListener(v->{listener.onLongClick(f);return true;});
        if(h.more!=null)h.more.setOnClickListener(v->listener.onMore(f));
    }

    private void bindThumb(ImageView image,ImageView icon,File f,int target){
        final String key=f.getAbsolutePath()+":"+f.lastModified()+":"+f.length()+":"+target;
        image.setTag(key);
        Bitmap cached=THUMBS.get(key);
        if(cached!=null){
            image.setImageBitmap(cached);image.setVisibility(View.VISIBLE);icon.setVisibility(View.GONE);return;
        }
        if(!PENDING.add(key))return;
        POOL.execute(()->{
            Bitmap bm=null;
            try{bm=decode(f,target);}catch(Throwable ignored){}
            if(bm!=null)THUMBS.put(key,bm);
            PENDING.remove(key);
            final Bitmap out=bm;
            activity.runOnUiThread(()->{
                if(out!=null&&key.equals(image.getTag())){
                    image.setImageBitmap(out);image.setVisibility(View.VISIBLE);icon.setVisibility(View.GONE);
                }
            });
        });
    }

    private Bitmap decode(File f,int target){
        BitmapFactory.Options o=new BitmapFactory.Options();
        o.inJustDecodeBounds=true;BitmapFactory.decodeFile(f.getAbsolutePath(),o);
        if(o.outWidth<=0||o.outHeight<=0)return null;
        int max=Math.max(o.outWidth,o.outHeight);o.inSampleSize=1;
        while(max/o.inSampleSize>target*2)o.inSampleSize*=2;
        o.inJustDecodeBounds=false;o.inPreferredConfig=Bitmap.Config.RGB_565;
        Bitmap src=BitmapFactory.decodeFile(f.getAbsolutePath(),o);if(src==null)return null;
        int side=Math.min(src.getWidth(),src.getHeight());
        int x=(src.getWidth()-side)/2,y=(src.getHeight()-side)/2;
        Bitmap crop=Bitmap.createBitmap(src,x,y,side,side);
        Bitmap out=Bitmap.createScaledBitmap(crop,target,target,true);
        if(src!=out&&src!=crop)src.recycle();
        if(crop!=out)crop.recycle();
        return out;
    }

    private int iconFor(File f){
        if(f.isDirectory())return R.drawable.ic_folder;
        if(FileEngine.isImage(f))return R.drawable.ic_image;
        if(FileEngine.isVideo(f))return R.drawable.ic_video;
        if(FileEngine.isAudio(f))return R.drawable.ic_music;
        if(FileEngine.isApk(f))return R.drawable.ic_apps;
        if(FileEngine.isArchive(f))return R.drawable.ic_archive;
        if(FileEngine.isText(f))return R.drawable.ic_description;
        return R.drawable.ic_file;
    }

    private String meta(File f){
        if(f.isDirectory())return "Folder · "+date.format(new Date(f.lastModified()));
        return type(f)+" · "+size(f.length())+" · "+date.format(new Date(f.lastModified()));
    }

    private String type(File f){
        if(FileEngine.isImage(f))return "Image";
        if(FileEngine.isVideo(f))return "Video";
        if(FileEngine.isAudio(f))return "Audio";
        if(FileEngine.isApk(f))return "APK";
        if(FileEngine.isArchive(f))return "Archive";
        if(FileEngine.isText(f))return "Text";
        String n=f.getName();int i=n.lastIndexOf('.');
        return i>0&&i<n.length()-1?n.substring(i+1).toUpperCase(Locale.US):"File";
    }

    private String size(long n){
        String[]u={"B","KB","MB","GB","TB"};double v=n;int i=0;
        while(v>=1024&&i<u.length-1){v/=1024;i++;}
        return String.format(Locale.US,v>=10?"%.0f %s":"%.1f %s",v,u[i]);
    }

    private int dp(int value){return Math.round(value*activity.getResources().getDisplayMetrics().density);}

    static final class Holder extends RecyclerView.ViewHolder{
        final MaterialCardView card;
        final ImageView thumb,icon,selected;
        final TextView name,meta;
        final MaterialButton more;
        Holder(View item,boolean grid){
            super(item);
            card=item.findViewById(R.id.card);
            thumb=item.findViewById(R.id.thumbnail);
            icon=item.findViewById(R.id.file_icon);
            selected=item.findViewById(R.id.selected_icon);
            name=item.findViewById(R.id.name);
            meta=item.findViewById(R.id.meta);
            more=grid?null:item.findViewById(R.id.more);
        }
    }
}
