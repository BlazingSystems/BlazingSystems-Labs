package com.blazefm.blazesystems;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.widget.TextView;
import android.widget.Toast;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class AppManagerActivity extends Activity {
    private final ArrayList<ApplicationInfo> apps=new ArrayList<>();
    private RecyclerView list;
    private TextView status;

    @Override public void onCreate(Bundle b){
        super.onCreate(b);
        setContentView(R.layout.activity_app_manager);

        MaterialToolbar toolbar=findViewById(R.id.apps_toolbar);
        toolbar.setNavigationOnClickListener(v->finish());

        status=findViewById(R.id.apps_status);
        list=findViewById(R.id.apps_list);
        list.setLayoutManager(new LinearLayoutManager(this));
        list.setItemAnimator(null);

        load();
    }

    private void load(){
        status.setText("Loading installed apps…");
        new Thread(()->{
            PackageManager pm=getPackageManager();
            List<ApplicationInfo> loaded=pm.getInstalledApplications(PackageManager.GET_META_DATA);
            Collections.sort(loaded,(x,y)->pm.getApplicationLabel(x).toString().compareToIgnoreCase(pm.getApplicationLabel(y).toString()));
            runOnUiThread(()->{
                apps.clear();apps.addAll(loaded);
                list.setAdapter(new AppRecyclerAdapter(pm,apps,this::menu));
                status.setText(apps.size()+" apps");
            });
        }).start();
    }

    private void menu(ApplicationInfo a){
        PackageManager pm=getPackageManager();
        String label=pm.getApplicationLabel(a).toString();
        String[] options={"Launch","App info","Backup APK","Uninstall"};
        new MaterialAlertDialogBuilder(this)
                .setTitle(label)
                .setItems(options,(d,w)->{
                    if(w==0){
                        Intent i=pm.getLaunchIntentForPackage(a.packageName);
                        if(i!=null)startActivity(i);else toast("No launcher activity");
                    }else if(w==1){
                        startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,Uri.parse("package:"+a.packageName)));
                    }else if(w==2){
                        backup(a,label);
                    }else if(w==3){
                        try{startActivity(new Intent(Intent.ACTION_DELETE,Uri.parse("package:"+a.packageName)));}
                        catch(Exception e){toast("Uninstall unavailable");}
                    }
                }).show();
    }

    private void backup(ApplicationInfo a,String label){
        status.setText("Backing up "+label+"…");
        new Thread(()->{
            try{
                File src=new File(a.sourceDir);
                String safe=label.replaceAll("[^A-Za-z0-9._-]","_");
                String saved=DownloadStore.save(this,"AppBackups",safe+"_"+a.packageName+".apk","application/vnd.android.package-archive",out->{
                    try(InputStream in=new BufferedInputStream(new FileInputStream(src))){
                        byte[]b=new byte[64*1024];int n;
                        while((n=in.read(b))!=-1)out.write(b,0,n);
                    }
                });
                runOnUiThread(()->{status.setText("Backed up: "+saved);toast("APK backup complete");});
            }catch(Exception e){
                runOnUiThread(()->toast("Backup failed: "+e.getMessage()));
            }
        }).start();
    }

    private void toast(String s){Toast.makeText(this,s,Toast.LENGTH_SHORT).show();}
}
