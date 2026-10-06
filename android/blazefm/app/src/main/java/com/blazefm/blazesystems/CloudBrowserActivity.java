package com.blazefm.blazesystems;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
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

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;

public class CloudBrowserActivity extends Activity {
    static final int PICK_UPLOAD=702;
    static final class E{String id,name,mime;long size;boolean dir;}

    private Uri tree,current;
    private final ArrayDeque<Uri> stack=new ArrayDeque<>();
    private final ArrayList<E> entries=new ArrayList<>();
    private SimpleRowAdapter adapter;
    private TextView path,status;

    @Override public void onCreate(Bundle b){
        super.onCreate(b);
        String s=getIntent().getStringExtra("tree");
        if(s==null){finish();return;}

        tree=Uri.parse(s);
        current=DocumentsContract.buildDocumentUriUsingTree(tree,DocumentsContract.getTreeDocumentId(tree));
        setContentView(R.layout.activity_cloud_browser);

        MaterialToolbar toolbar=findViewById(R.id.cloud_browser_toolbar);
        toolbar.setNavigationOnClickListener(v->up());

        path=findViewById(R.id.cloud_browser_path);
        status=findViewById(R.id.cloud_browser_status);

        ((Chip)findViewById(R.id.cloud_up)).setOnClickListener(v->up());
        ((Chip)findViewById(R.id.cloud_upload)).setOnClickListener(v->pickUpload());
        ((Chip)findViewById(R.id.cloud_new_folder)).setOnClickListener(v->newFolder());
        ((Chip)findViewById(R.id.cloud_refresh)).setOnClickListener(v->load());

        RecyclerView list=findViewById(R.id.cloud_browser_list);
        list.setLayoutManager(new LinearLayoutManager(this));
        list.setItemAnimator(null);

        adapter=new SimpleRowAdapter(new SimpleRowAdapter.Listener(){
            @Override public void onClick(int position){if(valid(position))open(entries.get(position));}
            @Override public void onMore(int position){if(valid(position))menu(entries.get(position));}
            @Override public boolean onLongClick(int position){if(valid(position))menu(entries.get(position));return true;}
        });
        list.setAdapter(adapter);
        load();
    }

    private boolean valid(int position){return position>=0&&position<entries.size();}

    private void load(){
        status.setText("Loading…");
        new Thread(()->{
            ArrayList<E> out=new ArrayList<>();
            try{
                String id=DocumentsContract.getDocumentId(current);
                Uri children=DocumentsContract.buildChildDocumentsUriUsingTree(tree,id);
                String[] projection={
                        DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                        DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                        DocumentsContract.Document.COLUMN_MIME_TYPE,
                        DocumentsContract.Document.COLUMN_SIZE
                };
                try(Cursor c=getContentResolver().query(children,projection,null,null,null)){
                    if(c!=null)while(c.moveToNext()){
                        E e=new E();
                        e.id=c.getString(0);
                        e.name=c.getString(1);
                        e.mime=c.getString(2);
                        e.size=c.isNull(3)?0:c.getLong(3);
                        e.dir=DocumentsContract.Document.MIME_TYPE_DIR.equals(e.mime);
                        out.add(e);
                    }
                }
                Collections.sort(out,(a,b)->{
                    if(a.dir!=b.dir)return a.dir?-1:1;
                    return a.name.compareToIgnoreCase(b.name);
                });
                runOnUiThread(()->{
                    entries.clear();entries.addAll(out);
                    List<SimpleRowAdapter.Row> rows=new ArrayList<>();
                    for(E e:out){
                        rows.add(new SimpleRowAdapter.Row(
                                e.dir?R.drawable.ic_folder:R.drawable.ic_cloud,
                                e.name,
                                e.dir?"Folder":fmt(e.size)+(e.mime==null?"":" · "+e.mime)
                        ));
                    }
                    adapter.submit(rows);
                    path.setText(current.toString());
                    status.setText(out.size()+" items");
                });
            }catch(Exception e){
                runOnUiThread(()->status.setText("Provider error: "+e.getMessage()));
            }
        }).start();
    }

    private Uri uri(E e){return DocumentsContract.buildDocumentUriUsingTree(tree,e.id);}

    private void open(E e){
        if(e.dir){
            stack.push(current);
            current=uri(e);
            load();
        }else{
            Intent i=new Intent(Intent.ACTION_VIEW);
            i.setDataAndType(uri(e),e.mime==null?"*/*":e.mime);
            i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            try{startActivity(i);}catch(Exception x){toast("No app can open this file");}
        }
    }

    private void menu(E e){
        String[] options=e.dir
                ?new String[]{"Rename","Delete"}
                :new String[]{"Open","Save to Downloads","Rename","Delete"};
        new MaterialAlertDialogBuilder(this)
                .setTitle(e.name)
                .setItems(options,(d,w)->{
                    if(!e.dir&&w==0){open(e);return;}
                    if(!e.dir&&w==1){download(e);return;}
                    int shift=e.dir?0:2;
                    if(w==shift)rename(e);
                    else if(w==shift+1)delete(e);
                }).show();
    }

    private void download(E e){
        status.setText("Downloading…");
        new Thread(()->{
            try{
                String saved=DownloadStore.save(this,"Cloud",e.name,e.mime,out->{
                    try(InputStream in=getContentResolver().openInputStream(uri(e))){
                        if(in==null)throw new IOException("Provider returned no data");
                        byte[]b=new byte[64*1024];int n;
                        while((n=in.read(b))!=-1)out.write(b,0,n);
                    }
                });
                runOnUiThread(()->status.setText("Saved: "+saved));
            }catch(Exception x){
                runOnUiThread(()->toast("Save failed: "+x.getMessage()));
            }
        }).start();
    }

    private void rename(E e){
        inputDialog("Rename","New name",e.name,"Rename",value->{
            try{
                DocumentsContract.renameDocument(getContentResolver(),uri(e),value);
                load();
            }catch(Exception x){toast("Rename failed: "+x.getMessage());}
        });
    }

    private void delete(E e){
        new MaterialAlertDialogBuilder(this)
                .setTitle("Delete from provider?")
                .setMessage(e.name)
                .setNegativeButton("Cancel",null)
                .setPositiveButton("Delete",(d,w)->{
                    try{
                        DocumentsContract.deleteDocument(getContentResolver(),uri(e));
                        load();
                    }catch(Exception x){toast("Delete failed: "+x.getMessage());}
                }).show();
    }

    private void newFolder(){
        inputDialog("New folder","Folder name","","Create",value->{
            try{
                DocumentsContract.createDocument(getContentResolver(),current,DocumentsContract.Document.MIME_TYPE_DIR,value);
                load();
            }catch(Exception x){toast("Create failed: "+x.getMessage());}
        });
    }

    private interface TextAction{void run(String text);}

    private void inputDialog(String title,String hint,String initial,String positive,TextAction action){
        TextInputLayout layout=new TextInputLayout(this);
        layout.setHint(hint);
        TextInputEditText edit=new TextInputEditText(layout.getContext());
        edit.setSingleLine(true);
        edit.setText(initial);
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
                })
                .show();
    }

    private void pickUpload(){
        Intent i=new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.setType("*/*");
        i.addCategory(Intent.CATEGORY_OPENABLE);
        startActivityForResult(i,PICK_UPLOAD);
    }

    @Override protected void onActivityResult(int r,int c,Intent data){
        super.onActivityResult(r,c,data);
        if(r==PICK_UPLOAD&&c==RESULT_OK&&data!=null&&data.getData()!=null)upload(data.getData());
    }

    private void upload(Uri src){
        status.setText("Uploading…");
        new Thread(()->{
            try{
                String name="upload.bin";
                String mime=getContentResolver().getType(src);
                try(Cursor c=getContentResolver().query(src,new String[]{OpenableColumns.DISPLAY_NAME},null,null,null)){
                    if(c!=null&&c.moveToFirst())name=c.getString(0);
                }
                Uri out=DocumentsContract.createDocument(getContentResolver(),current,mime==null?"application/octet-stream":mime,name);
                if(out==null)throw new IOException("Provider refused create");
                try(InputStream in=getContentResolver().openInputStream(src);OutputStream o=getContentResolver().openOutputStream(out)){
                    if(in==null||o==null)throw new IOException("Provider stream unavailable");
                    byte[]b=new byte[64*1024];int n;
                    while((n=in.read(b))!=-1)o.write(b,0,n);
                }
                runOnUiThread(this::load);
            }catch(Exception e){
                runOnUiThread(()->toast("Upload failed: "+e.getMessage()));
            }
        }).start();
    }

    private void up(){
        if(!stack.isEmpty()){
            current=stack.pop();
            load();
        }else finish();
    }

    private String fmt(long n){
        String[]u={"B","KB","MB","GB"};double v=n;int i=0;
        while(v>=1024&&i<u.length-1){v/=1024;i++;}
        return String.format(Locale.US,"%.1f %s",v,u[i]);
    }

    private void toast(String s){Toast.makeText(this,s,Toast.LENGTH_SHORT).show();}

    @Override public void onBackPressed(){up();}
}
