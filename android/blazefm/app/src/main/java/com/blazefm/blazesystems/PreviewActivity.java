package com.blazefm.blazesystems;

import android.app.*;
import android.content.*;
import android.graphics.*;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.*;
import android.view.*;
import android.widget.*;
import java.io.*;
import java.util.Locale;

public class PreviewActivity extends Activity {
    private File file; private MediaPlayer player;
    @Override public void onCreate(Bundle b){super.onCreate(b);String p=getIntent().getStringExtra("path");if(p==null){finish();return;}file=new File(p);setTitle(file.getName());show();}
    private void show(){
        if(FileEngine.isImage(file)){showImage();return;}
        if(FileEngine.isText(file)){showText();return;}
        if(FileEngine.isVideo(file)){showVideo();return;}
        if(FileEngine.isAudio(file)){showAudio();return;}
        openExternal();
    }
    private TextView title(){TextView t=Ui.text(this,file.getName(),18);t.setTextColor(Color.WHITE);t.setBackgroundColor(Ui.ORANGE_DARK);return t;}
    private void showImage(){LinearLayout r=new LinearLayout(this);r.setOrientation(LinearLayout.VERTICAL);r.addView(title());ImageView v=new ImageView(this);v.setAdjustViewBounds(true);v.setScaleType(ImageView.ScaleType.FIT_CENTER);r.addView(v,new LinearLayout.LayoutParams(-1,0,1));setContentView(r);new Thread(()->{try{BitmapFactory.Options o=new BitmapFactory.Options();o.inJustDecodeBounds=true;BitmapFactory.decodeFile(file.getAbsolutePath(),o);int max=Math.max(o.outWidth,o.outHeight);o.inSampleSize=1;while(max/o.inSampleSize>2048)o.inSampleSize*=2;o.inJustDecodeBounds=false;o.inPreferredConfig=Bitmap.Config.RGB_565;Bitmap bm=BitmapFactory.decodeFile(file.getAbsolutePath(),o);runOnUiThread(()->v.setImageBitmap(bm));}catch(Exception e){runOnUiThread(()->toast("Preview failed"));}}).start();}
    private void showText(){LinearLayout r=new LinearLayout(this);r.setOrientation(LinearLayout.VERTICAL);r.addView(title());ScrollView s=new ScrollView(this);TextView v=Ui.text(this,"Loading…",13);v.setTextIsSelectable(true);s.addView(v);r.addView(s,new LinearLayout.LayoutParams(-1,0,1));setContentView(r);new Thread(()->{String text;try{text=readText(file,1024*1024);}catch(Exception e){text="Unable to read: "+e;}String f=text;runOnUiThread(()->v.setText(f));}).start();}
    private String readText(File f,int max)throws IOException{ByteArrayOutputStream o=new ByteArrayOutputStream();try(InputStream in=new FileInputStream(f)){byte[]b=new byte[8192];int n,total=0;while((n=in.read(b))!=-1&&total<max){int w=Math.min(n,max-total);o.write(b,0,w);total+=w;}}String s=new String(o.toByteArray(),"UTF-8");if(f.length()>max)s+="\n\n[Preview truncated at 1 MiB]";return s;}
    private void showVideo(){LinearLayout r=new LinearLayout(this);r.setOrientation(LinearLayout.VERTICAL);r.addView(title());VideoView v=new VideoView(this);MediaController c=new MediaController(this);c.setAnchorView(v);v.setMediaController(c);v.setVideoPath(file.getAbsolutePath());r.addView(v,new LinearLayout.LayoutParams(-1,0,1));setContentView(r);v.setOnPreparedListener(mp->v.start());}
    private void showAudio(){LinearLayout r=new LinearLayout(this);r.setOrientation(LinearLayout.VERTICAL);r.setGravity(Gravity.CENTER_HORIZONTAL);r.addView(title(),new LinearLayout.LayoutParams(-1,-2));TextView info=Ui.text(this,file.getName()+"\n"+format(file.length()),16);info.setGravity(Gravity.CENTER);r.addView(info,new LinearLayout.LayoutParams(-1,0,1));LinearLayout bar=Ui.toolbar(this);Button play=Ui.button(this,"Play / Pause"),stop=Ui.button(this,"Stop");Ui.equalAdd(bar,play);Ui.equalAdd(bar,stop);r.addView(bar);setContentView(r);try{player=new MediaPlayer();player.setDataSource(file.getAbsolutePath());player.prepare();play.setOnClickListener(v->{if(player.isPlaying())player.pause();else player.start();});stop.setOnClickListener(v->{try{player.pause();player.seekTo(0);}catch(Exception ignored){}});}catch(Exception e){toast("Audio preview failed");}}
    private void openExternal(){Intent i=new Intent(Intent.ACTION_VIEW);i.setDataAndType(BlazeProvider.uriFor(file),FileEngine.mime(file));i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);try{startActivity(i);}catch(Exception e){toast("No app can open this file");}finish();}
    private String format(long n){String[]u={"B","KB","MB","GB"};double v=n;int i=0;while(v>=1024&&i<u.length-1){v/=1024;i++;}return String.format(Locale.US,"%.1f %s",v,u[i]);}
    private void toast(String s){Toast.makeText(this,s,Toast.LENGTH_SHORT).show();}
    @Override protected void onDestroy(){super.onDestroy();if(player!=null){try{player.release();}catch(Exception ignored){}}}
}
