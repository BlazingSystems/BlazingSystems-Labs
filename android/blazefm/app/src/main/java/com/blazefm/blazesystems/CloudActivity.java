package com.blazefm.blazesystems;

import android.app.*;
import android.content.*;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.widget.*;
import java.util.*;

public class CloudActivity extends Activity {
    private static final int ADD=701; private ListView list; private final ArrayList<String> uris=new ArrayList<>();
    @Override public void onCreate(Bundle b){super.onCreate(b);build();refresh();}
    private void build(){LinearLayout r=new LinearLayout(this);r.setOrientation(LinearLayout.VERTICAL);r.setBackgroundColor(Ui.BG);r.addView(Ui.screenHeader(this,"Cloud & Documents","Storage Access Framework providers"));TextView note=Ui.text(this,"Connect an installed document provider without storing its account password in BlazeFM.",12);note.setTextColor(Ui.MUTED);note.setBackground(Ui.rounded(this,Ui.SURFACE,16,Ui.BORDER,1));note.setPadding(Ui.dp(this,14),Ui.dp(this,12),Ui.dp(this,14),Ui.dp(this,12));LinearLayout noteWrap=new LinearLayout(this);noteWrap.setPadding(Ui.dp(this,12),Ui.dp(this,10),Ui.dp(this,12),0);noteWrap.addView(note,new LinearLayout.LayoutParams(-1,-2));r.addView(noteWrap);Button add=Ui.primaryButton(this,"＋  Add provider or folder");add.setOnClickListener(v->add());LinearLayout addWrap=Ui.toolbar(this);Ui.equalAdd(addWrap,add);r.addView(addWrap);list=new ListView(this);Ui.prepareList(this,list);r.addView(list,new LinearLayout.LayoutParams(-1,0,1));setContentView(r);list.setOnItemClickListener((p,v,pos,id)->{Intent i=new Intent(this,CloudBrowserActivity.class);i.putExtra("tree",uris.get(pos));startActivity(i);});list.setOnItemLongClickListener((p,v,pos,id)->{String u=uris.get(pos);new AlertDialog.Builder(this).setTitle("Remove saved provider?").setMessage(u).setPositiveButton("Remove",(d,w)->{release(u);AppPrefs.removeCloud(this,u);refresh();}).setNegativeButton("Cancel",null).show();return true;});}
    private void add(){Intent i=new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_WRITE_URI_PERMISSION|Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION|Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);startActivityForResult(i,ADD);}
    @Override protected void onActivityResult(int r,int c,Intent data){super.onActivityResult(r,c,data);if(r==ADD&&c==RESULT_OK&&data!=null&&data.getData()!=null){Uri u=data.getData();try{getContentResolver().takePersistableUriPermission(u,Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_WRITE_URI_PERMISSION);}catch(Exception first){try{getContentResolver().takePersistableUriPermission(u,Intent.FLAG_GRANT_READ_URI_PERMISSION);}catch(Exception ignored){}}AppPrefs.addCloud(this,u.toString());refresh();}}
    private void release(String raw){Uri u=Uri.parse(raw);try{getContentResolver().releasePersistableUriPermission(u,Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_WRITE_URI_PERMISSION);}catch(Exception first){try{getContentResolver().releasePersistableUriPermission(u,Intent.FLAG_GRANT_READ_URI_PERMISSION);}catch(Exception ignored){}}}
    private void refresh(){uris.clear();uris.addAll(AppPrefs.cloud(this));String[]a=new String[uris.size()];for(int i=0;i<a.length;i++){String u=uris.get(i);a[i]=friendly(Uri.parse(u))+"\n"+u;}list.setAdapter(new ModernListAdapter(this,a,"☁"));}
    private String friendly(Uri u){try{String id=DocumentsContract.getTreeDocumentId(u);return id==null?"Document provider":id;}catch(Exception e){return "Document provider";}}
}
