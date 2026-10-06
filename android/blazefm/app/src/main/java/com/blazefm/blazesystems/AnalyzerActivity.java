package com.blazefm.blazesystems;

import android.app.*;
import android.os.*;
import android.widget.*;
import java.io.File;
import java.util.Locale;

public class AnalyzerActivity extends Activity {
    private TextView body; private volatile boolean cancel;
    @Override public void onCreate(Bundle b){super.onCreate(b);LinearLayout r=new LinearLayout(this);r.setOrientation(LinearLayout.VERTICAL);TextView h=Ui.text(this,"Storage Analyzer",20);h.setTextColor(android.graphics.Color.WHITE);h.setBackgroundColor(Ui.ORANGE_DARK);r.addView(h);body=Ui.text(this,"Scanning…",13);ScrollView s=new ScrollView(this);s.addView(body);r.addView(s,new LinearLayout.LayoutParams(-1,0,1));Button c=Ui.button(this,"Cancel");c.setOnClickListener(v->{cancel=true;c.setEnabled(false);});r.addView(c);setContentView(r);String p=getIntent().getStringExtra("path");File root=p==null?android.os.Environment.getExternalStorageDirectory():new File(p);run(root);}
    private void run(File root){new Thread(()->{FileEngine.Summary x=FileEngine.analyze(root,AppPrefs.showHidden(this),new FileEngine.Progress(){public void update(String t,int d,int n){runOnUiThread(()->body.setText(t));}public boolean cancelled(){return cancel;}});String[] names={"Images","Video","Audio","Documents","APKs","Archives","Other"};StringBuilder b=new StringBuilder();b.append(root.getAbsolutePath()).append("\n\n").append("Files: ").append(x.files).append("\nFolders: ").append(x.dirs).append("\nEmpty folders: ").append(x.emptyDirs).append("\nTotal file data: ").append(fmt(x.bytes)).append("\n\nBy category\n");for(int i=0;i<names.length;i++)b.append(names[i]).append(": ").append(x.categoryCount[i]).append(" · ").append(fmt(x.categoryBytes[i])).append('\n');b.append("\nLargest files\n");for(File f:x.largest)b.append(fmt(f.length())).append("  ").append(f.getAbsolutePath()).append('\n');String out=b.toString();runOnUiThread(()->body.setText(out));}).start();}
    private String fmt(long n){String[]u={"B","KB","MB","GB","TB"};double v=n;int i=0;while(v>=1024&&i<u.length-1){v/=1024;i++;}return String.format(Locale.US,v>=10?"%.0f %s":"%.1f %s",v,u[i]);}
}
