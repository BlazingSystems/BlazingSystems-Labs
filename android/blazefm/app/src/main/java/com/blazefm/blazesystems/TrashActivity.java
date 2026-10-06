package com.blazefm.blazesystems;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.button.MaterialButton;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class TrashActivity extends Activity {
    private final ArrayList<TrashManager.Item> items=new ArrayList<>();
    private SimpleRowAdapter adapter;
    private RecyclerView list;
    private View empty;
    private TextView status;
    private MaterialButton emptyAll;

    @Override public void onCreate(Bundle state){
        super.onCreate(state);
        setContentView(R.layout.activity_trash);

        MaterialToolbar toolbar=findViewById(R.id.trash_toolbar);
        toolbar.setNavigationOnClickListener(v->finish());

        status=findViewById(R.id.trash_status);
        list=findViewById(R.id.trash_list);
        empty=findViewById(R.id.trash_empty);
        emptyAll=findViewById(R.id.trash_empty_all);

        list.setLayoutManager(new LinearLayoutManager(this));
        list.setItemAnimator(null);
        adapter=new SimpleRowAdapter(new SimpleRowAdapter.Listener(){
            @Override public void onClick(int position){if(valid(position))menu(items.get(position));}
            @Override public void onMore(int position){if(valid(position))menu(items.get(position));}
            @Override public boolean onLongClick(int position){if(valid(position))menu(items.get(position));return true;}
        });
        list.setAdapter(adapter);

        emptyAll.setOnClickListener(v->confirmEmpty());
        refresh();
    }

    @Override protected void onResume(){
        super.onResume();
        refresh();
    }

    private boolean valid(int position){return position>=0&&position<items.size();}

    private void refresh(){
        items.clear();
        items.addAll(TrashManager.list(this));

        List<SimpleRowAdapter.Row> rows=new ArrayList<>();
        for(TrashManager.Item item:items){
            File original=new File(item.original);
            String title=original.getName().isEmpty()?item.stored.getName():original.getName();
            rows.add(new SimpleRowAdapter.Row(
                    item.stored.isDirectory()?R.drawable.ic_folder:R.drawable.ic_file,
                    title,
                    "From "+friendlyParent(original)
            ));
        }
        adapter.submit(rows);

        boolean has=!items.isEmpty();
        list.setVisibility(has?View.VISIBLE:View.GONE);
        empty.setVisibility(has?View.GONE:View.VISIBLE);
        emptyAll.setVisibility(has?View.VISIBLE:View.GONE);
        status.setText(has?(items.size()==1?"1 item":items.size()+" items"):"Nothing to restore");
    }

    private void menu(TrashManager.Item item){
        File original=new File(item.original);
        ArrayList<ActionSheet.Item> actions=new ArrayList<>();
        actions.add(ActionSheet.item(
                R.drawable.ic_refresh,
                "Restore",
                "Return to "+friendlyParent(original),
                ()->restore(item)
        ));
        actions.add(ActionSheet.danger(
                R.drawable.ic_delete,
                "Delete permanently",
                "This cannot be undone",
                ()->confirmDelete(item)
        ));
        ActionSheet.show(this,original.getName().isEmpty()?item.stored.getName():original.getName(),item.original,actions);
    }

    private void restore(TrashManager.Item item){
        new Thread(()->{
            try{
                File restored=TrashManager.restore(this,item);
                runOnUiThread(()->{
                    toast("Restored to "+restored.getParent());
                    refresh();
                });
            }catch(Exception e){
                runOnUiThread(()->toast("Restore failed: "+e.getMessage()));
            }
        }).start();
    }

    private void confirmDelete(TrashManager.Item item){
        new MaterialAlertDialogBuilder(this)
                .setTitle("Delete permanently?")
                .setMessage(new File(item.original).getName()+" will be removed from BlazeFM Trash and cannot be restored.")
                .setNegativeButton("Cancel",null)
                .setPositiveButton("Delete",(d,w)->new Thread(()->{
                    TrashManager.purge(this,item);
                    runOnUiThread(()->{
                        toast("Deleted permanently");
                        refresh();
                    });
                }).start())
                .show();
    }

    private void confirmEmpty(){
        new MaterialAlertDialogBuilder(this)
                .setTitle("Empty Trash permanently?")
                .setMessage("All "+items.size()+" item(s) in BlazeFM Trash will be deleted.")
                .setNegativeButton("Cancel",null)
                .setPositiveButton("Empty Trash",(d,w)->new Thread(()->{
                    TrashManager.empty(this);
                    runOnUiThread(()->{
                        toast("Trash emptied");
                        refresh();
                    });
                }).start())
                .show();
    }

    private String friendlyParent(File file){
        File p=file.getParentFile();
        if(p==null)return "original location";
        File home=android.os.Environment.getExternalStorageDirectory();
        String base=home.getAbsolutePath();
        String full=p.getAbsolutePath();
        if(full.equals(base))return "Internal storage";
        if(full.startsWith(base+File.separator)){
            return "Internal storage › "+full.substring(base.length()+1).replace(File.separator," › ");
        }
        return full;
    }

    private void toast(String text){Toast.makeText(this,text,Toast.LENGTH_LONG).show();}
}
