package com.blazefm.blazesystems;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.button.MaterialButton;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import java.util.ArrayList;
import java.util.List;

public class CloudActivity extends Activity {
    private static final int ADD=701;
    private final ArrayList<String> uris=new ArrayList<>();
    private SimpleRowAdapter adapter;
    private TextView empty;

    @Override public void onCreate(Bundle b){
        super.onCreate(b);
        setContentView(R.layout.activity_cloud);

        MaterialToolbar toolbar=findViewById(R.id.cloud_toolbar);
        toolbar.setNavigationOnClickListener(v->finish());

        MaterialButton add=findViewById(R.id.cloud_add);
        add.setOnClickListener(v->add());

        empty=findViewById(R.id.cloud_empty);
        RecyclerView list=findViewById(R.id.cloud_list);
        list.setLayoutManager(new LinearLayoutManager(this));
        list.setItemAnimator(null);

        adapter=new SimpleRowAdapter(new SimpleRowAdapter.Listener(){
            @Override public void onClick(int position){open(position);}
            @Override public void onMore(int position){removePrompt(position);}
            @Override public boolean onLongClick(int position){removePrompt(position);return true;}
        });
        list.setAdapter(adapter);
        refresh();
    }

    private void open(int position){
        if(position<0||position>=uris.size())return;
        Intent i=new Intent(this,CloudBrowserActivity.class);
        i.putExtra("tree",uris.get(position));
        startActivity(i);
    }

    private void removePrompt(int position){
        if(position<0||position>=uris.size())return;
        String raw=uris.get(position);
        new MaterialAlertDialogBuilder(this)
                .setTitle("Remove saved provider?")
                .setMessage(raw)
                .setNegativeButton("Cancel",null)
                .setPositiveButton("Remove",(d,w)->{
                    release(raw);
                    AppPrefs.removeCloud(this,raw);
                    refresh();
                })
                .show();
    }

    private void add(){
        Intent i=new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION|
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION|
                Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION|
                Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        startActivityForResult(i,ADD);
    }

    @Override protected void onActivityResult(int r,int c,Intent data){
        super.onActivityResult(r,c,data);
        if(r==ADD&&c==RESULT_OK&&data!=null&&data.getData()!=null){
            Uri u=data.getData();
            try{
                getContentResolver().takePersistableUriPermission(u,Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            }catch(Exception first){
                try{getContentResolver().takePersistableUriPermission(u,Intent.FLAG_GRANT_READ_URI_PERMISSION);}
                catch(Exception ignored){}
            }
            AppPrefs.addCloud(this,u.toString());
            refresh();
        }
    }

    private void release(String raw){
        Uri u=Uri.parse(raw);
        try{
            getContentResolver().releasePersistableUriPermission(u,Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        }catch(Exception first){
            try{getContentResolver().releasePersistableUriPermission(u,Intent.FLAG_GRANT_READ_URI_PERMISSION);}
            catch(Exception ignored){}
        }
    }

    private void refresh(){
        uris.clear();
        uris.addAll(AppPrefs.cloud(this));
        List<SimpleRowAdapter.Row> rows=new ArrayList<>();
        for(String raw:uris){
            Uri u=Uri.parse(raw);
            rows.add(new SimpleRowAdapter.Row(R.drawable.ic_cloud,friendly(u),raw));
        }
        adapter.submit(rows);
        empty.setVisibility(rows.isEmpty()?View.VISIBLE:View.GONE);
    }

    private String friendly(Uri u){
        try{
            String id=DocumentsContract.getTreeDocumentId(u);
            return id==null?"Document provider":id;
        }catch(Exception e){
            return "Document provider";
        }
    }
}
