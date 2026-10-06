package com.blazefm.blazesystems;

import android.Manifest;
import android.app.*;
import android.content.*;
import android.content.pm.*;
import android.graphics.Color;
import android.net.Uri;
import android.os.*;
import android.provider.Settings;
import android.view.*;
import android.widget.*;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.bottomnavigation.BottomNavigationView;
import com.google.android.material.chip.Chip;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.floatingactionbutton.ExtendedFloatingActionButton;
import com.google.android.material.progressindicator.LinearProgressIndicator;

import java.io.*;
import java.text.SimpleDateFormat;
import java.util.*;

public class MainActivity extends Activity {
    private View homePanel,emptyState,headerDetails; private RecyclerView recycler; private TextView path,status,folderTitle,storageText; private LinearProgressIndicator storageBar; private MaterialToolbar mainToolbar,selectionToolbar; private Chip sortChip,viewChip,pasteChip; private ExtendedFloatingActionButton fab; private BottomNavigationView bottomNav;
    private File cwd; private final ArrayList<File>shown=new ArrayList<>(); private final LinkedHashSet<String>selected=new LinkedHashSet<>();
    private final ArrayList<File>clipboard=new ArrayList<>(); private boolean clipboardMove,cancel,showingResults; private final int REQ=9;

    @Override public void onCreate(Bundle b){super.onCreate(b);buildUi();ensurePermission();cwd=Environment.getExternalStorageDirectory();showDir(cwd);}
    @Override public void onResume(){super.onResume();if(cwd!=null&&!showingResults)showDir(cwd);}

    private void buildUi(){
        setContentView(R.layout.activity_main);
        mainToolbar=findViewById(R.id.main_toolbar);
        selectionToolbar=findViewById(R.id.selection_toolbar);
        headerDetails=findViewById(R.id.header_details);
        folderTitle=findViewById(R.id.folder_title);
        path=findViewById(R.id.path);
        status=findViewById(R.id.status);
        homePanel=findViewById(R.id.home_panel);
        emptyState=findViewById(R.id.empty_state);
        storageText=findViewById(R.id.storage_text);
        storageBar=findViewById(R.id.storage_bar);
        recycler=findViewById(R.id.file_list);
        sortChip=findViewById(R.id.chip_sort);
        viewChip=findViewById(R.id.chip_view);
        pasteChip=findViewById(R.id.chip_paste);
        fab=findViewById(R.id.fab_new);
        bottomNav=findViewById(R.id.bottom_nav);

        recycler.setHasFixedSize(false);
        recycler.setItemAnimator(null);

        mainToolbar.setOnMenuItemClickListener(item->{
            if(item.getItemId()==R.id.action_search){searchDialog();return true;}
            if(item.getItemId()==R.id.action_tools){tools();return true;}
            return false;
        });

        selectionToolbar.setNavigationOnClickListener(v->clearSelection());
        selectionToolbar.setOnMenuItemClickListener(item->{
            int id=item.getItemId();
            if(id==R.id.action_copy){prepareClipboard(false);return true;}
            if(id==R.id.action_move){prepareClipboard(true);return true;}
            if(id==R.id.action_zip){zipSelection();return true;}
            if(id==R.id.action_trash){trashSelection();return true;}
            if(id==R.id.action_more){
                List<File> one=selectedFiles();
                if(one.size()==1)fileMenu(one.get(0));else toast("Select one item for more actions");
                return true;
            }
            return false;
        });

        findViewById(R.id.chip_up).setOnClickListener(v->up());
        findViewById(R.id.chip_home).setOnClickListener(v->showDir(Environment.getExternalStorageDirectory()));
        sortChip.setOnClickListener(v->sortSheet());
        viewChip.setOnClickListener(v->toggleView());
        pasteChip.setOnClickListener(v->paste());
        fab.setOnClickListener(v->createMenu());
        findViewById(R.id.empty_create).setOnClickListener(v->createMenu());

        findViewById(R.id.category_images).setOnClickListener(v->categorySearch(0,"Images"));
        findViewById(R.id.category_video).setOnClickListener(v->categorySearch(1,"Video"));
        findViewById(R.id.category_audio).setOnClickListener(v->categorySearch(2,"Audio"));
        findViewById(R.id.category_docs).setOnClickListener(v->categorySearch(3,"Documents"));
        findViewById(R.id.category_apks).setOnClickListener(v->categorySearch(4,"APKs"));

        bottomNav.setSelectedItemId(R.id.nav_files);
        bottomNav.setOnItemSelectedListener(item->{
            int id=item.getItemId();
            if(id==R.id.nav_files){showDir(Environment.getExternalStorageDirectory());return true;}
            if(id==R.id.nav_analyze){Intent i=new Intent(this,AnalyzerActivity.class);i.putExtra("path",cwd==null?Environment.getExternalStorageDirectory().getAbsolutePath():cwd.getAbsolutePath());startActivity(i);return true;}
            if(id==R.id.nav_network){startActivity(new Intent(this,RemoteActivity.class));return true;}
            if(id==R.id.nav_cloud){startActivity(new Intent(this,CloudActivity.class));return true;}
            if(id==R.id.nav_apps){startActivity(new Intent(this,AppManagerActivity.class));return true;}
            return false;
        });
    }

    private void updateHomePanel(){
        if(homePanel==null||cwd==null)return;
        File home=Environment.getExternalStorageDirectory();
        boolean show=!showingResults&&cwd.equals(home);
        homePanel.setVisibility(show?View.VISIBLE:View.GONE);
        if(!show)return;
        long total=home.getTotalSpace(),free=home.getFreeSpace(),used=Math.max(0,total-free);
        int pct=total>0?(int)((used*100L)/total):0;
        storageBar.setMax(100);storageBar.setProgress(pct);
        storageText.setText(pct+"% used · "+fmt(free)+" free of "+fmt(total));
    }

    private void categorySearch(int category,String label){
        cancel=false;progress("Scanning "+label.toLowerCase(Locale.US)+"…");
        File base=Environment.getExternalStorageDirectory();
        new Thread(()->{
            List<File> all=FileEngine.walk(base,AppPrefs.showHidden(this),new P());
            ArrayList<File> result=new ArrayList<>();
            for(File f:all)if(FileEngine.category(f)==category)result.add(f);
            runOnUiThread(()->showResults(label,result));
        }).start();
    }

    private void ensurePermission(){if(Build.VERSION.SDK_INT>=30&&!Environment.isExternalStorageManager()){new MaterialAlertDialogBuilder(this).setTitle("File access required").setMessage("BlazeFM is a full file manager. Android 11+ requires All files access for normal filesystem browsing, duplicates, archives and Trash. You can still use Cloud/SAF without it.").setPositiveButton("Open settings",(d,w)->{try{startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,Uri.parse("package:"+getPackageName())));}catch(Exception e){startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));}}).setNegativeButton("Later",null).show();}else if(Build.VERSION.SDK_INT>=23&&Build.VERSION.SDK_INT<30&&checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE)!=PackageManager.PERMISSION_GRANTED)requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE,Manifest.permission.WRITE_EXTERNAL_STORAGE},REQ);}

    private void showDir(File d){if(d==null||!d.isDirectory())return;showingResults=false;cwd=d;path.setText(d.getAbsolutePath());folderTitle.setText(d.equals(Environment.getExternalStorageDirectory())?"Internal storage":d.getName());selected.clear();File[]a=null;try{a=d.listFiles();}catch(Exception ignored){}shown.clear();boolean hidden=AppPrefs.showHidden(this);if(a!=null){ArrayList<File>x=new ArrayList<>();for(File f:a)if((hidden||!f.getName().startsWith("."))&&!f.getName().equals(".BlazeFM_Trash"))x.add(f);sortFiles(x);shown.addAll(x);}updateHomePanel();render();status.setText(shown.size()+" items · "+sortLabel()+((clipboard.size()>0)?" · clipboard "+clipboard.size():""));}
    private void render(){
        boolean gridMode=AppPrefs.gridView(this);
        if(showingResults)homePanel.setVisibility(View.GONE);else updateHomePanel();

        int width=getResources().getConfiguration().screenWidthDp;
        int spans=Math.max(2,Math.min(5,width/120));
        recycler.setLayoutManager(gridMode?new GridLayoutManager(this,spans):new LinearLayoutManager(this));
        recycler.setAdapter(new FileRecyclerAdapter(this,shown,selected,gridMode,new FileRecyclerAdapter.Listener(){
            @Override public void onClick(File f){
                if(!selected.isEmpty()){toggleSelect(f);return;}
                if(f.isDirectory())showDir(f);else openFile(f);
            }
            @Override public void onLongClick(File f){toggleSelect(f);}
            @Override public void onMore(File f){fileMenu(f);}
        }));

        boolean hasItems=!shown.isEmpty();
        recycler.setVisibility(hasItems?View.VISIBLE:View.GONE);
        emptyState.setVisibility(hasItems?View.GONE:View.VISIBLE);

        sortChip.setText(sortLabel());
        viewChip.setText(gridMode?"List":"Grid");
        viewChip.setChipIconResource(gridMode?R.drawable.ic_list:R.drawable.ic_grid);
        pasteChip.setVisibility(clipboard.isEmpty()?View.GONE:View.VISIBLE);
        if(!clipboard.isEmpty())pasteChip.setText("Paste ("+clipboard.size()+")");

        boolean selecting=!selected.isEmpty();
        mainToolbar.setVisibility(selecting?View.GONE:View.VISIBLE);
        selectionToolbar.setVisibility(selecting?View.VISIBLE:View.GONE);
        headerDetails.setVisibility(selecting?View.GONE:View.VISIBLE);
        selectionToolbar.setTitle(selected.size()+" selected");
        fab.setVisibility(selecting?View.GONE:View.VISIBLE);

        folderTitle.setText(selecting?selected.size()+" selected":(showingResults?folderTitle.getText():(cwd!=null&&cwd.equals(Environment.getExternalStorageDirectory())?"Internal storage":cwd==null?"Files":cwd.getName())));
    }
    private void toggleSelect(File f){String k=AppPrefs.canon(f);if(!selected.remove(k))selected.add(k);render();}
    private void clearSelection(){selected.clear();render();}
    private List<File> selectedFiles(){ArrayList<File>a=new ArrayList<>();for(File f:shown)if(selected.contains(AppPrefs.canon(f)))a.add(f);return a;}
    private void up(){if(showingResults){showDir(cwd);return;}File p=cwd==null?null:cwd.getParentFile();if(p!=null)showDir(p);}

    private void openFile(File f){AppPrefs.addRecent(this,f);if(FileEngine.isImage(f)||FileEngine.isText(f)||FileEngine.isVideo(f)||FileEngine.isAudio(f)){Intent i=new Intent(this,PreviewActivity.class);i.putExtra("path",f.getAbsolutePath());startActivity(i);return;}Intent i=new Intent(Intent.ACTION_VIEW);i.setDataAndType(BlazeProvider.uriFor(f),FileEngine.mime(f));i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);try{startActivity(i);}catch(Exception e){toast("No app can open this file");}}

    private void fileMenu(File f){
        ArrayList<ActionSheet.Item> items=new ArrayList<>();
        items.add(ActionSheet.item(
                f.isDirectory()?R.drawable.ic_folder:R.drawable.ic_open_in_new,
                f.isDirectory()?"Open folder":"Preview / open",
                f.isDirectory()?f.getAbsolutePath():FileEngine.mime(f)+" · "+fmt(f.length()),
                ()->{if(f.isDirectory())showDir(f);else openFile(f);}
        ));
        items.add(ActionSheet.item(
                R.drawable.ic_select,
                selected.contains(AppPrefs.canon(f))?"Unselect":"Select",
                "Use contextual multi-select actions",
                ()->toggleSelect(f)
        ));
        items.add(ActionSheet.item(
                R.drawable.ic_star,
                AppPrefs.isFavorite(this,f)?"Remove favorite":"Add favorite",
                "Keep this item in Favorites",
                ()->{AppPrefs.toggleFavorite(this,f);toast(AppPrefs.isFavorite(this,f)?"Added to favorites":"Removed from favorites");}
        ));
        items.add(ActionSheet.item(R.drawable.ic_copy,"Copy","Copy to another folder",()->{selected.clear();selected.add(AppPrefs.canon(f));prepareClipboard(false);}));
        items.add(ActionSheet.item(R.drawable.ic_move,"Move","Move to another folder",()->{selected.clear();selected.add(AppPrefs.canon(f));prepareClipboard(true);}));
        items.add(ActionSheet.item(R.drawable.ic_edit,"Rename","Change the file or folder name",()->rename(f)));
        if(f.isFile())items.add(ActionSheet.item(R.drawable.ic_info,"Properties / SHA-256","Size, modified date, MIME and hash",()->hashDialog(f)));
        if(FileEngine.isArchive(f))items.add(ActionSheet.item(R.drawable.ic_archive,"Extract ZIP here","Safely extract this archive",()->extract(f)));
        if(FileEngine.isApk(f))items.add(ActionSheet.item(R.drawable.ic_apps,"APK info / Install","Inspect package metadata or open installer",()->apkInfo(f)));
        items.add(ActionSheet.item(R.drawable.ic_delete,"Move to Trash","Can be restored later",()->trashOne(f)));
        items.add(ActionSheet.danger(R.drawable.ic_delete,"Delete permanently","Cannot be restored from BlazeFM Trash",()->permanentDelete(f)));

        ActionSheet.show(
                this,
                f.getName(),
                f.isDirectory()?f.getAbsolutePath():fmt(f.length())+" · "+FileEngine.mime(f),
                items
        );
    }

    private void createMenu(){
        ArrayList<ActionSheet.Item> items=new ArrayList<>();
        items.add(ActionSheet.item(
                R.drawable.ic_create_folder,
                "New folder",
                "Create an empty folder in "+(cwd==null?"this location":cwd.getName()),
                ()->inputSheet("New folder","Folder name","", "Create",n->{
                    if(!validName(n))return;
                    try{
                        File f=new File(cwd,n);
                        if(!f.mkdir())toast("Folder already exists or could not be created");
                        showDir(cwd);
                    }catch(Exception e){toast("Create failed: "+e.getMessage());}
                })
        ));
        items.add(ActionSheet.item(
                R.drawable.ic_description,
                "New text file",
                "Create an empty text document",
                ()->inputSheet("New text file","File name.txt","", "Create",n->{
                    if(!validName(n))return;
                    try{
                        File f=new File(cwd,n);
                        if(!f.createNewFile())toast("File already exists or could not be created");
                        showDir(cwd);
                    }catch(Exception e){toast("Create failed: "+e.getMessage());}
                })
        ));
        ActionSheet.show(this,"Create here",cwd==null?"Current folder":cwd.getAbsolutePath(),items);
    }

    private void rename(File f){inputSheet("Rename","New name",f.getName(),"Rename",n->{if(!validName(n))return;File to=new File(f.getParentFile(),n);if(f.renameTo(to))showDir(cwd);else toast("Rename failed or destination already exists");});}

    private void prepareClipboard(boolean move){List<File>a=selectedFiles();if(a.isEmpty())return;clipboard.clear();clipboard.addAll(a);clipboardMove=move;clearSelection();status.setText((move?"Move":"Copy")+" clipboard: "+clipboard.size()+" · open destination and tap Paste");}
    private void paste(){if(clipboard.isEmpty()){toast("Clipboard is empty");return;}File dest=cwd;ArrayList<File>items=new ArrayList<>(clipboard);boolean move=clipboardMove;progress((move?"Moving":"Copying")+"…");new Thread(()->{int ok=0;for(File f:items){if(cancel)break;File out=FileEngine.unique(dest,f.getName());try{if(move)FileEngine.move(f,out);else FileEngine.copy(f,out);ok++;setProgress((move?"Move":"Copy")+": "+f.getName());}catch(Exception e){if(out.exists())FileEngine.delete(out);setProgress("Failed: "+f.getName()+" · "+e.getMessage());}}final int count=ok;runOnUiThread(()->{if(move&&count==items.size()){clipboard.clear();clipboardMove=false;}cancel=false;showDir(dest);status.setText(count+" item(s) completed");});}).start();}
    private void zipSelection(){List<File>a=selectedFiles();if(a.isEmpty())return;String ts=new SimpleDateFormat("yyyyMMdd-HHmmss",Locale.US).format(new Date());File out=FileEngine.unique(cwd,"BlazeFM-"+ts+".zip");progress("Creating ZIP…");new Thread(()->{try{FileEngine.zip(a,out);runOnUiThread(()->{clearSelection();showDir(cwd);status.setText("Created "+out.getName());});}catch(Exception e){FileEngine.delete(out);runOnUiThread(()->toast("ZIP failed: "+e.getMessage()));}}).start();}
    private void extract(File zip){File dest=FileEngine.unique(cwd,stripExt(zip.getName()));dest.mkdirs();progress("Extracting "+zip.getName()+"…");new Thread(()->{try{FileEngine.unzip(zip,dest);runOnUiThread(()->{showDir(cwd);status.setText("Extracted to "+dest.getName());});}catch(Exception e){FileEngine.delete(dest);runOnUiThread(()->toast("Extract failed: "+e.getMessage()));}}).start();}
    private String stripExt(String n){int i=n.lastIndexOf('.');return i>0?n.substring(0,i):n+"_files";}

    private void trashOne(File f){new MaterialAlertDialogBuilder(this).setTitle("Move to Trash?").setMessage(f.getAbsolutePath()).setPositiveButton("Trash",(d,w)->new Thread(()->{try{TrashManager.moveToTrash(this,f);runOnUiThread(()->showDir(cwd));}catch(Exception e){runOnUiThread(()->toast("Trash failed: "+e.getMessage()));}}).start()).setNegativeButton("Cancel",null).show();}
    private void trashSelection(){List<File>a=selectedFiles();if(a.isEmpty())return;new MaterialAlertDialogBuilder(this).setTitle("Move selected to Trash?").setMessage(a.size()+" item(s) can be restored later.").setPositiveButton("Trash",(d,w)->new Thread(()->{for(File f:a)try{TrashManager.moveToTrash(this,f);}catch(Exception ignored){}runOnUiThread(()->{clearSelection();showDir(cwd);});}).start()).setNegativeButton("Cancel",null).show();}
    private void permanentDelete(File f){new MaterialAlertDialogBuilder(this).setTitle("Delete permanently?").setMessage(f.getAbsolutePath()+"\n\nThis cannot be restored from BlazeFM Trash.").setPositiveButton("DELETE",(d,w)->new Thread(()->{boolean ok=FileEngine.delete(f);runOnUiThread(()->{if(!ok)toast("Delete failed");showDir(cwd);});}).start()).setNegativeButton("Cancel",null).show();}

    private void searchDialog(){inputSheet("Search in "+(cwd==null?"Files":cwd.getName()),"Filename contains…","","Search",this::runSearch);}
    private void runSearch(String q){if(q.trim().isEmpty())return;cancel=false;progress("Searching…");File base=cwd;new Thread(()->{List<File>a=FileEngine.walk(base,AppPrefs.showHidden(this),new P());ArrayList<File>r=new ArrayList<>();String needle=q.toLowerCase(Locale.US);for(File f:a)if(f.getName().toLowerCase(Locale.US).contains(needle))r.add(f);runOnUiThread(()->showResults("Search: "+q,r));}).start();}
    private void showResults(String t,List<File>r){showingResults=true;shown.clear();shown.addAll(r);sortFiles(shown);selected.clear();folderTitle.setText(t);path.setText("Search results · tap Up to return");status.setText(r.size()+" results · "+sortLabel());render();}

    private void scanDupes(){cancel=false;File base=cwd;progress("Finding exact duplicates…");new Thread(()->{try{List<FileEngine.DupGroup>g=FileEngine.exactDuplicates(base,AppPrefs.showHidden(this),new P());runOnUiThread(()->duplicateResults(g));}catch(Exception e){err(e);}}).start();}
    private void duplicateResults(List<FileEngine.DupGroup>g){long reclaim=0;StringBuilder b=new StringBuilder();for(FileEngine.DupGroup x:g){reclaim+=x.size*(x.files.size()-1L);b.append('\n').append(x.files.size()).append(" copies · ").append(fmt(x.size)).append(" each\n");for(File f:x.files)b.append(f.getAbsolutePath()).append('\n');}if(g.isEmpty())b.append("No exact duplicates found.");final long save=reclaim;new MaterialAlertDialogBuilder(this).setTitle("Exact duplicates · reclaimable "+fmt(save)).setMessage(b.toString()).setPositiveButton("Close",null).setNeutralButton(g.isEmpty()?"Close":"Trash extra copies",(d,w)->{if(!g.isEmpty())confirmDuplicateCleanup(g);}).show();status.setText("Duplicate scan complete");}
    private void confirmDuplicateCleanup(List<FileEngine.DupGroup>g){new MaterialAlertDialogBuilder(this).setTitle("Trash duplicate extras?").setMessage("BlazeFM will keep the first file in each SHA-256-verified group and move the other copies to Trash. Review the paths first.").setPositiveButton("Trash extras",(d,w)->new Thread(()->{int n=0;for(FileEngine.DupGroup x:g)for(int i=1;i<x.files.size();i++)try{TrashManager.moveToTrash(this,x.files.get(i));n++;}catch(Exception ignored){}final int z=n;runOnUiThread(()->{showDir(cwd);toast(z+" duplicate file(s) moved to Trash");});}).start()).setNegativeButton("Cancel",null).show();}
    private void scanPhotos(){cancel=false;File base=cwd;progress("Analyzing photos…");new Thread(()->{try{FileEngine.SimilarResult r=FileEngine.similarPhotos(base,AppPrefs.showHidden(this),new P());runOnUiThread(()->photoResults(r));}catch(Exception e){err(e);}}).start();}
    private void photoResults(FileEngine.SimilarResult r){List<FileEngine.SimilarPair>p=r.pairs;StringBuilder b=new StringBuilder();int max=Math.min(p.size(),300);for(int i=0;i<max;i++){FileEngine.SimilarPair x=p.get(i);b.append("distance ").append(x.distance).append("\n").append(x.a.getAbsolutePath()).append("\n↔ ").append(x.b.getAbsolutePath()).append("\n\n");}if(r.totalPairs==0)b.append("No visually similar photo pairs found.");if(p.size()>max)b.append("… ").append(p.size()-max).append(" retained pairs not shown\n");if(r.truncated)b.append("\nLow-memory mode retained the best ").append(p.size()).append(" of ").append(r.totalPairs).append(" matching pairs.");new MaterialAlertDialogBuilder(this).setTitle("Similar photos · "+r.totalPairs+" pairs").setMessage(b.toString()).setPositiveButton("Close",null).show();status.setText("Photo scan complete");}

    private void tools(){
        ArrayList<ActionSheet.Item> items=new ArrayList<>();
        items.add(ActionSheet.item(R.drawable.ic_copy,"Exact duplicates","SHA-256 verified duplicate cleanup",this::scanDupes));
        items.add(ActionSheet.item(R.drawable.ic_image,"Similar photos","Perceptual photo matching with low-memory limits",this::scanPhotos));
        items.add(ActionSheet.item(R.drawable.ic_analytics,"Storage analyzer","Category totals, empty folders and largest files",()->{
            Intent a=new Intent(this,AnalyzerActivity.class);
            a.putExtra("path",cwd.getAbsolutePath());
            startActivity(a);
        }));
        items.add(ActionSheet.item(R.drawable.ic_star,"Favorites","Open folders and files you pinned",()->showPaths("Favorites",AppPrefs.favorites(this))));
        items.add(ActionSheet.item(R.drawable.ic_history,"Recent files","Jump back to recently opened files",()->showPaths("Recent files",AppPrefs.recents(this))));
        items.add(ActionSheet.item(R.drawable.ic_delete,"Trash","Restore or permanently purge deleted items",this::trashDialog));
        items.add(ActionSheet.item(R.drawable.ic_visibility_off,"Hidden files",AppPrefs.showHidden(this)?"Currently shown · tap to hide":"Currently hidden · tap to show",()->{
            AppPrefs.setShowHidden(this,!AppPrefs.showHidden(this));
            showDir(cwd);
        }));
        items.add(ActionSheet.item(R.drawable.ic_refresh,"Refresh","Reload the current folder",()->showDir(cwd)));
        items.add(ActionSheet.item(R.drawable.ic_storage,"Storage info","Device capacity and current location",this::storageInfo));
        items.add(ActionSheet.item(R.drawable.ic_apps,"App manager","Launch, inspect, back up or uninstall apps",()->startActivity(new Intent(this,AppManagerActivity.class))));
        items.add(ActionSheet.item(R.drawable.ic_network,"Network","SMB, FTP and SFTP connections",()->startActivity(new Intent(this,RemoteActivity.class))));
        items.add(ActionSheet.item(R.drawable.ic_cloud,"Cloud","Android document and cloud providers",()->startActivity(new Intent(this,CloudActivity.class))));
        items.add(ActionSheet.item(R.drawable.ic_root,"Root browser","Privileged filesystem access when available",()->startActivity(new Intent(this,RootActivity.class))));
        items.add(ActionSheet.item(R.drawable.ic_info,"About BlazeFM","Version, platform support and feature summary",this::about));
        ActionSheet.show(this,"Tools & utilities","Cleanup, locations, connections and system tools",items);
    }

    private void toggleView(){AppPrefs.setGridView(this,!AppPrefs.gridView(this));render();}

    private void sortSheet(){
        String current=AppPrefs.sortMode(this);
        ArrayList<ActionSheet.Item> items=new ArrayList<>();
        items.add(sortItem("name","Name","A to Z",current));
        items.add(sortItem("date","Date","Newest first",current));
        items.add(sortItem("size","Size","Largest first",current));
        items.add(sortItem("type","Type","File type then name",current));
        ActionSheet.show(this,"Sort files","Folders stay first",items);
    }

    private ActionSheet.Item sortItem(String mode,String name,String desc,String current){
        int icon=mode.equals(current)?R.drawable.ic_check:R.drawable.ic_sort;
        String subtitle=mode.equals(current)?desc+" · Selected":desc;
        return ActionSheet.item(icon,name,subtitle,()->{
            AppPrefs.setSortMode(this,mode);
            if(showingResults){
                sortFiles(shown);
                render();
                status.setText(shown.size()+" results · "+sortLabel());
            }else showDir(cwd);
        });
    }

    private String sortLabel(){String m=AppPrefs.sortMode(this);return "date".equals(m)?"Date ↓":"size".equals(m)?"Size ↓":"type".equals(m)?"Type":"Name A–Z";}

    private void sortFiles(List<File> files){
        final String mode=AppPrefs.sortMode(this);Collections.sort(files,(a,b)->{
            if(a.isDirectory()!=b.isDirectory())return a.isDirectory()?-1:1;
            int c=0;
            if("date".equals(mode))c=Long.compare(b.lastModified(),a.lastModified());
            else if("size".equals(mode))c=Long.compare(b.length(),a.length());
            else if("type".equals(mode)){c=fileType(a).compareToIgnoreCase(fileType(b));}
            if(c!=0)return c;return a.getName().compareToIgnoreCase(b.getName());
        });
    }

    private String fileType(File f){if(f.isDirectory())return "folder";String n=f.getName();int i=n.lastIndexOf('.');return i>=0&&i<n.length()-1?n.substring(i+1):"";}

    private interface TextAction{void run(String value);}

    private void inputSheet(String titleText,String hint,String initial,String actionLabel,TextAction action){
        MaterialPrompts.text(this,titleText,hint,initial,actionLabel,action::run);
    }

    private boolean validName(String n){if(n.isEmpty()||n.equals(".")||n.equals("..")||n.contains("/")||n.contains("\\")||n.indexOf('\0')>=0){toast("Invalid file name");return false;}return true;}

    private void showPaths(String t,Collection<String>paths){ArrayList<String>a=new ArrayList<>();for(String p:paths)if(new File(p).exists())a.add(p);if(a.isEmpty()){toast("No saved items");return;}new MaterialAlertDialogBuilder(this).setTitle(t).setItems(a.toArray(new String[0]),(d,w)->{File f=new File(a.get(w));if(f.isDirectory())showDir(f);else openFile(f);}).setPositiveButton("Close",null).show();}
    private void trashDialog(){List<TrashManager.Item>a=TrashManager.list(this);if(a.isEmpty()){toast("Trash is empty");return;}String[]rows=new String[a.size()];for(int i=0;i<rows.length;i++)rows[i]=a.get(i).stored.getName()+"\nfrom: "+a.get(i).original;new MaterialAlertDialogBuilder(this).setTitle("Trash · "+a.size()+" items").setItems(rows,(d,w)->trashItem(a.get(w))).setNeutralButton("Empty Trash",(d,w)->new MaterialAlertDialogBuilder(this).setTitle("Empty Trash permanently?").setPositiveButton("Empty",(x,y)->new Thread(()->{TrashManager.empty(this);runOnUiThread(()->toast("Trash emptied"));}).start()).setNegativeButton("Cancel",null).show()).setPositiveButton("Close",null).show();}
    private void trashItem(TrashManager.Item i){String[]o={"Restore","Delete permanently"};new MaterialAlertDialogBuilder(this).setTitle(i.stored.getName()).setMessage("Original: "+i.original).setItems(o,(d,w)->{if(w==0)new Thread(()->{try{File r=TrashManager.restore(this,i);runOnUiThread(()->toast("Restored: "+r.getAbsolutePath()));}catch(Exception e){runOnUiThread(()->toast("Restore failed: "+e.getMessage()));}}).start();else new Thread(()->{TrashManager.purge(this,i);runOnUiThread(()->toast("Deleted permanently"));}).start();}).show();}

    private void hashDialog(File f){progress("Hashing…");new Thread(()->{try{String h=FileEngine.sha256(f);String msg="Path: "+f.getAbsolutePath()+"\nSize: "+fmt(f.length())+"\nModified: "+new Date(f.lastModified())+"\nMIME: "+FileEngine.mime(f)+"\n\nSHA-256\n"+h;runOnUiThread(()->new MaterialAlertDialogBuilder(this).setTitle(f.getName()).setMessage(msg).setPositiveButton("OK",null).show());}catch(Exception e){err(e);}}).start();}
    private void apkInfo(File f){PackageManager pm=getPackageManager();PackageInfo pi=pm.getPackageArchiveInfo(f.getAbsolutePath(),PackageManager.GET_ACTIVITIES);String msg;if(pi==null)msg="Unable to parse APK.";else{ApplicationInfo ai=pi.applicationInfo;if(ai!=null){ai.sourceDir=f.getAbsolutePath();ai.publicSourceDir=f.getAbsolutePath();}String label=ai==null?f.getName():String.valueOf(pm.getApplicationLabel(ai));msg=label+"\nPackage: "+pi.packageName+"\nVersion: "+pi.versionName+" ("+(Build.VERSION.SDK_INT>=28?pi.getLongVersionCode():pi.versionCode)+")\nSize: "+fmt(f.length());}new MaterialAlertDialogBuilder(this).setTitle("APK").setMessage(msg).setPositiveButton("Install",(d,w)->installApk(f)).setNegativeButton("Close",null).show();}
    private void installApk(File f){if(Build.VERSION.SDK_INT>=26&&!getPackageManager().canRequestPackageInstalls()){try{startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,Uri.parse("package:"+getPackageName())));toast("Allow BlazeFM to install unknown apps, then retry the APK.");}catch(Exception e){toast("Enable 'Install unknown apps' for BlazeFM in Android settings.");}return;}Intent i=new Intent(Intent.ACTION_VIEW);i.setDataAndType(BlazeProvider.uriFor(f),"application/vnd.android.package-archive");i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_ACTIVITY_NEW_TASK);try{startActivity(i);}catch(Exception e){toast("Installer unavailable: "+e.getMessage());}}

    private void storageInfo(){File e=Environment.getExternalStorageDirectory();long total=e.getTotalSpace(),free=e.getFreeSpace();new MaterialAlertDialogBuilder(this).setTitle("Storage").setMessage("Total: "+fmt(total)+"\nUsed: "+fmt(total-free)+"\nFree: "+fmt(free)+"\n\nCurrent folder:\n"+cwd.getAbsolutePath()).setPositiveButton("OK",null).show();}
    private void about(){String m="BlazeFM 1.3.0\ncom.blazefm.blazesystems\nAndroid 5.0+ (API 21)\n\nLocal file manager, batch copy/move, ZIP, Trash, hidden files, favorites/recent, exact SHA-256 duplicates, similar photos, analyzer, APK manager/backup, media/text preview, SAF cloud providers, SMB/FTP/SFTP, and optional root browser.\n\nNo ads or analytics.";new MaterialAlertDialogBuilder(this).setTitle("About BlazeFM").setMessage(m).setPositiveButton("OK",null).show();}

    private void progress(String s){cancel=false;status.setText(s+" · tap status to cancel");status.setOnClickListener(v->{cancel=true;status.setText("Cancelling…");});}
    private void setProgress(String s){runOnUiThread(()->status.setText(s));}
    private class P implements FileEngine.Progress{public void update(String s,int d,int t){setProgress(s);}public boolean cancelled(){return cancel;}}
    private void err(Exception e){runOnUiThread(()->{status.setText("Error");new MaterialAlertDialogBuilder(this).setTitle("Operation failed").setMessage(e.getClass().getSimpleName()+": "+e.getMessage()).setPositiveButton("OK",null).show();});}
    private String fmt(long n){String[]u={"B","KB","MB","GB","TB"};double v=n;int i=0;while(v>=1024&&i<u.length-1){v/=1024;i++;}return String.format(Locale.US,v>=10?"%.0f %s":"%.1f %s",v,u[i]);}
    private void toast(String s){Toast.makeText(this,s,Toast.LENGTH_LONG).show();}
    @Override public void onBackPressed(){if(!selected.isEmpty()){clearSelection();return;}if(showingResults){showDir(cwd);return;}File home=Environment.getExternalStorageDirectory();if(cwd!=null&&!cwd.equals(home)&&cwd.getParentFile()!=null)up();else super.onBackPressed();}
}
