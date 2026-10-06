package com.blazefm.blazesystems;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.button.MaterialButton;
import com.google.android.material.progressindicator.LinearProgressIndicator;

import java.io.File;
import java.util.Locale;

public class AnalyzerActivity extends Activity {
    private TextView status,scope,dataValue,filesValue,foldersValue,emptyFoldersValue;
    private LinearLayout categories,largest;
    private LinearProgressIndicator progress;
    private MaterialButton cancelButton;
    private volatile boolean cancel;
    private File root;

    @Override public void onCreate(Bundle b){
        super.onCreate(b);
        setContentView(R.layout.activity_analyzer);

        MaterialToolbar toolbar=findViewById(R.id.analyzer_toolbar);
        toolbar.setNavigationOnClickListener(v->finish());

        status=findViewById(R.id.analyzer_status);
        scope=findViewById(R.id.analyzer_scope);
        dataValue=findViewById(R.id.analyzer_data);
        filesValue=findViewById(R.id.analyzer_files);
        foldersValue=findViewById(R.id.analyzer_folders);
        emptyFoldersValue=findViewById(R.id.analyzer_empty_folders);
        categories=findViewById(R.id.analyzer_categories);
        largest=findViewById(R.id.analyzer_largest);
        progress=findViewById(R.id.analyzer_progress);
        cancelButton=findViewById(R.id.analyzer_cancel);

        cancelButton.setOnClickListener(v->{
            cancel=true;
            cancelButton.setEnabled(false);
            cancelButton.setText("Cancelling…");
        });

        String p=getIntent().getStringExtra("path");
        root=p==null?Environment.getExternalStorageDirectory():new File(p);
        scope.setText(friendlyPath(root));
        run(root);
    }

    private void run(File scanRoot){
        cancel=false;
        progress.setVisibility(View.VISIBLE);
        status.setText("Scanning…");

        new Thread(()->{
            FileEngine.Summary x=FileEngine.analyze(scanRoot,AppPrefs.showHidden(this),new FileEngine.Progress(){
                @Override public void update(String text,int done,int total){
                    runOnUiThread(()->status.setText(text));
                }
                @Override public boolean cancelled(){return cancel;}
            });

            runOnUiThread(()->render(x));
        }).start();
    }

    private void render(FileEngine.Summary x){
        progress.setVisibility(View.GONE);
        cancelButton.setVisibility(View.GONE);

        dataValue.setText(fmt(x.bytes));
        filesValue.setText(String.valueOf(x.files));
        foldersValue.setText(String.valueOf(x.dirs));
        emptyFoldersValue.setText(String.valueOf(x.emptyDirs));
        status.setText(cancel?"Scan cancelled":"Analysis complete");

        renderCategories(x);
        renderLargest(x);
    }

    private void renderCategories(FileEngine.Summary x){
        categories.removeAllViews();

        String[] names={"Images","Video","Audio","Documents","APKs","Archives","Other"};
        int[] icons={
                R.drawable.ic_image,
                R.drawable.ic_video,
                R.drawable.ic_music,
                R.drawable.ic_description,
                R.drawable.ic_apps,
                R.drawable.ic_archive,
                R.drawable.ic_file
        };

        LayoutInflater inflater=LayoutInflater.from(this);
        for(int i=0;i<names.length;i++){
            View row=inflater.inflate(R.layout.item_analyzer_category,categories,false);
            ImageView icon=row.findViewById(R.id.category_icon);
            TextView name=row.findViewById(R.id.category_name);
            TextView meta=row.findViewById(R.id.category_meta);
            LinearProgressIndicator bar=row.findViewById(R.id.category_progress);

            icon.setImageResource(icons[i]);
            name.setText(names[i]);
            meta.setText(x.categoryCount[i]+" files · "+fmt(x.categoryBytes[i]));

            int percent=x.bytes<=0?0:(int)Math.min(100,(x.categoryBytes[i]*100L)/x.bytes);
            bar.setMax(100);
            bar.setProgress(percent);

            categories.addView(row);
        }
    }

    private void renderLargest(FileEngine.Summary x){
        largest.removeAllViews();
        LayoutInflater inflater=LayoutInflater.from(this);
        int max=Math.min(25,x.largest.size());

        if(max==0){
            TextView none=new TextView(this);
            none.setText("No files found in this location.");
            none.setTextColor(getResources().getColor(R.color.blaze_on_surface_variant));
            none.setPadding(Ui.dp(this,4),Ui.dp(this,12),Ui.dp(this,4),Ui.dp(this,12));
            largest.addView(none,new LinearLayout.LayoutParams(-1,-2));
            return;
        }

        for(int i=0;i<max;i++){
            File file=x.largest.get(i);
            View row=inflater.inflate(R.layout.item_analyzer_file,largest,false);
            ImageView icon=row.findViewById(R.id.largest_icon);
            TextView name=row.findViewById(R.id.largest_name);
            TextView meta=row.findViewById(R.id.largest_meta);

            icon.setImageResource(iconFor(file));
            name.setText(file.getName());
            meta.setText(fmt(file.length())+" · "+friendlyPath(file.getParentFile()));
            row.setBackgroundResource(android.R.drawable.list_selector_background);
            row.setOnClickListener(v->open(file));

            largest.addView(row);
        }
    }

    private int iconFor(File f){
        if(FileEngine.isImage(f))return R.drawable.ic_image;
        if(FileEngine.isVideo(f))return R.drawable.ic_video;
        if(FileEngine.isAudio(f))return R.drawable.ic_music;
        if(FileEngine.isApk(f))return R.drawable.ic_apps;
        if(FileEngine.isArchive(f))return R.drawable.ic_archive;
        if(FileEngine.isText(f))return R.drawable.ic_description;
        return R.drawable.ic_file;
    }

    private void open(File f){
        if(FileEngine.isImage(f)||FileEngine.isText(f)||FileEngine.isVideo(f)||FileEngine.isAudio(f)){
            Intent i=new Intent(this,PreviewActivity.class);
            i.putExtra("path",f.getAbsolutePath());
            startActivity(i);
            return;
        }

        Intent i=new Intent(Intent.ACTION_VIEW);
        i.setDataAndType(BlazeProvider.uriFor(f),FileEngine.mime(f));
        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try{startActivity(i);}
        catch(Exception e){Toast.makeText(this,"No app can open this file",Toast.LENGTH_SHORT).show();}
    }

    private String friendlyPath(File dir){
        if(dir==null)return "Files";
        File home=Environment.getExternalStorageDirectory();
        String base=home.getAbsolutePath();
        String full=dir.getAbsolutePath();

        if(full.equals(base))return "Internal storage";
        if(full.startsWith(base+File.separator)){
            return "Internal storage › "+full.substring(base.length()+1).replace(File.separator," › ");
        }
        return full;
    }

    private String fmt(long n){
        String[]u={"B","KB","MB","GB","TB"};
        double v=n;
        int i=0;
        while(v>=1024&&i<u.length-1){v/=1024;i++;}
        return String.format(Locale.US,v>=10?"%.0f %s":"%.1f %s",v,u[i]);
    }
}
