package com.blazefm.blazesystems;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.button.MaterialButton;
import com.google.android.material.progressindicator.LinearProgressIndicator;

import java.io.File;
import java.util.Locale;

public class AnalyzerActivity extends Activity {
    private TextView body;
    private LinearProgressIndicator progress;
    private MaterialButton cancelButton;
    private volatile boolean cancel;

    @Override public void onCreate(Bundle b){
        super.onCreate(b);
        setContentView(R.layout.activity_analyzer);

        MaterialToolbar toolbar=findViewById(R.id.analyzer_toolbar);
        toolbar.setNavigationOnClickListener(v->finish());
        body=findViewById(R.id.analyzer_body);
        progress=findViewById(R.id.analyzer_progress);
        cancelButton=findViewById(R.id.analyzer_cancel);
        cancelButton.setOnClickListener(v->{cancel=true;cancelButton.setEnabled(false);cancelButton.setText("Cancelling…");});

        String p=getIntent().getStringExtra("path");
        File root=p==null?android.os.Environment.getExternalStorageDirectory():new File(p);
        run(root);
    }

    private void run(File root){
        new Thread(()->{
            FileEngine.Summary x=FileEngine.analyze(root,AppPrefs.showHidden(this),new FileEngine.Progress(){
                public void update(String t,int d,int n){runOnUiThread(()->body.setText(t));}
                public boolean cancelled(){return cancel;}
            });

            String[] names={"Images","Video","Audio","Documents","APKs","Archives","Other"};
            StringBuilder b=new StringBuilder();
            b.append(root.getAbsolutePath()).append("\n\n")
             .append("Files: ").append(x.files).append("\n")
             .append("Folders: ").append(x.dirs).append("\n")
             .append("Empty folders: ").append(x.emptyDirs).append("\n")
             .append("Total file data: ").append(fmt(x.bytes)).append("\n\n")
             .append("By category\n");
            for(int i=0;i<names.length;i++)b.append(names[i]).append(": ").append(x.categoryCount[i]).append(" · ").append(fmt(x.categoryBytes[i])).append('\n');
            b.append("\nLargest files\n");
            for(File f:x.largest)b.append(fmt(f.length())).append("  ").append(f.getAbsolutePath()).append('\n');

            String out=b.toString();
            runOnUiThread(()->{
                body.setText(cancel?"Scan cancelled.\n\n"+out:out);
                progress.hide();
                cancelButton.setVisibility(View.GONE);
            });
        }).start();
    }

    private String fmt(long n){
        String[]u={"B","KB","MB","GB","TB"};double v=n;int i=0;
        while(v>=1024&&i<u.length-1){v/=1024;i++;}
        return String.format(Locale.US,v>=10?"%.0f %s":"%.1f %s",v,u[i]);
    }
}
