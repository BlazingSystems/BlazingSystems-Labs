package com.blazefm.blazesystems;

import android.app.*;
import android.content.*;
import android.graphics.*;
import android.media.MediaPlayer;
import android.os.*;
import android.view.Gravity;
import android.widget.*;

import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.button.MaterialButton;
import com.google.android.material.card.MaterialCardView;

import java.io.*;
import java.util.Locale;

public class PreviewActivity extends Activity {
    private File file;
    private MediaPlayer player;
    private FrameLayout content;
    private MaterialToolbar toolbar;
    private TextView meta;

    @Override public void onCreate(Bundle b){
        super.onCreate(b);
        String p=getIntent().getStringExtra("path");
        if(p==null){finish();return;}
        file=new File(p);

        setContentView(R.layout.activity_preview);
        toolbar=findViewById(R.id.preview_toolbar);
        meta=findViewById(R.id.preview_meta);
        content=findViewById(R.id.preview_content);
        MaterialButton open=findViewById(R.id.preview_open);
        MaterialButton details=findViewById(R.id.preview_details);

        toolbar.setNavigationOnClickListener(v->finish());
        toolbar.setTitle(file.getName());
        toolbar.setOnMenuItemClickListener(item->{
            if(item.getItemId()==R.id.action_share){share();return true;}
            return false;
        });
        open.setOnClickListener(v->openExternal(false));
        details.setOnClickListener(v->details());

        show();
    }

    private void show(){
        content.removeAllViews();
        if(FileEngine.isImage(file)){meta.setText("Image · "+format(file.length()));showImage();return;}
        if(FileEngine.isText(file)){meta.setText("Text · "+format(file.length()));showText();return;}
        if(FileEngine.isVideo(file)){meta.setText("Video · "+format(file.length()));showVideo();return;}
        if(FileEngine.isAudio(file)){meta.setText("Audio · "+format(file.length()));showAudio();return;}
        meta.setText(FileEngine.mime(file)+" · "+format(file.length()));
        openExternal(true);
    }

    private void showImage(){
        FrameLayout stage=new FrameLayout(this);
        stage.setBackgroundColor(0xFF0E1014);
        int pad=Ui.dp(this,8);stage.setPadding(pad,pad,pad,pad);
        ImageView image=new ImageView(this);
        image.setAdjustViewBounds(true);
        image.setScaleType(ImageView.ScaleType.FIT_CENTER);
        stage.addView(image,new FrameLayout.LayoutParams(-1,-1));
        content.addView(stage,new FrameLayout.LayoutParams(-1,-1));

        new Thread(()->{
            try{
                BitmapFactory.Options o=new BitmapFactory.Options();o.inJustDecodeBounds=true;
                BitmapFactory.decodeFile(file.getAbsolutePath(),o);
                int max=Math.max(o.outWidth,o.outHeight);o.inSampleSize=1;
                while(max/o.inSampleSize>2048)o.inSampleSize*=2;
                o.inJustDecodeBounds=false;o.inPreferredConfig=Bitmap.Config.RGB_565;
                Bitmap bm=BitmapFactory.decodeFile(file.getAbsolutePath(),o);
                runOnUiThread(()->{if(bm!=null)image.setImageBitmap(bm);else toast("Preview failed");});
            }catch(Exception e){runOnUiThread(()->toast("Preview failed"));}
        }).start();
    }

    private void showText(){
        ScrollView scroll=new ScrollView(this);
        scroll.setFillViewport(true);
        int pad=Ui.dp(this,12);scroll.setPadding(pad,pad,pad,pad);
        TextView text=new TextView(this);
        text.setText("Loading…");
        text.setTextColor(getResources().getColor(R.color.blaze_on_surface));
        text.setTextSize(13);
        text.setTypeface(android.graphics.Typeface.MONOSPACE);
        text.setTextIsSelectable(true);
        text.setGravity(Gravity.TOP|Gravity.START);
        int inner=Ui.dp(this,16);text.setPadding(inner,inner,inner,inner);
        text.setBackgroundResource(R.drawable.bg_icon_container);
        scroll.addView(text,new ScrollView.LayoutParams(-1,-2));
        content.addView(scroll,new FrameLayout.LayoutParams(-1,-1));

        new Thread(()->{
            String value;
            try{value=readText(file,1024*1024);}
            catch(Exception e){value="Unable to read: "+e.getMessage();}
            final String out=value;
            runOnUiThread(()->text.setText(out));
        }).start();
    }

    private void showVideo(){
        FrameLayout stage=new FrameLayout(this);
        stage.setBackgroundColor(0xFF0E1014);
        VideoView video=new VideoView(this);
        MediaController controls=new MediaController(this);
        controls.setAnchorView(video);
        video.setMediaController(controls);
        video.setVideoPath(file.getAbsolutePath());
        stage.addView(video,new FrameLayout.LayoutParams(-1,-1));
        content.addView(stage,new FrameLayout.LayoutParams(-1,-1));
        video.setOnPreparedListener(mp->video.start());
    }

    private void showAudio(){
        FrameLayout wrap=new FrameLayout(this);
        int pad=Ui.dp(this,20);wrap.setPadding(pad,pad,pad,pad);

        MaterialCardView card=new MaterialCardView(this);
        card.setRadius(Ui.dp(this,24));
        card.setStrokeWidth(Ui.dp(this,1));
        card.setStrokeColor(getResources().getColor(R.color.blaze_outline));
        card.setCardElevation(0);

        LinearLayout body=new LinearLayout(this);
        body.setOrientation(LinearLayout.VERTICAL);
        body.setGravity(Gravity.CENTER);
        int inner=Ui.dp(this,24);body.setPadding(inner,inner,inner,inner);

        ImageView icon=new ImageView(this);
        icon.setImageResource(R.drawable.ic_music);
        icon.setColorFilter(getResources().getColor(R.color.blaze_primary));
        body.addView(icon,new LinearLayout.LayoutParams(Ui.dp(this,56),Ui.dp(this,56)));

        TextView name=new TextView(this);
        name.setText(file.getName());
        name.setTextSize(18);name.setGravity(Gravity.CENTER);name.setTextColor(getResources().getColor(R.color.blaze_on_surface));
        name.setPadding(0,Ui.dp(this,12),0,Ui.dp(this,4));
        body.addView(name,new LinearLayout.LayoutParams(-1,-2));

        TextView details=new TextView(this);
        details.setText(format(file.length())+" · "+FileEngine.mime(file));
        details.setTextSize(12);details.setGravity(Gravity.CENTER);details.setTextColor(getResources().getColor(R.color.blaze_on_surface_variant));
        body.addView(details,new LinearLayout.LayoutParams(-1,-2));

        LinearLayout actions=new LinearLayout(this);
        actions.setOrientation(LinearLayout.HORIZONTAL);
        actions.setPadding(0,Ui.dp(this,18),0,0);

        MaterialButton play=new MaterialButton(this);play.setText("Play");
        MaterialButton stop=new MaterialButton(this,null,com.google.android.material.R.attr.materialButtonOutlinedStyle);stop.setText("Stop");
        actions.addView(play,new LinearLayout.LayoutParams(0,-2,1));
        LinearLayout.LayoutParams gap=new LinearLayout.LayoutParams(Ui.dp(this,8),1);actions.addView(new Space(this),gap);
        actions.addView(stop,new LinearLayout.LayoutParams(0,-2,1));
        body.addView(actions,new LinearLayout.LayoutParams(-1,-2));

        card.addView(body);
        wrap.addView(card,new FrameLayout.LayoutParams(-1,-2,Gravity.CENTER));
        content.addView(wrap,new FrameLayout.LayoutParams(-1,-1));

        try{
            player=new MediaPlayer();
            player.setDataSource(file.getAbsolutePath());
            player.prepare();
            play.setOnClickListener(v->{
                try{
                    if(player.isPlaying()){player.pause();play.setText("Play");}
                    else{player.start();play.setText("Pause");}
                }catch(Exception ignored){}
            });
            stop.setOnClickListener(v->{try{player.pause();player.seekTo(0);play.setText("Play");}catch(Exception ignored){}});
        }catch(Exception e){toast("Audio preview failed");}
    }

    private String readText(File f,int max)throws IOException{
        ByteArrayOutputStream o=new ByteArrayOutputStream();
        try(InputStream in=new FileInputStream(f)){
            byte[]b=new byte[8192];int n,total=0;
            while((n=in.read(b))!=-1&&total<max){int w=Math.min(n,max-total);o.write(b,0,w);total+=w;}
        }
        String s=new String(o.toByteArray(),"UTF-8");
        if(f.length()>max)s+="\n\n[Preview truncated at 1 MiB]";
        return s;
    }

    private void share(){
        Intent i=new Intent(Intent.ACTION_SEND);
        i.setType(FileEngine.mime(file));
        i.putExtra(Intent.EXTRA_STREAM,BlazeProvider.uriFor(file));
        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try{startActivity(Intent.createChooser(i,"Share "+file.getName()));}
        catch(Exception e){toast("No app can share this file");}
    }

    private void details(){
        String msg="Path: "+file.getAbsolutePath()+"\n\nSize: "+format(file.length())+"\nMIME: "+FileEngine.mime(file)+"\nModified: "+new java.util.Date(file.lastModified());
        new com.google.android.material.dialog.MaterialAlertDialogBuilder(this)
                .setTitle("File details")
                .setMessage(msg)
                .setPositiveButton("Close",null)
                .show();
    }

    private void openExternal(boolean finishAfter){
        Intent i=new Intent(Intent.ACTION_VIEW);
        i.setDataAndType(BlazeProvider.uriFor(file),FileEngine.mime(file));
        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try{startActivity(i);if(finishAfter)finish();}
        catch(Exception e){toast("No app can open this file");if(finishAfter)finish();}
    }

    private String format(long n){
        String[]u={"B","KB","MB","GB","TB"};double v=n;int i=0;
        while(v>=1024&&i<u.length-1){v/=1024;i++;}
        return String.format(Locale.US,v>=10?"%.0f %s":"%.1f %s",v,u[i]);
    }

    private void toast(String s){Toast.makeText(this,s,Toast.LENGTH_SHORT).show();}

    @Override protected void onDestroy(){
        if(player!=null){try{player.release();}catch(Exception ignored){}}
        super.onDestroy();
    }
}
