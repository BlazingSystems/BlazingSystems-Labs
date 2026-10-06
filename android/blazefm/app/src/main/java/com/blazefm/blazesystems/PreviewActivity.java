package com.blazefm.blazesystems;

import android.app.*;
import android.content.*;
import android.graphics.*;
import android.media.MediaPlayer;
import android.os.*;
import android.view.*;
import android.widget.*;
import java.io.*;
import java.util.Locale;

public class PreviewActivity extends Activity {
    private File file; private MediaPlayer player;

    @Override public void onCreate(Bundle b){
        super.onCreate(b);
        String p=getIntent().getStringExtra("path");if(p==null){finish();return;}
        file=new File(p);show();
    }

    private void show(){
        if(FileEngine.isImage(file)){showImage();return;}
        if(FileEngine.isText(file)){showText();return;}
        if(FileEngine.isVideo(file)){showVideo();return;}
        if(FileEngine.isAudio(file)){showAudio();return;}
        openExternal(true);
    }

    private LinearLayout base(String kind){
        LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setBackgroundColor(Ui.BG);
        LinearLayout top=new LinearLayout(this);top.setOrientation(LinearLayout.HORIZONTAL);top.setGravity(Gravity.CENTER_VERTICAL);top.setPadding(Ui.dp(this,8),Ui.dp(this,10),Ui.dp(this,10),Ui.dp(this,10));top.setBackgroundColor(Ui.HEADER);top.setElevation(Ui.dp(this,4));
        Button back=Ui.iconButton(this,"‹");back.setContentDescription("Back");back.setOnClickListener(v->finish());top.addView(back,new LinearLayout.LayoutParams(Ui.dp(this,48),Ui.dp(this,44)));
        LinearLayout labels=new LinearLayout(this);labels.setOrientation(LinearLayout.VERTICAL);
        TextView name=Ui.text(this,file.getName(),17);name.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);name.setTextColor(Color.WHITE);name.setSingleLine(true);name.setEllipsize(android.text.TextUtils.TruncateAt.MIDDLE);name.setPadding(0,0,0,0);labels.addView(name);
        TextView meta=Ui.text(this,kind+"  ·  "+format(file.length()),11);meta.setTextColor(Ui.HEADER_MUTED);meta.setPadding(0,Ui.dp(this,2),0,0);labels.addView(meta);
        top.addView(labels,new LinearLayout.LayoutParams(0,-2,1));
        Button share=Ui.iconButton(this,"↗");share.setContentDescription("Share");share.setOnClickListener(v->share());top.addView(share,new LinearLayout.LayoutParams(Ui.dp(this,48),Ui.dp(this,44)));
        root.addView(top);
        return root;
    }

    private void addFooter(LinearLayout root){
        LinearLayout bar=Ui.toolbar(this);
        Button open=Ui.primaryButton(this,"Open in app");open.setOnClickListener(v->openExternal(false));Ui.equalAdd(bar,open);
        Button info=Ui.button(this,"Details");info.setOnClickListener(v->details());Ui.equalAdd(bar,info);
        root.addView(bar);
    }

    private void showImage(){
        LinearLayout r=base("Image");
        FrameLayout stage=new FrameLayout(this);stage.setBackgroundColor(0xFF0E1014);stage.setPadding(Ui.dp(this,8),Ui.dp(this,8),Ui.dp(this,8),Ui.dp(this,8));
        ImageView v=new ImageView(this);v.setAdjustViewBounds(true);v.setScaleType(ImageView.ScaleType.FIT_CENTER);stage.addView(v,new FrameLayout.LayoutParams(-1,-1));r.addView(stage,new LinearLayout.LayoutParams(-1,0,1));addFooter(r);setContentView(r);
        new Thread(()->{try{BitmapFactory.Options o=new BitmapFactory.Options();o.inJustDecodeBounds=true;BitmapFactory.decodeFile(file.getAbsolutePath(),o);int max=Math.max(o.outWidth,o.outHeight);o.inSampleSize=1;while(max/o.inSampleSize>2048)o.inSampleSize*=2;o.inJustDecodeBounds=false;o.inPreferredConfig=Bitmap.Config.RGB_565;Bitmap bm=BitmapFactory.decodeFile(file.getAbsolutePath(),o);runOnUiThread(()->{if(bm!=null)v.setImageBitmap(bm);else toast("Preview failed");});}catch(Exception e){runOnUiThread(()->toast("Preview failed"));}}).start();
    }

    private void showText(){
        LinearLayout r=base("Text");
        ScrollView scroll=new ScrollView(this);scroll.setFillViewport(true);scroll.setPadding(Ui.dp(this,10),Ui.dp(this,10),Ui.dp(this,10),Ui.dp(this,10));
        TextView v=Ui.text(this,"Loading…",13);v.setTypeface(android.graphics.Typeface.MONOSPACE);v.setTextIsSelectable(true);v.setGravity(Gravity.TOP|Gravity.LEFT);v.setBackground(Ui.rounded(this,Ui.SURFACE,16,Ui.BORDER,1));v.setPadding(Ui.dp(this,16),Ui.dp(this,16),Ui.dp(this,16),Ui.dp(this,16));scroll.addView(v,new ScrollView.LayoutParams(-1,-2));r.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));addFooter(r);setContentView(r);
        new Thread(()->{String text;try{text=readText(file,1024*1024);}catch(Exception e){text="Unable to read: "+e.getMessage();}String f=text;runOnUiThread(()->v.setText(f));}).start();
    }

    private void showVideo(){
        LinearLayout r=base("Video");FrameLayout stage=new FrameLayout(this);stage.setBackgroundColor(0xFF0E1014);
        VideoView v=new VideoView(this);MediaController c=new MediaController(this);c.setAnchorView(v);v.setMediaController(c);v.setVideoPath(file.getAbsolutePath());stage.addView(v,new FrameLayout.LayoutParams(-1,-1));r.addView(stage,new LinearLayout.LayoutParams(-1,0,1));addFooter(r);setContentView(r);v.setOnPreparedListener(mp->v.start());
    }

    private void showAudio(){
        LinearLayout r=base("Audio");
        LinearLayout wrap=new LinearLayout(this);wrap.setGravity(Gravity.CENTER);wrap.setPadding(Ui.dp(this,18),Ui.dp(this,18),Ui.dp(this,18),Ui.dp(this,18));
        LinearLayout card=new LinearLayout(this);card.setOrientation(LinearLayout.VERTICAL);card.setGravity(Gravity.CENTER);card.setPadding(Ui.dp(this,22),Ui.dp(this,24),Ui.dp(this,22),Ui.dp(this,24));card.setBackground(Ui.rounded(this,Ui.SURFACE,20,Ui.BORDER,1));card.setElevation(Ui.dp(this,2));
        TextView icon=Ui.text(this,"♪",42);icon.setGravity(Gravity.CENTER);icon.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);icon.setTextColor(Ui.ORANGE);icon.setPadding(0,0,0,Ui.dp(this,8));card.addView(icon);
        TextView n=Ui.text(this,file.getName(),16);n.setGravity(Gravity.CENTER);n.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);n.setPadding(0,0,0,Ui.dp(this,4));card.addView(n);
        TextView meta=Ui.text(this,format(file.length())+"  ·  "+FileEngine.mime(file),11);meta.setTextColor(Ui.MUTED);meta.setGravity(Gravity.CENTER);meta.setPadding(0,0,0,Ui.dp(this,14));card.addView(meta);
        LinearLayout controls=Ui.toolbar(this);Button play=Ui.primaryButton(this,"Play / Pause"),stop=Ui.button(this,"Stop");Ui.equalAdd(controls,play);Ui.equalAdd(controls,stop);card.addView(controls,new LinearLayout.LayoutParams(-1,-2));
        wrap.addView(card,new LinearLayout.LayoutParams(-1,-2));r.addView(wrap,new LinearLayout.LayoutParams(-1,0,1));addFooter(r);setContentView(r);
        try{player=new MediaPlayer();player.setDataSource(file.getAbsolutePath());player.prepare();play.setOnClickListener(v->{try{if(player.isPlaying())player.pause();else player.start();}catch(Exception ignored){}});stop.setOnClickListener(v->{try{player.pause();player.seekTo(0);}catch(Exception ignored){}});}catch(Exception e){toast("Audio preview failed");}
    }

    private String readText(File f,int max)throws IOException{ByteArrayOutputStream o=new ByteArrayOutputStream();try(InputStream in=new FileInputStream(f)){byte[]b=new byte[8192];int n,total=0;while((n=in.read(b))!=-1&&total<max){int w=Math.min(n,max-total);o.write(b,0,w);total+=w;}}String s=new String(o.toByteArray(),"UTF-8");if(f.length()>max)s+="\n\n[Preview truncated at 1 MiB]";return s;}

    private void share(){
        Intent i=new Intent(Intent.ACTION_SEND);i.setType(FileEngine.mime(file));i.putExtra(Intent.EXTRA_STREAM,BlazeProvider.uriFor(file));i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try{startActivity(Intent.createChooser(i,"Share "+file.getName()));}catch(Exception e){toast("No app can share this file");}
    }

    private void details(){
        String msg="Path: "+file.getAbsolutePath()+"\n\nSize: "+format(file.length())+"\nMIME: "+FileEngine.mime(file)+"\nModified: "+new java.util.Date(file.lastModified());
        new AlertDialog.Builder(this).setTitle("File details").setMessage(msg).setPositiveButton("Close",null).show();
    }

    private void openExternal(boolean finishAfter){
        Intent i=new Intent(Intent.ACTION_VIEW);i.setDataAndType(BlazeProvider.uriFor(file),FileEngine.mime(file));i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try{startActivity(i);if(finishAfter)finish();}catch(Exception e){toast("No app can open this file");if(finishAfter)finish();}
    }

    private String format(long n){String[]u={"B","KB","MB","GB","TB"};double v=n;int i=0;while(v>=1024&&i<u.length-1){v/=1024;i++;}return String.format(Locale.US,v>=10?"%.0f %s":"%.1f %s",v,u[i]);}
    private void toast(String s){Toast.makeText(this,s,Toast.LENGTH_SHORT).show();}
    @Override protected void onDestroy(){if(player!=null){try{player.release();}catch(Exception ignored){}}super.onDestroy();}
}
