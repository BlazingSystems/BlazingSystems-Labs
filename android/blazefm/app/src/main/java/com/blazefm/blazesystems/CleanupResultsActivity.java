package com.blazefm.blazesystems;

import android.app.Activity;
import android.content.Intent;
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
import java.util.Locale;

public class CleanupResultsActivity extends Activity {
    static final String EXTRA_MODE="mode";
    static final String EXTRA_REPORT="report";
    static final String MODE_DUPLICATES="duplicates";
    static final String MODE_SIMILAR="similar";

    private String mode;
    private File reportFile;
    private RecyclerView list;
    private View empty;
    private TextView summaryTitle,summaryBody,status;
    private MaterialButton primary;
    private SimpleRowAdapter adapter;

    private CleanupReportStore.DuplicateReport duplicateReport;
    private CleanupReportStore.SimilarReport similarReport;

    @Override public void onCreate(Bundle state){
        super.onCreate(state);
        setContentView(R.layout.activity_cleanup_results);

        MaterialToolbar toolbar=findViewById(R.id.cleanup_toolbar);
        toolbar.setNavigationOnClickListener(v->finish());

        summaryTitle=findViewById(R.id.cleanup_summary_title);
        summaryBody=findViewById(R.id.cleanup_summary_body);
        status=findViewById(R.id.cleanup_status);
        list=findViewById(R.id.cleanup_list);
        empty=findViewById(R.id.cleanup_empty);
        primary=findViewById(R.id.cleanup_primary);

        list.setLayoutManager(new LinearLayoutManager(this));
        list.setItemAnimator(null);
        adapter=new SimpleRowAdapter(new SimpleRowAdapter.Listener(){
            @Override public void onClick(int position){openRow(position);}
            @Override public void onMore(int position){openRow(position);}
            @Override public boolean onLongClick(int position){openRow(position);return true;}
        });
        list.setAdapter(adapter);

        mode=getIntent().getStringExtra(EXTRA_MODE);
        String raw=getIntent().getStringExtra(EXTRA_REPORT);
        if(raw==null){finish();return;}
        reportFile=new File(raw);

        try{
            String cache=getCacheDir().getCanonicalPath()+File.separator;
            String report=reportFile.getCanonicalPath();
            if(!report.startsWith(cache))throw new IllegalArgumentException("Invalid report location");
        }catch(Exception e){
            toast("Cleanup report is unavailable");
            finish();
            return;
        }

        load();
    }

    private void load(){
        try{
            if(MODE_DUPLICATES.equals(mode))loadDuplicates();
            else if(MODE_SIMILAR.equals(mode))loadSimilar();
            else throw new IllegalArgumentException("Unknown cleanup mode");
        }catch(Exception e){
            status.setText("Unable to load cleanup results");
            toast("Cleanup results unavailable: "+e.getMessage());
        }
    }

    private void loadDuplicates()throws Exception{
        duplicateReport=CleanupReportStore.loadDuplicates(reportFile);
        List<SimpleRowAdapter.Row> rows=new ArrayList<>();
        for(FileEngine.DupGroup group:duplicateReport.groups){
            long reclaim=group.size*Math.max(0,group.files.size()-1L);
            rows.add(new SimpleRowAdapter.Row(
                    R.drawable.ic_copy,
                    group.files.size()+" identical copies",
                    fmt(group.size)+" each · "+fmt(reclaim)+" reclaimable"
            ));
        }
        adapter.submit(rows);

        summaryTitle.setText("Exact duplicates");
        summaryBody.setText(fmt(duplicateReport.reclaim)+" reclaimable across "+duplicateReport.groups.size()+" verified group(s)");
        status.setText(duplicateReport.groups.isEmpty()?"No exact duplicates found":"Tap a group to inspect its copies");
        primary.setVisibility(duplicateReport.groups.isEmpty()?View.GONE:View.VISIBLE);
        primary.setText("Trash duplicate extras");
        primary.setOnClickListener(v->confirmDuplicateCleanup());

        showEmpty(duplicateReport.groups.isEmpty(),"No exact duplicates","No SHA-256-verified duplicate groups were found.");
    }

    private void loadSimilar()throws Exception{
        similarReport=CleanupReportStore.loadSimilar(reportFile);
        List<SimpleRowAdapter.Row> rows=new ArrayList<>();
        for(FileEngine.SimilarPair pair:similarReport.pairs){
            rows.add(new SimpleRowAdapter.Row(
                    R.drawable.ic_image,
                    pair.a.getName()+" ↔ "+pair.b.getName(),
                    similarityLabel(pair.distance)+" · distance "+pair.distance
            ));
        }
        adapter.submit(rows);

        summaryTitle.setText("Similar photos");
        String body=similarReport.totalPairs+" matching pair(s)";
        if(similarReport.truncated)body+=" · best "+similarReport.pairs.size()+" retained for low-memory viewing";
        summaryBody.setText(body);
        status.setText(similarReport.pairs.isEmpty()?"No visually similar photos found":"Tap a pair to open either image");
        primary.setVisibility(View.GONE);

        showEmpty(similarReport.pairs.isEmpty(),"No similar photos","No visually similar photo pairs were found in this scan.");
    }

    private void showEmpty(boolean isEmpty,String title,String body){
        list.setVisibility(isEmpty?View.GONE:View.VISIBLE);
        empty.setVisibility(isEmpty?View.VISIBLE:View.GONE);
        if(isEmpty){
            ((TextView)findViewById(R.id.cleanup_empty_title)).setText(title);
            ((TextView)findViewById(R.id.cleanup_empty_body)).setText(body);
        }
    }

    private void openRow(int position){
        if(MODE_DUPLICATES.equals(mode)){
            if(duplicateReport==null||position<0||position>=duplicateReport.groups.size())return;
            openDuplicateGroup(duplicateReport.groups.get(position));
        }else if(MODE_SIMILAR.equals(mode)){
            if(similarReport==null||position<0||position>=similarReport.pairs.size())return;
            openSimilarPair(similarReport.pairs.get(position));
        }
    }

    private void openDuplicateGroup(FileEngine.DupGroup group){
        ArrayList<ActionSheet.Item> actions=new ArrayList<>();
        for(int i=0;i<group.files.size();i++){
            File file=group.files.get(i);
            final File target=file;
            String label=(i==0?"Keep candidate · ":"Copy "+(i+1)+" · ")+file.getName();
            actions.add(ActionSheet.item(
                    iconFor(file),
                    label,
                    friendlyPath(file),
                    ()->open(target)
            ));
        }
        ActionSheet.show(
                this,
                group.files.size()+" identical copies",
                fmt(group.size)+" each · SHA-256 verified",
                actions
        );
    }

    private void openSimilarPair(FileEngine.SimilarPair pair){
        ArrayList<ActionSheet.Item> actions=new ArrayList<>();
        actions.add(ActionSheet.item(
                R.drawable.ic_image,
                "Open "+pair.a.getName(),
                friendlyPath(pair.a),
                ()->open(pair.a)
        ));
        actions.add(ActionSheet.item(
                R.drawable.ic_image,
                "Open "+pair.b.getName(),
                friendlyPath(pair.b),
                ()->open(pair.b)
        ));
        ActionSheet.show(
                this,
                similarityLabel(pair.distance),
                "Perceptual distance "+pair.distance,
                actions
        );
    }

    private void confirmDuplicateCleanup(){
        if(duplicateReport==null||duplicateReport.groups.isEmpty())return;
        new MaterialAlertDialogBuilder(this)
                .setTitle("Trash duplicate extras?")
                .setMessage("BlazeFM will keep one existing SHA-256-verified copy in each group and move the other existing copies to Trash.")
                .setNegativeButton("Cancel",null)
                .setPositiveButton("Trash extras",(d,w)->cleanupDuplicateExtras())
                .show();
    }

    private void cleanupDuplicateExtras(){
        primary.setEnabled(false);
        status.setText("Moving duplicate extras to Trash…");

        new Thread(()->{
            int moved=0;
            for(FileEngine.DupGroup group:duplicateReport.groups){
                File keep=null;
                for(File file:group.files){
                    if(file.exists()){keep=file;break;}
                }
                if(keep==null)continue;

                for(File file:group.files){
                    if(file.equals(keep)||!file.exists())continue;
                    try{
                        TrashManager.moveToTrash(this,file);
                        moved++;
                    }catch(Exception ignored){}
                }
            }

            final int count=moved;
            runOnUiThread(()->{
                toast(count+" duplicate file(s) moved to Trash");
                primary.setVisibility(View.GONE);
                status.setText("Cleanup complete · rescan to refresh results");
            });
        }).start();
    }

    private void open(File file){
        if(!file.exists()){toast("File no longer exists");return;}
        if(FileEngine.isImage(file)||FileEngine.isText(file)||FileEngine.isVideo(file)||FileEngine.isAudio(file)){
            Intent i=new Intent(this,PreviewActivity.class);
            i.putExtra("path",file.getAbsolutePath());
            startActivity(i);
            return;
        }

        Intent i=new Intent(Intent.ACTION_VIEW);
        i.setDataAndType(BlazeProvider.uriFor(file),FileEngine.mime(file));
        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try{startActivity(i);}
        catch(Exception e){toast("No app can open this file");}
    }

    private String similarityLabel(int distance){
        if(distance<=1)return "Nearly identical";
        if(distance<=3)return "Very similar";
        return "Similar";
    }

    private int iconFor(File file){
        if(FileEngine.isImage(file))return R.drawable.ic_image;
        if(FileEngine.isVideo(file))return R.drawable.ic_video;
        if(FileEngine.isAudio(file))return R.drawable.ic_music;
        if(FileEngine.isApk(file))return R.drawable.ic_apps;
        if(FileEngine.isArchive(file))return R.drawable.ic_archive;
        if(FileEngine.isText(file))return R.drawable.ic_description;
        return R.drawable.ic_file;
    }

    private String friendlyPath(File file){
        File home=android.os.Environment.getExternalStorageDirectory();
        String base=home.getAbsolutePath();
        String full=file.getAbsolutePath();
        if(full.equals(base))return "Internal storage";
        if(full.startsWith(base+File.separator)){
            return "Internal storage › "+full.substring(base.length()+1).replace(File.separator," › ");
        }
        return full;
    }

    private String fmt(long n){
        String[]u={"B","KB","MB","GB","TB"};
        double v=n;
        int i=0;
        while(v>=1024&&i<u.length-1){v/=1024;i++;}
        return String.format(Locale.US,v>=10?"%.0f %s":"%.1f %s",v,u[i]);
    }

    private void toast(String text){Toast.makeText(this,text,Toast.LENGTH_LONG).show();}
}
