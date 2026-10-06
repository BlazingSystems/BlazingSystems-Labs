package com.blazefm.blazesystems;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.TextView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.button.MaterialButton;
import com.google.android.material.progressindicator.LinearProgressIndicator;
import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import java.io.File;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;

public class SearchActivity extends Activity {
    public static final String EXTRA_BASE="base";
    public static final String EXTRA_TARGET="target";

    private final ArrayList<File> results=new ArrayList<>();
    private final HashSet<String> selected=new HashSet<>();
    private TextInputLayout queryLayout;
    private TextInputEditText query;
    private TextView scope,status,emptyTitle,emptyBody;
    private LinearProgressIndicator progress;
    private RecyclerView list;
    private View empty;
    private File base;
    private volatile int generation;

    @Override public void onCreate(Bundle state){
        super.onCreate(state);
        setContentView(R.layout.activity_search);

        String raw=getIntent().getStringExtra(EXTRA_BASE);
        base=raw==null?Environment.getExternalStorageDirectory():new File(raw);
        if(!base.isDirectory())base=Environment.getExternalStorageDirectory();

        MaterialToolbar toolbar=findViewById(R.id.search_toolbar);
        toolbar.setNavigationOnClickListener(v->finish());

        queryLayout=findViewById(R.id.search_query_layout);
        query=findViewById(R.id.search_query);
        scope=findViewById(R.id.search_scope);
        status=findViewById(R.id.search_status);
        progress=findViewById(R.id.search_progress);
        list=findViewById(R.id.search_results);
        empty=findViewById(R.id.search_empty);
        emptyTitle=findViewById(R.id.search_empty_title);
        emptyBody=findViewById(R.id.search_empty_body);
        MaterialButton go=findViewById(R.id.search_go);

        scope.setText("Searching in "+friendlyPath(base));
        list.setLayoutManager(new LinearLayoutManager(this));
        list.setItemAnimator(null);
        render();

        go.setOnClickListener(v->search());
        query.setOnEditorActionListener((v,action,event)->{
            if(action==EditorInfo.IME_ACTION_SEARCH){search();return true;}
            return false;
        });
        query.requestFocus();
    }

    private void search(){
        String q=query.getText()==null?"":query.getText().toString().trim();
        if(q.isEmpty()){
            queryLayout.setError("Enter a file or folder name");
            return;
        }
        queryLayout.setError(null);
        final int token=++generation;
        progress.setVisibility(View.VISIBLE);
        status.setText("Searching…");
        empty.setVisibility(View.GONE);
        list.setVisibility(View.GONE);

        new Thread(()->{
            List<File> all=FileEngine.walk(base,AppPrefs.showHidden(this),new FileEngine.Progress(){
                @Override public void update(String text,int done,int total){
                    if(token!=generation)return;
                    runOnUiThread(()->status.setText(text));
                }
                @Override public boolean cancelled(){return token!=generation;}
            });
            if(token!=generation)return;

            ArrayList<File> found=new ArrayList<>();
            String needle=q.toLowerCase(Locale.US);
            for(File f:all){
                if(token!=generation)return;
                if(f.getName().toLowerCase(Locale.US).contains(needle))found.add(f);
            }
            found.sort((a,b)->{
                if(a.isDirectory()!=b.isDirectory())return a.isDirectory()?-1:1;
                return a.getName().compareToIgnoreCase(b.getName());
            });

            runOnUiThread(()->{
                if(token!=generation)return;
                results.clear();
                results.addAll(found);
                progress.setVisibility(View.GONE);
                status.setText(found.size()==1?"1 result":found.size()+" results");
                render();
                if(found.isEmpty()){
                    emptyTitle.setText("No matches");
                    emptyBody.setText("Try a shorter or different name.");
                }
            });
        }).start();
    }

    private void render(){
        list.setAdapter(new FileRecyclerAdapter(this,results,selected,false,new FileRecyclerAdapter.Listener(){
            @Override public void onClick(File file){returnTarget(file);}
            @Override public void onLongClick(File file){returnTarget(file);}
            @Override public void onMore(File file){returnTarget(file);}
        }));
        boolean has=!results.isEmpty();
        list.setVisibility(has?View.VISIBLE:View.GONE);
        empty.setVisibility(has?View.GONE:View.VISIBLE);
    }

    private void returnTarget(File file){
        Intent data=new Intent();
        data.putExtra(EXTRA_TARGET,file.getAbsolutePath());
        setResult(RESULT_OK,data);
        finish();
    }

    private String friendlyPath(File dir){
        File home=Environment.getExternalStorageDirectory();
        String basePath=home.getAbsolutePath();
        String full=dir.getAbsolutePath();
        if(full.equals(basePath))return "Internal storage";
        if(full.startsWith(basePath+File.separator)){
            return "Internal storage › "+full.substring(basePath.length()+1).replace(File.separator," › ");
        }
        return full;
    }

    @Override protected void onDestroy(){
        generation++;
        super.onDestroy();
    }
}
