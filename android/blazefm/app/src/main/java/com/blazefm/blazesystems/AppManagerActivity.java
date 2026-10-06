package com.blazefm.blazesystems;

import android.app.*;
import android.content.*;
import android.content.pm.*;
import android.net.Uri;
import android.os.*;
import android.provider.Settings;
import android.widget.*;
import java.io.File;
import java.util.*;

public class AppManagerActivity extends Activity {
    private final ArrayList<ApplicationInfo> apps=new ArrayList<>(); private ListView list; private TextView status;
    @Override public void onCreate(Bundle b){super.onCreate(b);LinearLayout r=new LinearLayout(this);r.setOrientation(LinearLayout.VERTICAL);TextView h=Ui.text(this,"App Manager",20);h.setTextColor(android.graphics.Color.WHITE);h.setBackgroundColor(Ui.ORANGE_DARK);r.addView(h);list=new ListView(this);r.addView(list,new LinearLayout.LayoutParams(-1,0,1));status=Ui.text(this,"Loading installed apps…",11);r.addView(status);setContentView(r);load();list.setOnItemClickListener((p,v,pos,id)->menu(apps.get(pos)));}
    private void load(){new Thread(()->{PackageManager pm=getPackageManager();List<ApplicationInfo>a=pm.getInstalledApplications(PackageManager.GET_META_DATA);Collections.sort(a,(x,y)->pm.getApplicationLabel(x).toString().compareToIgnoreCase(pm.getApplicationLabel(y).toString()));apps.clear();apps.addAll(a);String[]rows=new String[a.size()];for(int i=0;i<a.size();i++){ApplicationInfo x=a.get(i);rows[i]=pm.getApplicationLabel(x)+"\n"+x.packageName+(((x.flags&ApplicationInfo.FLAG_SYSTEM)!=0)?" · system":"");}runOnUiThread(()->{list.setAdapter(new ArrayAdapter<String>(this,android.R.layout.simple_list_item_1,rows));status.setText(a.size()+" apps");});}).start();}
    private void menu(ApplicationInfo a){PackageManager pm=getPackageManager();String label=pm.getApplicationLabel(a).toString();String[]o={"Launch","App info","Backup APK","Uninstall"};new AlertDialog.Builder(this).setTitle(label).setItems(o,(d,w)->{if(w==0){Intent i=pm.getLaunchIntentForPackage(a.packageName);if(i!=null)startActivity(i);else toast("No launcher activity");}if(w==1)startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,Uri.parse("package:"+a.packageName)));if(w==2)backup(a,label);if(w==3){try{startActivity(new Intent(Intent.ACTION_DELETE,Uri.parse("package:"+a.packageName)));}catch(Exception e){toast("Uninstall unavailable");}}}).show();}
    private void backup(ApplicationInfo a,String label){status.setText("Backing up "+label+"…");new Thread(()->{try{File src=new File(a.sourceDir);File dir=new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS),"BlazeFM/AppBackups");dir.mkdirs();String safe=label.replaceAll("[^A-Za-z0-9._-]","_");File dst=FileEngine.unique(dir,safe+"_"+a.packageName+".apk");FileEngine.copy(src,dst);runOnUiThread(()->{status.setText("Backed up: "+dst.getAbsolutePath());toast("APK backup complete");});}catch(Exception e){runOnUiThread(()->toast("Backup failed: "+e.getMessage()));}}).start();}
    private void toast(String s){Toast.makeText(this,s,Toast.LENGTH_SHORT).show();}
}
