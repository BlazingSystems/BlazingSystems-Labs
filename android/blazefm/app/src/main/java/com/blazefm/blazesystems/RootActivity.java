package com.blazefm.blazesystems;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.chip.Chip;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class RootActivity extends Activity {
    private final ExecutorService exec=Executors.newSingleThreadExecutor();
    private final ArrayList<RootShell.Entry> entries=new ArrayList<>();
    private SimpleRowAdapter adapter;
    private TextView path,status;
    private String cwd="/";

    @Override public void onCreate(Bundle b){
        super.onCreate(b);
        setContentView(R.layout.activity_root);

        MaterialToolbar toolbar=findViewById(R.id.root_toolbar);
        toolbar.setNavigationOnClickListener(v->back());

        path=findViewById(R.id.root_path);
        status=findViewById(R.id.root_status);

        ((Chip)findViewById(R.id.root_up)).setOnClickListener(v->{cwd=RootShell.parent(cwd);load();});
        ((Chip)findViewById(R.id.root_go)).setOnClickListener(v->go());
        ((Chip)findViewById(R.id.root_refresh)).setOnClickListener(v->load());

        RecyclerView list=findViewById(R.id.root_list);
        list.setLayoutManager(new LinearLayoutManager(this));
        list.setItemAnimator(null);

        adapter=new SimpleRowAdapter(new SimpleRowAdapter.Listener(){
            @Override public void onClick(int position){
                if(!valid(position))return;
                RootShell.Entry e=entries.get(position);
                if(e.dir){cwd=e.path;load();}else preview(e);
            }
            @Override public void onMore(int position){if(valid(position))menu(entries.get(position));}
            @Override public boolean onLongClick(int position){if(valid(position))menu(entries.get(position));return true;}
        });
        list.setAdapter(adapter);

        if(!RootShell.available()){
            new MaterialAlertDialogBuilder(this)
                    .setTitle("Root unavailable")
                    .setMessage("No working su/root shell was detected. BlazeFM will not fake root access.")
                    .setPositiveButton("Close",(d,w)->finish())
                    .setOnCancelListener(d->finish())
                    .show();
        }else load();
    }

    private boolean valid(int position){return position>=0&&position<entries.size();}

    private void load(){
        path.setText(cwd);
        status.setText("Reading as root…");
        exec.execute(()->{
            try{
                List<RootShell.Entry> loaded=RootShell.list(cwd);
                runOnUiThread(()->{
                    entries.clear();entries.addAll(loaded);
                    List<SimpleRowAdapter.Row> rows=new ArrayList<>();
                    for(RootShell.Entry e:loaded){
                        rows.add(new SimpleRowAdapter.Row(
                                e.dir?R.drawable.ic_folder:R.drawable.ic_root,
                                e.name,
                                e.dir?"Folder":fmt(e.size)+" · root"
                        ));
                    }
                    adapter.submit(rows);
                    status.setText(loaded.size()+" items");
                    path.setText(cwd);
                });
            }catch(Exception e){
                runOnUiThread(()->status.setText("Root read failed: "+e.getMessage()));
            }
        });
    }

    private void go(){
        inputDialog("Root path","Absolute path",cwd,"Go",value->{
            cwd=value.isEmpty()?"/":value;
            load();
        });
    }

    private void preview(RootShell.Entry e){
        status.setText("Preparing secure preview…");
        exec.execute(()->{
            try{
                File out=new File(getCacheDir(),"root_preview_"+System.currentTimeMillis()+"_"+e.name);
                RootShell.copyFile(e.path,out);
                runOnUiThread(()->{
                    Intent i=new Intent(this,PreviewActivity.class);
                    i.putExtra("path",out.getAbsolutePath());
                    startActivity(i);
                    status.setText("Preview copy created in app cache");
                });
            }catch(Exception x){
                runOnUiThread(()->toast("Preview failed: "+x.getMessage()));
            }
        });
    }

    private void menu(RootShell.Entry e){
        String[] options=e.dir
                ?new String[]{"Open","Rename","Delete as root"}
                :new String[]{"Preview","Copy to Downloads","Rename","Delete as root"};
        new MaterialAlertDialogBuilder(this)
                .setTitle(e.name)
                .setItems(options,(d,w)->{
                    if(e.dir&&w==0){cwd=e.path;load();return;}
                    if(!e.dir&&w==0){preview(e);return;}
                    if(!e.dir&&w==1){copyOut(e);return;}
                    int offset=e.dir?1:2;
                    if(w==offset)rename(e);
                    else if(w==offset+1)delete(e);
                }).show();
    }

    private void copyOut(RootShell.Entry e){
        status.setText("Copying to Downloads…");
        exec.execute(()->{
            try{
                String saved=DownloadStore.save(this,"Root",e.name,"application/octet-stream",out->RootShell.copyTo(e.path,out));
                runOnUiThread(()->status.setText("Saved: "+saved));
            }catch(Exception x){
                runOnUiThread(()->toast("Copy failed: "+x.getMessage()));
            }
        });
    }

    private void rename(RootShell.Entry e){
        inputDialog("Rename as root","New name",e.name,"Rename",value->
                exec.execute(()->{
                    try{
                        RootShell.rename(e.path,("/".equals(cwd)?"/":cwd+"/")+value);
                        runOnUiThread(this::load);
                    }catch(Exception x){
                        runOnUiThread(()->toast("Rename failed: "+x.getMessage()));
                    }
                })
        );
    }

    private void delete(RootShell.Entry e){
        new MaterialAlertDialogBuilder(this)
                .setTitle("Permanent root delete?")
                .setMessage(e.path+"\n\nThis bypasses BlazeFM Trash.")
                .setNegativeButton("Cancel",null)
                .setPositiveButton("DELETE",(d,w)->
                        exec.execute(()->{
                            try{
                                RootShell.delete(e.path);
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
                    String value=String.valueOf(edit.getText()).trim();
                    if(!value.isEmpty())action.run(value);
                }).show();
    }

    private void back(){
        if(!"/".equals(cwd)){cwd=RootShell.parent(cwd);load();}
        else finish();
    }

    private String fmt(long n){
        String[]u={"B","KB","MB","GB"};double v=n;int i=0;
        while(v>=1024&&i<u.length-1){v/=1024;i++;}
        return String.format(Locale.US,"%.1f %s",v,u[i]);
    }

    private void toast(String s){Toast.makeText(this,s,Toast.LENGTH_LONG).show();}

    @Override public void onBackPressed(){back();}

    @Override protected void onDestroy(){
        exec.shutdownNow();
        super.onDestroy();
    }
}
