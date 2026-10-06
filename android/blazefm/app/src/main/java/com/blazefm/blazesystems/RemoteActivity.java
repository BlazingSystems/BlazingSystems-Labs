package com.blazefm.blazesystems;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.button.MaterialButton;
import com.google.android.material.chip.Chip;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.textfield.MaterialAutoCompleteTextView;
import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

public class RemoteActivity extends Activity {
    private static final int PICK=703;

    private MaterialAutoCompleteTextView proto;
    private TextInputEditText host,port,user,pass,share,domain,start;
    private TextInputLayout shareLayout,domainLayout;
    private View connectScroll,browserBox;
    private MaterialButton connectButton;
    private TextView path,status;
    private SimpleRowAdapter adapter;

    private final ArrayList<RemoteSession.Entry> entries=new ArrayList<>();
    private final ExecutorService exec=Executors.newSingleThreadExecutor();
    private RemoteSession session;
    private String cwd="/";

    @Override public void onCreate(Bundle b){
        super.onCreate(b);
        setContentView(R.layout.activity_remote);

        MaterialToolbar toolbar=findViewById(R.id.remote_toolbar);
        toolbar.setNavigationOnClickListener(v->back());

        connectScroll=findViewById(R.id.remote_connect_scroll);
        browserBox=findViewById(R.id.remote_browser_box);
        path=findViewById(R.id.remote_path);
        status=findViewById(R.id.remote_status);
        connectButton=findViewById(R.id.remote_connect);

        proto=findViewById(R.id.remote_proto);
        host=findViewById(R.id.remote_host);
        port=findViewById(R.id.remote_port);
        user=findViewById(R.id.remote_user);
        pass=findViewById(R.id.remote_pass);
        share=findViewById(R.id.remote_share);
        domain=findViewById(R.id.remote_domain);
        start=findViewById(R.id.remote_start);
        shareLayout=findViewById(R.id.remote_share_layout);
        domainLayout=findViewById(R.id.remote_domain_layout);

        String[] protocols={"SMB","FTP","SFTP"};
        proto.setAdapter(new ArrayAdapter<>(this,android.R.layout.simple_list_item_1,protocols));
        proto.setText("SMB",false);
        proto.setOnItemClickListener((parent,view,position,id)->updateProtocolFields());
        updateProtocolFields();

        connectButton.setOnClickListener(v->connect());
        ((Chip)findViewById(R.id.remote_up)).setOnClickListener(v->up());
        ((Chip)findViewById(R.id.remote_upload)).setOnClickListener(v->pick());
        ((Chip)findViewById(R.id.remote_new_folder)).setOnClickListener(v->mkdir());
        ((Chip)findViewById(R.id.remote_disconnect)).setOnClickListener(v->disconnect());

        RecyclerView list=findViewById(R.id.remote_list);
        list.setLayoutManager(new LinearLayoutManager(this));
        list.setItemAnimator(null);
        adapter=new SimpleRowAdapter(new SimpleRowAdapter.Listener(){
            @Override public void onClick(int position){
                if(valid(position))open(entries.get(position));
            }
            @Override public void onMore(int position){
                if(valid(position))menu(entries.get(position));
            }
            @Override public boolean onLongClick(int position){
                if(valid(position))menu(entries.get(position));
                return true;
            }
        });
        list.setAdapter(adapter);
    }

    private boolean valid(int position){return position>=0&&position<entries.size();}

    private void updateProtocolFields(){
        boolean smb="SMB".equalsIgnoreCase(String.valueOf(proto.getText()));
        shareLayout.setVisibility(smb?View.VISIBLE:View.GONE);
        domainLayout.setVisibility(smb?View.VISIBLE:View.GONE);
    }

    private String value(TextInputEditText edit){
        return edit.getText()==null?"":edit.getText().toString().trim();
    }

    private void connect(){
        String pr=String.valueOf(proto.getText()).trim();
        if(pr.isEmpty())pr="SMB";
        String h=value(host);
        if(h.isEmpty()){host.setError("Host required");return;}

        int parsedPort=0;
        try{
            String x=value(port);
            if(!x.isEmpty())parsedPort=Integer.parseInt(x);
        }catch(Exception e){
            port.setError("Invalid port");
            return;
        }
        final int fp=parsedPort;
        final String protocol=pr;

        connectButton.setEnabled(false);
        connectButton.setText("Connecting…");

        exec.execute(()->{
            try{
                RemoteSession s=RemoteFactory.connect(
                        protocol,h,fp,value(user),value(pass),value(share),value(domain),
                        getFilesDir(),this::confirmHostKey
                );
                session=s;
                String initial=value(start);
                cwd=initial.isEmpty()?"/":initial;
                runOnUiThread(()->{
                    pass.setText("");
                    connectButton.setEnabled(true);
                    connectButton.setText("Connect");
                    connectScroll.setVisibility(View.GONE);
                    browserBox.setVisibility(View.VISIBLE);
                    load();
                });
            }catch(Exception e){
                runOnUiThread(()->{
                    connectButton.setEnabled(true);
                    connectButton.setText("Connect");
                    toast("Connect failed: "+e.getMessage());
                });
            }
        });
    }

    private void load(){
        if(session==null)return;
        status.setText("Loading…");
        path.setText(cwd);
        exec.execute(()->{
            try{
                List<RemoteSession.Entry> loaded=session.list(cwd);
                runOnUiThread(()->{
                    entries.clear();entries.addAll(loaded);
                    List<SimpleRowAdapter.Row> rows=new ArrayList<>();
                    for(RemoteSession.Entry e:loaded){
                        rows.add(new SimpleRowAdapter.Row(
                                e.dir?R.drawable.ic_folder:R.drawable.ic_network,
                                e.name,
                                e.dir?"Folder":fmt(e.size)
                        ));
                    }
                    adapter.submit(rows);
                    status.setText(loaded.size()+" items");
                    path.setText(cwd);
                });
            }catch(Exception e){
                runOnUiThread(()->status.setText("Error: "+e.getMessage()));
            }
        });
    }

    private void open(RemoteSession.Entry e){
        if(e.dir){cwd=e.path;load();}
        else download(e);
    }

    private void menu(RemoteSession.Entry e){
        String[] options=e.dir
                ?new String[]{"Open","Rename","Delete"}
                :new String[]{"Download","Rename","Delete"};
        new MaterialAlertDialogBuilder(this)
                .setTitle(e.name)
                .setItems(options,(d,w)->{
                    if(w==0){
                        if(e.dir){cwd=e.path;load();}
                        else download(e);
                    }else if(w==1)rename(e);
                    else if(w==2)delete(e);
                }).show();
    }

    private void download(RemoteSession.Entry e){
        status.setText("Downloading "+e.name+"…");
        exec.execute(()->{
            try{
                String saved=DownloadStore.save(this,"Remote",e.name,"application/octet-stream",out->session.download(e.path,out));
                runOnUiThread(()->status.setText("Saved: "+saved));
            }catch(Exception x){
                runOnUiThread(()->toast("Download failed: "+x.getMessage()));
            }
        });
    }

    private void pick(){
        Intent i=new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        startActivityForResult(i,PICK);
    }

    @Override protected void onActivityResult(int r,int c,Intent data){
        super.onActivityResult(r,c,data);
        if(r==PICK&&c==RESULT_OK&&data!=null&&data.getData()!=null)upload(data.getData());
    }

    private void upload(Uri u){
        status.setText("Uploading…");
        exec.execute(()->{
            try{
                String name="upload.bin";
                try(Cursor c=getContentResolver().query(u,new String[]{OpenableColumns.DISPLAY_NAME},null,null,null)){
                    if(c!=null&&c.moveToFirst())name=c.getString(0);
                }
                String dst=RemoteFactory.join(cwd,name);
                try(InputStream in=getContentResolver().openInputStream(u)){
                    if(in==null)throw new IOException("Cannot read selected file");
                    session.upload(dst,in);
                }
                runOnUiThread(()->{status.setText("Upload complete");load();});
            }catch(Exception e){
                runOnUiThread(()->toast("Upload failed: "+e.getMessage()));
            }
        });
    }

    private void mkdir(){
        inputDialog("New remote folder","Folder name","","Create",value->
                exec.execute(()->{
                    try{
                        session.mkdir(RemoteFactory.join(cwd,value));
                        runOnUiThread(this::load);
                    }catch(Exception x){
                        runOnUiThread(()->toast("Create failed: "+x.getMessage()));
                    }
                })
        );
    }

    private void rename(RemoteSession.Entry e){
        inputDialog("Rename","New name",e.name,"Rename",value->
                exec.execute(()->{
                    try{
                        session.rename(e.path,RemoteFactory.join(cwd,value));
                        runOnUiThread(this::load);
                    }catch(Exception x){
                        runOnUiThread(()->toast("Rename failed: "+x.getMessage()));
                    }
                })
        );
    }

    private void delete(RemoteSession.Entry e){
        new MaterialAlertDialogBuilder(this)
                .setTitle("Delete remote item?")
                .setMessage(e.name+(e.dir?"\nFolder must be empty.":""))
                .setNegativeButton("Cancel",null)
                .setPositiveButton("Delete",(d,w)->
                        exec.execute(()->{
                            try{
                                session.delete(e.path,e.dir);
                                runOnUiThread(this::load);
                            }catch(Exception x){
                                runOnUiThread(()->toast("Delete failed: "+x.getMessage()));
                            }
                        })
                ).show();
    }

    private interface TextAction{void run(String value);}

    private void inputDialog(String title,String hint,String initial,String positive,TextAction action){
        TextInputLayout layout=new TextInputLayout(this);
        layout.setHint(hint);
        TextInputEditText edit=new TextInputEditText(layout.getContext());
        edit.setSingleLine(true);
        edit.setText(initial);
        edit.setSelectAllOnFocus(true);
        layout.addView(edit,new TextInputLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,ViewGroup.LayoutParams.WRAP_CONTENT));

        LinearLayout wrap=new LinearLayout(this);
        wrap.setPadding(Ui.dp(this,20),Ui.dp(this,8),Ui.dp(this,20),0);
        wrap.addView(layout,new LinearLayout.LayoutParams(-1,-2));

        new MaterialAlertDialogBuilder(this)
                .setTitle(title)
                .setView(wrap)
                .setNegativeButton("Cancel",null)
                .setPositiveButton(positive,(d,w)->{
                    String v=String.valueOf(edit.getText()).trim();
                    if(!v.isEmpty())action.run(v);
                })
                .show();
    }

    private void up(){
        if(session==null)return;
        String parent=session.parent(cwd);
        if(parent.equals(cwd))return;
        cwd=parent;
        load();
    }

    private void disconnect(){
        try{if(session!=null)session.close();}catch(Exception ignored){}
        session=null;
        entries.clear();
        if(adapter!=null)adapter.submit(new ArrayList<>());
        browserBox.setVisibility(View.GONE);
        connectScroll.setVisibility(View.VISIBLE);
    }

    private void back(){
        if(session==null){finish();return;}
        String parent=session.parent(cwd);
        if(!parent.equals(cwd)){cwd=parent;load();}
        else disconnect();
    }

    private String fmt(long n){
        String[]u={"B","KB","MB","GB"};double v=n;int i=0;
        while(v>=1024&&i<u.length-1){v/=1024;i++;}
        return String.format(Locale.US,"%.1f %s",v,u[i]);
    }

    private boolean confirmHostKey(String message){
        CountDownLatch latch=new CountDownLatch(1);
        final boolean[] trusted={false};
        runOnUiThread(()->{
            androidx.appcompat.app.AlertDialog dialog=new MaterialAlertDialogBuilder(this)
                    .setTitle("SFTP host key")
                    .setMessage(message+"\n\nOnly trust this host if the fingerprint matches the server you intended to reach.")
                    .setPositiveButton("Trust",(d,w)->{trusted[0]=true;latch.countDown();})
                    .setNegativeButton("Cancel",(d,w)->latch.countDown())
                    .create();
            dialog.setOnCancelListener(d->latch.countDown());
            dialog.show();
        });
        try{return latch.await(2,TimeUnit.MINUTES)&&trusted[0];}
        catch(InterruptedException e){Thread.currentThread().interrupt();return false;}
    }

    private void toast(String s){Toast.makeText(this,s,Toast.LENGTH_LONG).show();}

    @Override public void onBackPressed(){back();}

    @Override protected void onDestroy(){
        try{if(session!=null)session.close();}catch(Exception ignored){}
        session=null;
        exec.shutdownNow();
        super.onDestroy();
    }
}
