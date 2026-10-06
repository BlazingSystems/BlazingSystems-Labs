package com.blazefm.blazesystems;

import android.app.*;
import android.content.*;
import android.content.pm.*;
import android.net.Uri;
import android.os.*;
import android.provider.Settings;
import android.widget.*;
import java.io.*;
import java.util.*;

public class AppManagerActivity extends Activity {
    private final ArrayList<ApplicationInfo> apps=new ArrayList<>(); private ListView list; private TextView status;
    @Override public void onCreate(Bundle b){super.onCreate(b);LinearLayout r=new LinearLayout(this);r.setOrientation(LinearLayout.VERTICAL);r.setBackgroundColor(Ui.BG);r.addView(Ui.screenHeader(this,"App Manager","Installed apps, APK backup and uninstall"));status=Ui.text(this,"Loading installed apps…",11);status.setTextColor(Ui.MUTED);status.setPadding(Ui.dp(this,16),Ui.dp(this,8),Ui.dp(this,16),Ui.dp(this,2));r.addView(status);list=new ListView(this);Ui.prepareList(this,list);r.addView(list,new LinearLayout.LayoutParams(-1,0,1));setContentView(r);load();list.setOnItemClickListener((p,v,pos,id)->menu(apps.get(pos)));}
    private void load(){new Thread(()->{PackageManager pm=getPackageManager();List<ApplicationInfo>a=pm.getInstalledApplications(PackageManager.GET_META_DATA);Collections.sort(a,(x,y)->pm.getApplicationLabel(x).toString().compareToIgnoreCase(pm.getApplicationLabel(y).toString()));apps.clear();apps.addAll(a);String[]rows=new String[a.size()];for(int i=0;i<a.size();i++){ApplicationInfo x=a.get(i);rows[i]=pm.getApplicationLabel(x)+"\n"+x.packageName+(((x.flags&ApplicationInfo.FLAG_SYSTEM)!=0)?" · system":"");}runOnUiThread(()->{list.setAdapter(new ModernListAdapter(this,rows,"A"));status.setText(a.size()+" apps");});}).start();}
    private void menu(ApplicationInfo a){PackageManager pm=getPackageManager();String label=pm.getApplicationLabel(a).toString();String[]o={"Launch","App info","Backup APK","Uninstall"};new AlertDialog.Builder(this).setTitle(label).setItems(o,(d,w)->{if(w==0){Intent i=pm.getLaunchIntentForPackage(a.packageName);if(i!=null)startActivity(i);else toast("No launcher activity");}if(w==1)startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,Uri.parse("package:"+a.packageName)));if(w==2)backup(a,label);if(w==3){try{startActivity(new Intent(Intent.ACTION_DELETE,Uri.parse("package:"+a.packageName)));}catch(Exception e){toast("Uninstall unavailable");}}}).show();}
    private void backup(ApplicationInfo a,String label){status.setText("Backing up "+label+"…");new Thread(()->{try{File src=new File(a.sourceDir);String safe=label.replaceAll("[^A-Za-z0-9._-]","_");String saved=DownloadStore.save(this,"AppBackups",safe+"_"+a.packageName+".apk","application/vnd.android.package-archive",out->{try(InputStream in=new BufferedInputStream(new FileInputStream(src))){byte[]b=new byte[64*1024];int n;while((n=in.read(b))!=-1)out.write(b,0,n);}});runOnUiThread(()->{status.setText("Backed up: "+saved);toast("APK backup complete");});}catch(Exception e){runOnUiThread(()->toast("Backup failed: "+e.getMessage()));}}).start();}
    private void toast(String s){Toast.makeText(this,s,Toast.LENGTH_SHORT).show();}
}
