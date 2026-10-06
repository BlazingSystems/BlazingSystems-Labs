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
import java.io.*;
import java.text.SimpleDateFormat;
import java.util.*;

public class MainActivity extends Activity {
    private LinearLayout root,selectionBar,homePanel,emptyState; private FrameLayout contentFrame; private ListView list; private GridView grid; private TextView path,status,title,folderTitle,storageText; private ProgressBar storageBar; private Button sortButton,viewButton;
    private File cwd; private final ArrayList<File>shown=new ArrayList<>(); private final LinkedHashSet<String>selected=new LinkedHashSet<>();
    private final ArrayList<File>clipboard=new ArrayList<>(); private boolean clipboardMove,cancel,showingResults; private final int REQ=9;

    @Override public void onCreate(Bundle b){super.onCreate(b);buildUi();ensurePermission();cwd=Environment.getExternalStorageDirectory();showDir(cwd);}
    @Override public void onResume(){super.onResume();if(cwd!=null&&!showingResults)showDir(cwd);}

    private void buildUi(){
        root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setBackgroundColor(Ui.BG);

        LinearLayout top=new LinearLayout(this);top.setOrientation(LinearLayout.VERTICAL);top.setPadding(Ui.dp(this,16),Ui.dp(this,12),Ui.dp(this,16),Ui.dp(this,10));top.setBackgroundColor(Ui.HEADER);top.setElevation(Ui.dp(this,4));
        LinearLayout brandRow=new LinearLayout(this);brandRow.setOrientation(LinearLayout.HORIZONTAL);brandRow.setGravity(Gravity.CENTER_VERTICAL);
        title=Ui.text(this,"BlazeFM",22);title.setTextColor(Color.WHITE);title.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);title.setPadding(0,0,0,0);brandRow.addView(title,new LinearLayout.LayoutParams(0,-2,1));
        Button search=Ui.iconButton(this,"⌕");search.setContentDescription("Search");search.setOnClickListener(v->searchDialog());brandRow.addView(search,new LinearLayout.LayoutParams(Ui.dp(this,46),Ui.dp(this,42)));
        Button more=Ui.iconButton(this,"⋮");more.setContentDescription("More tools");more.setOnClickListener(v->tools());brandRow.addView(more,new LinearLayout.LayoutParams(Ui.dp(this,46),Ui.dp(this,42)));
        top.addView(brandRow);

        folderTitle=Ui.text(this,"Internal storage",18);folderTitle.setTextColor(Color.WHITE);folderTitle.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);folderTitle.setPadding(0,Ui.dp(this,8),0,0);top.addView(folderTitle);
        path=Ui.text(this,"",11);path.setTextColor(Ui.HEADER_MUTED);path.setSingleLine(true);path.setEllipsize(android.text.TextUtils.TruncateAt.START);path.setTextIsSelectable(true);path.setPadding(0,Ui.dp(this,2),0,Ui.dp(this,8));top.addView(path);

        HorizontalScrollView hs=new HorizontalScrollView(this);hs.setHorizontalScrollBarEnabled(false);LinearLayout quick=new LinearLayout(this);quick.setOrientation(LinearLayout.HORIZONTAL);quick.setPadding(0,Ui.dp(this,4),0,Ui.dp(this,2));
        Button up=Ui.chipButton(this,"↑  Up");up.setOnClickListener(v->up());quick.addView(up,new LinearLayout.LayoutParams(Ui.dp(this,80),Ui.dp(this,40)));
        Button home=Ui.chipButton(this,"⌂  Home");home.setOnClickListener(v->showDir(Environment.getExternalStorageDirectory()));quick.addView(home,new LinearLayout.LayoutParams(Ui.dp(this,92),Ui.dp(this,40)));
        sortButton=Ui.chipButton(this,"Sort");sortButton.setOnClickListener(v->sortSheet());quick.addView(sortButton,new LinearLayout.LayoutParams(Ui.dp(this,82),Ui.dp(this,40)));
        viewButton=Ui.chipButton(this,AppPrefs.gridView(this)?"≡  List":"▦  Grid");viewButton.setOnClickListener(v->toggleView());quick.addView(viewButton,new LinearLayout.LayoutParams(Ui.dp(this,86),Ui.dp(this,40)));
        Button paste=Ui.chipButton(this,"Paste");paste.setOnClickListener(v->paste());quick.addView(paste,new LinearLayout.LayoutParams(Ui.dp(this,82),Ui.dp(this,40)));
        Button add=Ui.primaryButton(this,"＋  New");add.setOnClickListener(v->createMenu());quick.addView(add,new LinearLayout.LayoutParams(Ui.dp(this,96),Ui.dp(this,40)));
        hs.addView(quick);top.addView(hs);root.addView(top);

        status=Ui.text(this,"Ready",11);status.setTextColor(Ui.MUTED);status.setPadding(Ui.dp(this,16),Ui.dp(this,8),Ui.dp(this,16),Ui.dp(this,4));root.addView(status);

        buildHomePanel();

        contentFrame=new FrameLayout(this);contentFrame.setBackgroundColor(Ui.BG);
        list=new ListView(this);list.setDivider(null);list.setDividerHeight(0);list.setClipToPadding(false);list.setPadding(Ui.dp(this,8),Ui.dp(this,4),Ui.dp(this,8),Ui.dp(this,10));list.setBackgroundColor(Ui.BG);contentFrame.addView(list,new FrameLayout.LayoutParams(-1,-1));
        grid=new GridView(this);grid.setNumColumns(GridView.AUTO_FIT);grid.setColumnWidth(Ui.dp(this,112));grid.setHorizontalSpacing(Ui.dp(this,5));grid.setVerticalSpacing(Ui.dp(this,5));grid.setStretchMode(GridView.STRETCH_COLUMN_WIDTH);grid.setClipToPadding(false);grid.setPadding(Ui.dp(this,8),Ui.dp(this,6),Ui.dp(this,8),Ui.dp(this,10));grid.setBackgroundColor(Ui.BG);contentFrame.addView(grid,new FrameLayout.LayoutParams(-1,-1));
        emptyState=new LinearLayout(this);emptyState.setOrientation(LinearLayout.VERTICAL);emptyState.setGravity(Gravity.CENTER);emptyState.setPadding(Ui.dp(this,32),Ui.dp(this,24),Ui.dp(this,32),Ui.dp(this,24));
        TextView emptyIcon=Ui.text(this,"□",42);emptyIcon.setGravity(Gravity.CENTER);emptyIcon.setTextColor(Ui.ORANGE);emptyIcon.setPadding(0,0,0,Ui.dp(this,8));emptyState.addView(emptyIcon);
        TextView emptyTitle=Ui.text(this,"This folder is empty",16);emptyTitle.setGravity(Gravity.CENTER);emptyTitle.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);emptyTitle.setPadding(0,0,0,Ui.dp(this,4));emptyState.addView(emptyTitle);
        TextView emptyText=Ui.text(this,"Create a folder or file here, or paste something from the clipboard.",11);emptyText.setGravity(Gravity.CENTER);emptyText.setTextColor(Ui.MUTED);emptyText.setPadding(0,0,0,Ui.dp(this,14));emptyState.addView(emptyText);
        Button emptyNew=Ui.primaryButton(this,"＋  Create here");emptyNew.setOnClickListener(v->createMenu());emptyState.addView(emptyNew,new LinearLayout.LayoutParams(Ui.dp(this,150),Ui.dp(this,42)));
        contentFrame.addView(emptyState,new FrameLayout.LayoutParams(-1,-1));root.addView(contentFrame,new LinearLayout.LayoutParams(-1,0,1));

        selectionBar=Ui.toolbar(this);selectionBar.setVisibility(View.GONE);String[]ss={"Copy","Move","Zip","Trash","Clear"};for(String s:ss){Button b=Ui.button(this,s);Ui.equalAdd(selectionBar,b);if(s.equals("Copy"))b.setOnClickListener(v->prepareClipboard(false));if(s.equals("Move"))b.setOnClickListener(v->prepareClipboard(true));if(s.equals("Zip"))b.setOnClickListener(v->zipSelection());if(s.equals("Trash"))b.setOnClickListener(v->trashSelection());if(s.equals("Clear"))b.setOnClickListener(v->clearSelection());}root.addView(selectionBar);

        LinearLayout nav=new LinearLayout(this);nav.setOrientation(LinearLayout.HORIZONTAL);nav.setPadding(Ui.dp(this,6),Ui.dp(this,5),Ui.dp(this,6),Ui.dp(this,7));nav.setBackgroundColor(Ui.SURFACE);nav.setElevation(Ui.dp(this,8));
        String[]ns={"Files","Analyze","Network","Cloud","Apps"};for(String s:ns){Button n=Ui.navButton(this,s,s.equals("Files"));Ui.equalAdd(nav,n);if(s.equals("Files"))n.setOnClickListener(v->showDir(Environment.getExternalStorageDirectory()));else if(s.equals("Analyze"))n.setOnClickListener(v->{Intent i=new Intent(this,AnalyzerActivity.class);i.putExtra("path",cwd.getAbsolutePath());startActivity(i);});else if(s.equals("Network"))n.setOnClickListener(v->startActivity(new Intent(this,RemoteActivity.class)));else if(s.equals("Cloud"))n.setOnClickListener(v->startActivity(new Intent(this,CloudActivity.class)));else n.setOnClickListener(v->startActivity(new Intent(this,AppManagerActivity.class)));}root.addView(nav);

        setContentView(root);
        AdapterView.OnItemClickListener click=(p,v,pos,id)->{File f=shown.get(pos);if(!selected.isEmpty()){toggleSelect(f);return;}if(f.isDirectory())showDir(f);else openFile(f);};
        AdapterView.OnItemLongClickListener hold=(p,v,pos,id)->{File f=shown.get(pos);if(!selected.isEmpty())toggleSelect(f);else fileMenu(f);return true;};
        list.setOnItemClickListener(click);list.setOnItemLongClickListener(hold);grid.setOnItemClickListener(click);grid.setOnItemLongClickListener(hold);
    }

    private void buildHomePanel(){
        homePanel=new LinearLayout(this);homePanel.setOrientation(LinearLayout.VERTICAL);homePanel.setPadding(Ui.dp(this,14),Ui.dp(this,12),Ui.dp(this,14),Ui.dp(this,12));
        homePanel.setBackground(Ui.rounded(this,Ui.SURFACE,16,Ui.BORDER,1));homePanel.setElevation(Ui.dp(this,1));
        LinearLayout wrap=new LinearLayout(this);wrap.setPadding(Ui.dp(this,12),Ui.dp(this,5),Ui.dp(this,12),Ui.dp(this,5));wrap.addView(homePanel,new LinearLayout.LayoutParams(-1,-2));root.addView(wrap);

        TextView t=Ui.text(this,"Device storage",14);t.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);t.setPadding(0,0,0,0);homePanel.addView(t);
        storageText=Ui.text(this,"",11);storageText.setTextColor(Ui.MUTED);storageText.setPadding(0,Ui.dp(this,3),0,Ui.dp(this,8));homePanel.addView(storageText);
        storageBar=new ProgressBar(this,null,android.R.attr.progressBarStyleHorizontal);storageBar.setMax(1000);if(Build.VERSION.SDK_INT>=21)storageBar.setProgressTintList(android.content.res.ColorStateList.valueOf(Ui.ORANGE));homePanel.addView(storageBar,new LinearLayout.LayoutParams(-1,Ui.dp(this,6)));

        TextView quickLabel=Ui.text(this,"Quick locations",11);quickLabel.setTextColor(Ui.MUTED);quickLabel.setPadding(0,Ui.dp(this,11),0,Ui.dp(this,5));homePanel.addView(quickLabel);
        HorizontalScrollView scroll=new HorizontalScrollView(this);scroll.setHorizontalScrollBarEnabled(false);LinearLayout shortcuts=new LinearLayout(this);shortcuts.setOrientation(LinearLayout.HORIZONTAL);
        addShortcut(shortcuts,"Downloads",Environment.DIRECTORY_DOWNLOADS);addShortcut(shortcuts,"DCIM",Environment.DIRECTORY_DCIM);addShortcut(shortcuts,"Pictures",Environment.DIRECTORY_PICTURES);addShortcut(shortcuts,"Movies",Environment.DIRECTORY_MOVIES);addShortcut(shortcuts,"Music",Environment.DIRECTORY_MUSIC);
        scroll.addView(shortcuts);homePanel.addView(scroll);
    }

    private void addShortcut(LinearLayout bar,String label,String dirName){
        Button b=Ui.button(this,label);LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(Ui.dp(this,94),Ui.dp(this,40));lp.setMargins(0,0,Ui.dp(this,7),0);bar.addView(b,lp);
        b.setOnClickListener(v->{File d=Environment.getExternalStoragePublicDirectory(dirName);if(d!=null&&d.isDirectory())showDir(d);else toast(label+" folder is unavailable");});
    }

    private void updateHomePanel(){
        if(homePanel==null||cwd==null)return;File home=Environment.getExternalStorageDirectory();boolean show=cwd.equals(home);homePanel.setVisibility(show?View.VISIBLE:View.GONE);if(!show)return;
        long total=home.getTotalSpace(),free=home.getFreeSpace(),used=Math.max(0,total-free);int progress=total>0?(int)Math.min(1000,(used*1000L)/total):0;storageBar.setProgress(progress);
        int pct=total>0?(int)((used*100L)/total):0;storageText.setText(pct+"% used  ·  "+fmt(free)+" free of "+fmt(total));
    }

    private void ensurePermission(){if(Build.VERSION.SDK_INT>=30&&!Environment.isExternalStorageManager()){new AlertDialog.Builder(this).setTitle("File access required").setMessage("BlazeFM is a full file manager. Android 11+ requires All files access for normal filesystem browsing, duplicates, archives and Trash. You can still use Cloud/SAF without it.").setPositiveButton("Open settings",(d,w)->{try{startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,Uri.parse("package:"+getPackageName())));}catch(Exception e){startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));}}).setNegativeButton("Later",null).show();}else if(Build.VERSION.SDK_INT>=23&&Build.VERSION.SDK_INT<30&&checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE)!=PackageManager.PERMISSION_GRANTED)requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE,Manifest.permission.WRITE_EXTERNAL_STORAGE},REQ);}

    private void showDir(File d){if(d==null||!d.isDirectory())return;showingResults=false;cwd=d;path.setText(d.getAbsolutePath());folderTitle.setText(d.equals(Environment.getExternalStorageDirectory())?"Internal storage":d.getName());selected.clear();File[]a=null;try{a=d.listFiles();}catch(Exception ignored){}shown.clear();boolean hidden=AppPrefs.showHidden(this);if(a!=null){ArrayList<File>x=new ArrayList<>();for(File f:a)if((hidden||!f.getName().startsWith("."))&&!f.getName().equals(".BlazeFM_Trash"))x.add(f);sortFiles(x);shown.addAll(x);}updateHomePanel();render();status.setText(shown.size()+" items · "+sortLabel()+((clipboard.size()>0)?" · clipboard "+clipboard.size():""));}
    private void render(){
        boolean gridMode=AppPrefs.gridView(this);FileListAdapter adapter=new FileListAdapter(this,shown,selected,gridMode);
        if(shown.isEmpty()){list.setVisibility(View.GONE);grid.setVisibility(View.GONE);emptyState.setVisibility(View.VISIBLE);}
        else{emptyState.setVisibility(View.GONE);list.setVisibility(gridMode?View.GONE:View.VISIBLE);grid.setVisibility(gridMode?View.VISIBLE:View.GONE);if(gridMode)grid.setAdapter(adapter);else list.setAdapter(adapter);}
        if(viewButton!=null)viewButton.setText(gridMode?"≡  List":"▦  Grid");
        selectionBar.setVisibility(selected.isEmpty()?View.GONE:View.VISIBLE);folderTitle.setText(selected.isEmpty()?(cwd!=null&&cwd.equals(Environment.getExternalStorageDirectory())?"Internal storage":cwd==null?"Files":cwd.getName()):selected.size()+" selected");
    }
    private String icon(File f){if(FileEngine.isImage(f))return "🖼 ";if(FileEngine.isVideo(f))return "🎬 ";if(FileEngine.isAudio(f))return "🎵 ";if(FileEngine.isApk(f))return "🤖 ";if(FileEngine.isArchive(f))return "🗜 ";return "📄 ";}
    private void toggleSelect(File f){String k=AppPrefs.canon(f);if(!selected.remove(k))selected.add(k);render();}
    private void clearSelection(){selected.clear();render();}
    private List<File> selectedFiles(){ArrayList<File>a=new ArrayList<>();for(File f:shown)if(selected.contains(AppPrefs.canon(f)))a.add(f);return a;}
    private void up(){if(showingResults){showDir(cwd);return;}File p=cwd==null?null:cwd.getParentFile();if(p!=null)showDir(p);}

    private void openFile(File f){AppPrefs.addRecent(this,f);if(FileEngine.isImage(f)||FileEngine.isText(f)||FileEngine.isVideo(f)||FileEngine.isAudio(f)){Intent i=new Intent(this,PreviewActivity.class);i.putExtra("path",f.getAbsolutePath());startActivity(i);return;}Intent i=new Intent(Intent.ACTION_VIEW);i.setDataAndType(BlazeProvider.uriFor(f),FileEngine.mime(f));i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);try{startActivity(i);}catch(Exception e){toast("No app can open this file");}}

    private void fileMenu(File f){
        final Dialog d=new Dialog(this,android.R.style.Theme_Material_Light_NoActionBar_Fullscreen);
        FrameLayout overlay=new FrameLayout(this);overlay.setBackgroundColor(0x99000000);overlay.setOnClickListener(v->d.dismiss());
        LinearLayout sheet=new LinearLayout(this);sheet.setOrientation(LinearLayout.VERTICAL);sheet.setPadding(Ui.dp(this,14),Ui.dp(this,14),Ui.dp(this,14),Ui.dp(this,12));sheet.setBackground(Ui.rounded(this,Ui.SURFACE,22,Ui.BORDER,1));sheet.setElevation(Ui.dp(this,8));sheet.setOnClickListener(v->{});

        LinearLayout head=new LinearLayout(this);head.setOrientation(LinearLayout.HORIZONTAL);head.setGravity(Gravity.CENTER_VERTICAL);
        TextView badge=Ui.text(this,f.isDirectory()?"▰":FileEngine.isImage(f)?"◩":FileEngine.isVideo(f)?"▶":FileEngine.isAudio(f)?"♪":FileEngine.isApk(f)?"A":FileEngine.isArchive(f)?"Z":"•",18);badge.setGravity(Gravity.CENTER);badge.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);badge.setTextColor(Ui.ORANGE);badge.setPadding(0,0,0,0);badge.setBackground(Ui.rounded(this,Ui.ACCENT_SOFT,14,Color.TRANSPARENT,0));head.addView(badge,new LinearLayout.LayoutParams(Ui.dp(this,46),Ui.dp(this,46)));
        LinearLayout labels=new LinearLayout(this);labels.setOrientation(LinearLayout.VERTICAL);labels.setPadding(Ui.dp(this,12),0,Ui.dp(this,8),0);
        TextView name=Ui.text(this,f.getName(),16);name.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);name.setSingleLine(true);name.setEllipsize(android.text.TextUtils.TruncateAt.MIDDLE);name.setPadding(0,0,0,0);labels.addView(name);
        TextView meta=Ui.text(this,f.isDirectory()?f.getAbsolutePath():fmt(f.length())+"  ·  "+FileEngine.mime(f),11);meta.setTextColor(Ui.MUTED);meta.setSingleLine(true);meta.setEllipsize(android.text.TextUtils.TruncateAt.START);meta.setPadding(0,Ui.dp(this,3),0,0);labels.addView(meta);
        head.addView(labels,new LinearLayout.LayoutParams(0,-2,1));
        Button close=Ui.button(this,"×");close.setTextSize(20);close.setOnClickListener(v->d.dismiss());head.addView(close,new LinearLayout.LayoutParams(Ui.dp(this,42),Ui.dp(this,42)));sheet.addView(head);

        TextView hint=Ui.text(this,"FILE ACTIONS",10);hint.setTextColor(Ui.MUTED);hint.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);hint.setPadding(Ui.dp(this,2),Ui.dp(this,13),Ui.dp(this,2),Ui.dp(this,6));sheet.addView(hint);

        ScrollView scroll=new ScrollView(this);LinearLayout actions=new LinearLayout(this);actions.setOrientation(LinearLayout.VERTICAL);
        addFileAction(actions,d,f.isDirectory()?"›":"↗",f.isDirectory()?"Open folder":"Preview / open",false,v->{if(f.isDirectory())showDir(f);else openFile(f);});
        addFileAction(actions,d,"✓",selected.contains(AppPrefs.canon(f))?"Unselect":"Select",false,v->toggleSelect(f));
        addFileAction(actions,d,"★",AppPrefs.isFavorite(this,f)?"Remove favorite":"Add favorite",false,v->{AppPrefs.toggleFavorite(this,f);toast(AppPrefs.isFavorite(this,f)?"Added to favorites":"Removed from favorites");});
        addFileAction(actions,d,"C","Copy",false,v->{selected.clear();selected.add(AppPrefs.canon(f));prepareClipboard(false);});
        addFileAction(actions,d,"M","Move",false,v->{selected.clear();selected.add(AppPrefs.canon(f));prepareClipboard(true);});
        addFileAction(actions,d,"R","Rename",false,v->rename(f));
        if(f.isFile())addFileAction(actions,d,"i","Properties / SHA-256",false,v->hashDialog(f));
        if(FileEngine.isArchive(f))addFileAction(actions,d,"Z","Extract ZIP here",false,v->extract(f));
        if(FileEngine.isApk(f))addFileAction(actions,d,"A","APK info / Install",false,v->apkInfo(f));
        addFileAction(actions,d,"T","Move to Trash",false,v->trashOne(f));
        addFileAction(actions,d,"!","Delete permanently",true,v->permanentDelete(f));
        scroll.addView(actions);sheet.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));

        int h=(int)(getResources().getDisplayMetrics().heightPixels*0.80f);
        FrameLayout.LayoutParams lp=new FrameLayout.LayoutParams(-1,h,Gravity.BOTTOM);lp.setMargins(Ui.dp(this,8),0,Ui.dp(this,8),Ui.dp(this,8));overlay.addView(sheet,lp);d.setContentView(overlay);d.show();
    }

    private void addFileAction(LinearLayout box,Dialog dialog,String icon,String label,boolean danger,View.OnClickListener action){
        LinearLayout row=new LinearLayout(this);row.setOrientation(LinearLayout.HORIZONTAL);row.setGravity(Gravity.CENTER_VERTICAL);row.setPadding(Ui.dp(this,10),Ui.dp(this,8),Ui.dp(this,10),Ui.dp(this,8));row.setBackground(Ui.rounded(this,danger?0xFFFFF0EE:Ui.SURFACE,14,danger?0xFFFFC9C2:Ui.BORDER,1));
        TextView b=Ui.text(this,icon,14);b.setGravity(Gravity.CENTER);b.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);b.setTextColor(danger?Ui.DANGER:Ui.ORANGE);b.setPadding(0,0,0,0);b.setBackground(Ui.rounded(this,danger?0xFFFFE0DC:Ui.ACCENT_SOFT,12,Color.TRANSPARENT,0));row.addView(b,new LinearLayout.LayoutParams(Ui.dp(this,38),Ui.dp(this,38)));
        TextView t=Ui.text(this,label,13);t.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);t.setTextColor(danger?Ui.DANGER:Ui.TEXT);t.setPadding(Ui.dp(this,12),0,0,0);row.addView(t,new LinearLayout.LayoutParams(0,Ui.dp(this,42),1));
        row.setOnClickListener(v->{dialog.dismiss();action.onClick(v);});LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);lp.setMargins(0,0,0,Ui.dp(this,6));box.addView(row,lp);
    }

    private void createMenu(){
        final Dialog d=bottomSheet("Create here","Choose what to add in "+(cwd==null?"this folder":cwd.getName()));LinearLayout body=(LinearLayout)d.findViewById(1002);
        addSheetAction(body,d,"▰","New folder","Create an empty folder",false,v->inputSheet("New folder","Folder name","", "Create",n->{if(!validName(n))return;try{File f=new File(cwd,n);if(!f.mkdir())toast("Folder already exists or could not be created");showDir(cwd);}catch(Exception e){toast("Create failed: "+e.getMessage());}}));
        addSheetAction(body,d,"T","New text file","Create an empty text document",false,v->inputSheet("New text file","File name.txt","", "Create",n->{if(!validName(n))return;try{File f=new File(cwd,n);if(!f.createNewFile())toast("File already exists or could not be created");showDir(cwd);}catch(Exception e){toast("Create failed: "+e.getMessage());}}));
        d.show();
    }

    private void rename(File f){inputSheet("Rename","New name",f.getName(),"Rename",n->{if(!validName(n))return;File to=new File(f.getParentFile(),n);if(f.renameTo(to))showDir(cwd);else toast("Rename failed or destination already exists");});}

    private void prepareClipboard(boolean move){List<File>a=selectedFiles();if(a.isEmpty())return;clipboard.clear();clipboard.addAll(a);clipboardMove=move;clearSelection();status.setText((move?"Move":"Copy")+" clipboard: "+clipboard.size()+" · open destination and tap Paste");}
    private void paste(){if(clipboard.isEmpty()){toast("Clipboard is empty");return;}File dest=cwd;ArrayList<File>items=new ArrayList<>(clipboard);boolean move=clipboardMove;progress((move?"Moving":"Copying")+"…");new Thread(()->{int ok=0;for(File f:items){if(cancel)break;File out=FileEngine.unique(dest,f.getName());try{if(move)FileEngine.move(f,out);else FileEngine.copy(f,out);ok++;setProgress((move?"Move":"Copy")+": "+f.getName());}catch(Exception e){if(out.exists())FileEngine.delete(out);setProgress("Failed: "+f.getName()+" · "+e.getMessage());}}final int count=ok;runOnUiThread(()->{if(move&&count==items.size()){clipboard.clear();clipboardMove=false;}cancel=false;showDir(dest);status.setText(count+" item(s) completed");});}).start();}
    private void zipSelection(){List<File>a=selectedFiles();if(a.isEmpty())return;String ts=new SimpleDateFormat("yyyyMMdd-HHmmss",Locale.US).format(new Date());File out=FileEngine.unique(cwd,"BlazeFM-"+ts+".zip");progress("Creating ZIP…");new Thread(()->{try{FileEngine.zip(a,out);runOnUiThread(()->{clearSelection();showDir(cwd);status.setText("Created "+out.getName());});}catch(Exception e){FileEngine.delete(out);runOnUiThread(()->toast("ZIP failed: "+e.getMessage()));}}).start();}
    private void extract(File zip){File dest=FileEngine.unique(cwd,stripExt(zip.getName()));dest.mkdirs();progress("Extracting "+zip.getName()+"…");new Thread(()->{try{FileEngine.unzip(zip,dest);runOnUiThread(()->{showDir(cwd);status.setText("Extracted to "+dest.getName());});}catch(Exception e){FileEngine.delete(dest);runOnUiThread(()->toast("Extract failed: "+e.getMessage()));}}).start();}
    private String stripExt(String n){int i=n.lastIndexOf('.');return i>0?n.substring(0,i):n+"_files";}

    private void trashOne(File f){new AlertDialog.Builder(this).setTitle("Move to Trash?").setMessage(f.getAbsolutePath()).setPositiveButton("Trash",(d,w)->new Thread(()->{try{TrashManager.moveToTrash(this,f);runOnUiThread(()->showDir(cwd));}catch(Exception e){runOnUiThread(()->toast("Trash failed: "+e.getMessage()));}}).start()).setNegativeButton("Cancel",null).show();}
    private void trashSelection(){List<File>a=selectedFiles();if(a.isEmpty())return;new AlertDialog.Builder(this).setTitle("Move selected to Trash?").setMessage(a.size()+" item(s) can be restored later.").setPositiveButton("Trash",(d,w)->new Thread(()->{for(File f:a)try{TrashManager.moveToTrash(this,f);}catch(Exception ignored){}runOnUiThread(()->{clearSelection();showDir(cwd);});}).start()).setNegativeButton("Cancel",null).show();}
    private void permanentDelete(File f){new AlertDialog.Builder(this).setTitle("Delete permanently?").setMessage(f.getAbsolutePath()+"\n\nThis cannot be restored from BlazeFM Trash.").setPositiveButton("DELETE",(d,w)->new Thread(()->{boolean ok=FileEngine.delete(f);runOnUiThread(()->{if(!ok)toast("Delete failed");showDir(cwd);});}).start()).setNegativeButton("Cancel",null).show();}

    private void searchDialog(){inputSheet("Search in "+(cwd==null?"Files":cwd.getName()),"Filename contains…","","Search",this::runSearch);}
    private void runSearch(String q){if(q.trim().isEmpty())return;cancel=false;progress("Searching…");File base=cwd;new Thread(()->{List<File>a=FileEngine.walk(base,AppPrefs.showHidden(this),new P());ArrayList<File>r=new ArrayList<>();String needle=q.toLowerCase(Locale.US);for(File f:a)if(f.getName().toLowerCase(Locale.US).contains(needle))r.add(f);runOnUiThread(()->showResults("Search: "+q,r));}).start();}
    private void showResults(String t,List<File>r){showingResults=true;shown.clear();shown.addAll(r);sortFiles(shown);selected.clear();folderTitle.setText(t);path.setText("Search results · tap Up to return");status.setText(r.size()+" results · "+sortLabel());render();}

    private void scanDupes(){cancel=false;File base=cwd;progress("Finding exact duplicates…");new Thread(()->{try{List<FileEngine.DupGroup>g=FileEngine.exactDuplicates(base,AppPrefs.showHidden(this),new P());runOnUiThread(()->duplicateResults(g));}catch(Exception e){err(e);}}).start();}
    private void duplicateResults(List<FileEngine.DupGroup>g){long reclaim=0;StringBuilder b=new StringBuilder();for(FileEngine.DupGroup x:g){reclaim+=x.size*(x.files.size()-1L);b.append('\n').append(x.files.size()).append(" copies · ").append(fmt(x.size)).append(" each\n");for(File f:x.files)b.append(f.getAbsolutePath()).append('\n');}if(g.isEmpty())b.append("No exact duplicates found.");final long save=reclaim;new AlertDialog.Builder(this).setTitle("Exact duplicates · reclaimable "+fmt(save)).setMessage(b.toString()).setPositiveButton("Close",null).setNeutralButton(g.isEmpty()?"Close":"Trash extra copies",(d,w)->{if(!g.isEmpty())confirmDuplicateCleanup(g);}).show();status.setText("Duplicate scan complete");}
    private void confirmDuplicateCleanup(List<FileEngine.DupGroup>g){new AlertDialog.Builder(this).setTitle("Trash duplicate extras?").setMessage("BlazeFM will keep the first file in each SHA-256-verified group and move the other copies to Trash. Review the paths first.").setPositiveButton("Trash extras",(d,w)->new Thread(()->{int n=0;for(FileEngine.DupGroup x:g)for(int i=1;i<x.files.size();i++)try{TrashManager.moveToTrash(this,x.files.get(i));n++;}catch(Exception ignored){}final int z=n;runOnUiThread(()->{showDir(cwd);toast(z+" duplicate file(s) moved to Trash");});}).start()).setNegativeButton("Cancel",null).show();}
    private void scanPhotos(){cancel=false;File base=cwd;progress("Analyzing photos…");new Thread(()->{try{FileEngine.SimilarResult r=FileEngine.similarPhotos(base,AppPrefs.showHidden(this),new P());runOnUiThread(()->photoResults(r));}catch(Exception e){err(e);}}).start();}
    private void photoResults(FileEngine.SimilarResult r){List<FileEngine.SimilarPair>p=r.pairs;StringBuilder b=new StringBuilder();int max=Math.min(p.size(),300);for(int i=0;i<max;i++){FileEngine.SimilarPair x=p.get(i);b.append("distance ").append(x.distance).append("\n").append(x.a.getAbsolutePath()).append("\n↔ ").append(x.b.getAbsolutePath()).append("\n\n");}if(r.totalPairs==0)b.append("No visually similar photo pairs found.");if(p.size()>max)b.append("… ").append(p.size()-max).append(" retained pairs not shown\n");if(r.truncated)b.append("\nLow-memory mode retained the best ").append(p.size()).append(" of ").append(r.totalPairs).append(" matching pairs.");new AlertDialog.Builder(this).setTitle("Similar photos · "+r.totalPairs+" pairs").setMessage(b.toString()).setPositiveButton("Close",null).show();status.setText("Photo scan complete");}

    private void tools(){
        final Dialog d=new Dialog(this,android.R.style.Theme_Material_Light_NoActionBar_Fullscreen);
        LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setBackgroundColor(Ui.BG);

        LinearLayout top=new LinearLayout(this);top.setOrientation(LinearLayout.HORIZONTAL);top.setGravity(Gravity.CENTER_VERTICAL);top.setPadding(Ui.dp(this,16),Ui.dp(this,12),Ui.dp(this,10),Ui.dp(this,12));top.setBackgroundColor(Ui.HEADER);
        LinearLayout names=new LinearLayout(this);names.setOrientation(LinearLayout.VERTICAL);
        TextView title=Ui.text(this,"Tools & utilities",20);title.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);title.setTextColor(Color.WHITE);title.setPadding(0,0,0,0);names.addView(title);
        TextView sub=Ui.text(this,"Cleanup, locations, connections and system tools",11);sub.setTextColor(Ui.HEADER_MUTED);sub.setPadding(0,Ui.dp(this,3),0,0);names.addView(sub);
        top.addView(names,new LinearLayout.LayoutParams(0,-2,1));
        Button close=Ui.iconButton(this,"×");close.setContentDescription("Close tools");close.setOnClickListener(v->d.dismiss());top.addView(close,new LinearLayout.LayoutParams(Ui.dp(this,48),Ui.dp(this,44)));
        root.addView(top);

        ScrollView scroll=new ScrollView(this);LinearLayout body=new LinearLayout(this);body.setOrientation(LinearLayout.VERTICAL);body.setPadding(Ui.dp(this,12),Ui.dp(this,10),Ui.dp(this,12),Ui.dp(this,18));

        toolGroup(body,"CLEAN UP");
        addTool(body,d,"D","Exact duplicates","SHA-256 verified duplicate cleanup",v->scanDupes());
        addTool(body,d,"P","Similar photos","Perceptual photo matching with low-memory limits",v->scanPhotos());
        addTool(body,d,"A","Storage analyzer","Category totals, empty folders and largest files",v->{Intent a=new Intent(this,AnalyzerActivity.class);a.putExtra("path",cwd.getAbsolutePath());startActivity(a);});

        toolGroup(body,"PLACES");
        addTool(body,d,"★","Favorites","Open folders and files you pinned",v->showPaths("Favorites",AppPrefs.favorites(this)));
        addTool(body,d,"↺","Recent files","Jump back to recently opened files",v->showPaths("Recent files",AppPrefs.recents(this)));
        addTool(body,d,"T","Trash","Restore or permanently purge deleted items",v->trashDialog());

        toolGroup(body,"BROWSER");
        String hidden=AppPrefs.showHidden(this)?"Hide hidden files":"Show hidden files";
        addTool(body,d,".","Hidden files",hidden,v->{AppPrefs.setShowHidden(this,!AppPrefs.showHidden(this));showDir(cwd);});
        addTool(body,d,"↻","Refresh","Reload the current folder",v->showDir(cwd));
        addTool(body,d,"S","Storage info","Device capacity and current location",v->storageInfo());

        toolGroup(body,"CONNECTIONS & SYSTEM");
        addTool(body,d,"A","App manager","Launch, inspect, back up or uninstall apps",v->startActivity(new Intent(this,AppManagerActivity.class)));
        addTool(body,d,"N","Network","SMB, FTP and SFTP connections",v->startActivity(new Intent(this,RemoteActivity.class)));
        addTool(body,d,"☁","Cloud","Android document and cloud providers",v->startActivity(new Intent(this,CloudActivity.class)));
        addTool(body,d,"#","Root browser","Open privileged filesystem access when available",v->startActivity(new Intent(this,RootActivity.class)));
        addTool(body,d,"i","About BlazeFM","Version, platform support and feature summary",v->about());

        scroll.addView(body);root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));d.setContentView(root);d.show();
    }

    private void toggleView(){AppPrefs.setGridView(this,!AppPrefs.gridView(this));render();}

    private void sortSheet(){
        final Dialog d=bottomSheet("Sort files","Folders stay first; choose how items are ordered");LinearLayout body=(LinearLayout)d.findViewById(1002);
        String current=AppPrefs.sortMode(this);
        addSortChoice(body,d,"A","Name","A to Z","name",current);
        addSortChoice(body,d,"↺","Date","Newest first","date",current);
        addSortChoice(body,d,"S","Size","Largest first","size",current);
        addSortChoice(body,d,"T","Type","File type then name","type",current);
        d.show();
    }

    private void addSortChoice(LinearLayout body,Dialog d,String icon,String name,String desc,String mode,String current){
        addSheetAction(body,d,icon,name+(mode.equals(current)?"  ✓":""),desc,false,v->{AppPrefs.setSortMode(this,mode);if(showingResults){sortFiles(shown);render();status.setText(shown.size()+" results · "+sortLabel());}else showDir(cwd);});
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
        final Dialog d=new Dialog(this,android.R.style.Theme_Material_Light_NoActionBar_Fullscreen);FrameLayout overlay=new FrameLayout(this);overlay.setBackgroundColor(0x99000000);overlay.setOnClickListener(v->d.dismiss());
        LinearLayout sheet=new LinearLayout(this);sheet.setOrientation(LinearLayout.VERTICAL);sheet.setPadding(Ui.dp(this,16),Ui.dp(this,16),Ui.dp(this,16),Ui.dp(this,14));sheet.setBackground(Ui.rounded(this,Ui.SURFACE,22,Ui.BORDER,1));sheet.setOnClickListener(v->{});
        TextView title=Ui.text(this,titleText,18);title.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);title.setPadding(0,0,0,Ui.dp(this,10));sheet.addView(title);
        EditText e=new EditText(this);e.setSingleLine(true);e.setHint(hint);e.setText(initial);e.setTextColor(Ui.TEXT);e.setHintTextColor(Ui.MUTED);e.setSelectAllOnFocus(true);e.setBackground(Ui.rounded(this,Ui.SURFACE_2,14,Ui.BORDER,1));e.setPadding(Ui.dp(this,14),0,Ui.dp(this,14),0);sheet.addView(e,new LinearLayout.LayoutParams(-1,Ui.dp(this,52)));
        LinearLayout actions=Ui.toolbar(this);Button cancel=Ui.button(this,"Cancel"),ok=Ui.primaryButton(this,actionLabel);cancel.setOnClickListener(v->d.dismiss());ok.setOnClickListener(v->{String value=e.getText().toString().trim();if(value.isEmpty()){e.setError("Required");return;}d.dismiss();action.run(value);});Ui.equalAdd(actions,cancel);Ui.equalAdd(actions,ok);sheet.addView(actions);
        FrameLayout.LayoutParams lp=new FrameLayout.LayoutParams(-1,-2,Gravity.BOTTOM);lp.setMargins(Ui.dp(this,8),0,Ui.dp(this,8),Ui.dp(this,8));overlay.addView(sheet,lp);d.setContentView(overlay);d.setOnShowListener(x->{e.requestFocus();d.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);});d.show();
    }

    private boolean validName(String n){if(n.isEmpty()||n.equals(".")||n.equals("..")||n.contains("/")||n.contains("\\")||n.indexOf('\0')>=0){toast("Invalid file name");return false;}return true;}

    private Dialog bottomSheet(String titleText,String subtitle){
        final Dialog d=new Dialog(this,android.R.style.Theme_Material_Light_NoActionBar_Fullscreen);FrameLayout overlay=new FrameLayout(this);overlay.setBackgroundColor(0x99000000);overlay.setOnClickListener(v->d.dismiss());
        LinearLayout sheet=new LinearLayout(this);sheet.setOrientation(LinearLayout.VERTICAL);sheet.setPadding(Ui.dp(this,14),Ui.dp(this,14),Ui.dp(this,14),Ui.dp(this,12));sheet.setBackground(Ui.rounded(this,Ui.SURFACE,22,Ui.BORDER,1));sheet.setOnClickListener(v->{});
        LinearLayout head=new LinearLayout(this);head.setOrientation(LinearLayout.HORIZONTAL);head.setGravity(Gravity.CENTER_VERTICAL);LinearLayout labels=new LinearLayout(this);labels.setOrientation(LinearLayout.VERTICAL);
        TextView title=Ui.text(this,titleText,18);title.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);title.setPadding(0,0,0,0);labels.addView(title);TextView sub=Ui.text(this,subtitle,11);sub.setTextColor(Ui.MUTED);sub.setPadding(0,Ui.dp(this,3),0,0);labels.addView(sub);head.addView(labels,new LinearLayout.LayoutParams(0,-2,1));
        Button close=Ui.button(this,"×");close.setTextSize(20);close.setOnClickListener(v->d.dismiss());head.addView(close,new LinearLayout.LayoutParams(Ui.dp(this,42),Ui.dp(this,42)));sheet.addView(head);
        LinearLayout body=new LinearLayout(this);body.setId(1002);body.setOrientation(LinearLayout.VERTICAL);body.setPadding(0,Ui.dp(this,12),0,0);sheet.addView(body);
        FrameLayout.LayoutParams lp=new FrameLayout.LayoutParams(-1,-2,Gravity.BOTTOM);lp.setMargins(Ui.dp(this,8),0,Ui.dp(this,8),Ui.dp(this,8));overlay.addView(sheet,lp);d.setContentView(overlay);return d;
    }

    private void addSheetAction(LinearLayout box,Dialog dialog,String icon,String label,String desc,boolean danger,View.OnClickListener action){
        LinearLayout row=new LinearLayout(this);row.setOrientation(LinearLayout.HORIZONTAL);row.setGravity(Gravity.CENTER_VERTICAL);row.setPadding(Ui.dp(this,10),Ui.dp(this,8),Ui.dp(this,10),Ui.dp(this,8));row.setBackground(Ui.rounded(this,danger?0xFFFFF0EE:Ui.SURFACE,14,danger?0xFFFFC9C2:Ui.BORDER,1));
        TextView b=Ui.text(this,icon,14);b.setGravity(Gravity.CENTER);b.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);b.setTextColor(danger?Ui.DANGER:Ui.ORANGE);b.setPadding(0,0,0,0);b.setBackground(Ui.rounded(this,danger?0xFFFFE0DC:Ui.ACCENT_SOFT,12,Color.TRANSPARENT,0));row.addView(b,new LinearLayout.LayoutParams(Ui.dp(this,40),Ui.dp(this,40)));
        LinearLayout text=new LinearLayout(this);text.setOrientation(LinearLayout.VERTICAL);text.setPadding(Ui.dp(this,12),0,0,0);TextView n=Ui.text(this,label,13);n.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);n.setTextColor(danger?Ui.DANGER:Ui.TEXT);n.setPadding(0,0,0,0);text.addView(n);TextView s=Ui.text(this,desc,11);s.setTextColor(Ui.MUTED);s.setPadding(0,Ui.dp(this,2),0,0);text.addView(s);row.addView(text,new LinearLayout.LayoutParams(0,-2,1));
        row.setOnClickListener(v->{dialog.dismiss();action.onClick(v);});LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);lp.setMargins(0,0,0,Ui.dp(this,7));box.addView(row,lp);
    }

    private void toolGroup(LinearLayout box,String label){
        TextView t=Ui.text(this,label,10);t.setTextColor(Ui.MUTED);t.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);t.setPadding(Ui.dp(this,4),Ui.dp(this,13),Ui.dp(this,4),Ui.dp(this,6));box.addView(t);
    }

    private void addTool(LinearLayout box,Dialog dialog,String badge,String name,String desc,View.OnClickListener action){
        LinearLayout row=new LinearLayout(this);row.setOrientation(LinearLayout.HORIZONTAL);row.setGravity(Gravity.CENTER_VERTICAL);row.setPadding(Ui.dp(this,12),Ui.dp(this,10),Ui.dp(this,10),Ui.dp(this,10));row.setBackground(Ui.rounded(this,Ui.SURFACE,16,Ui.BORDER,1));row.setElevation(Ui.dp(this,1));row.setClickable(true);
        TextView icon=Ui.text(this,badge,16);icon.setGravity(Gravity.CENTER);icon.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);icon.setTextColor(Ui.ORANGE);icon.setPadding(0,0,0,0);icon.setBackground(Ui.rounded(this,Ui.ACCENT_SOFT,14,Color.TRANSPARENT,0));row.addView(icon,new LinearLayout.LayoutParams(Ui.dp(this,44),Ui.dp(this,44)));
        LinearLayout text=new LinearLayout(this);text.setOrientation(LinearLayout.VERTICAL);text.setPadding(Ui.dp(this,12),0,Ui.dp(this,8),0);
        TextView n=Ui.text(this,name,14);n.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);n.setPadding(0,0,0,0);text.addView(n);
        TextView s=Ui.text(this,desc,11);s.setTextColor(Ui.MUTED);s.setPadding(0,Ui.dp(this,3),0,0);text.addView(s);
        row.addView(text,new LinearLayout.LayoutParams(0,-2,1));
        TextView arrow=Ui.text(this,"›",22);arrow.setTextColor(Ui.MUTED);arrow.setGravity(Gravity.CENTER);arrow.setPadding(0,0,0,0);row.addView(arrow,new LinearLayout.LayoutParams(Ui.dp(this,28),Ui.dp(this,44)));
        row.setOnClickListener(v->{dialog.dismiss();action.onClick(v);});
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);lp.setMargins(0,0,0,Ui.dp(this,7));box.addView(row,lp);
    }
    private void showPaths(String t,Collection<String>paths){ArrayList<String>a=new ArrayList<>();for(String p:paths)if(new File(p).exists())a.add(p);if(a.isEmpty()){toast("No saved items");return;}new AlertDialog.Builder(this).setTitle(t).setItems(a.toArray(new String[0]),(d,w)->{File f=new File(a.get(w));if(f.isDirectory())showDir(f);else openFile(f);}).setPositiveButton("Close",null).show();}
    private void trashDialog(){List<TrashManager.Item>a=TrashManager.list(this);if(a.isEmpty()){toast("Trash is empty");return;}String[]rows=new String[a.size()];for(int i=0;i<rows.length;i++)rows[i]=a.get(i).stored.getName()+"\nfrom: "+a.get(i).original;new AlertDialog.Builder(this).setTitle("Trash · "+a.size()+" items").setItems(rows,(d,w)->trashItem(a.get(w))).setNeutralButton("Empty Trash",(d,w)->new AlertDialog.Builder(this).setTitle("Empty Trash permanently?").setPositiveButton("Empty",(x,y)->new Thread(()->{TrashManager.empty(this);runOnUiThread(()->toast("Trash emptied"));}).start()).setNegativeButton("Cancel",null).show()).setPositiveButton("Close",null).show();}
    private void trashItem(TrashManager.Item i){String[]o={"Restore","Delete permanently"};new AlertDialog.Builder(this).setTitle(i.stored.getName()).setMessage("Original: "+i.original).setItems(o,(d,w)->{if(w==0)new Thread(()->{try{File r=TrashManager.restore(this,i);runOnUiThread(()->toast("Restored: "+r.getAbsolutePath()));}catch(Exception e){runOnUiThread(()->toast("Restore failed: "+e.getMessage()));}}).start();else new Thread(()->{TrashManager.purge(this,i);runOnUiThread(()->toast("Deleted permanently"));}).start();}).show();}

    private void hashDialog(File f){progress("Hashing…");new Thread(()->{try{String h=FileEngine.sha256(f);String msg="Path: "+f.getAbsolutePath()+"\nSize: "+fmt(f.length())+"\nModified: "+new Date(f.lastModified())+"\nMIME: "+FileEngine.mime(f)+"\n\nSHA-256\n"+h;runOnUiThread(()->new AlertDialog.Builder(this).setTitle(f.getName()).setMessage(msg).setPositiveButton("OK",null).show());}catch(Exception e){err(e);}}).start();}
    private void apkInfo(File f){PackageManager pm=getPackageManager();PackageInfo pi=pm.getPackageArchiveInfo(f.getAbsolutePath(),PackageManager.GET_ACTIVITIES);String msg;if(pi==null)msg="Unable to parse APK.";else{ApplicationInfo ai=pi.applicationInfo;if(ai!=null){ai.sourceDir=f.getAbsolutePath();ai.publicSourceDir=f.getAbsolutePath();}String label=ai==null?f.getName():String.valueOf(pm.getApplicationLabel(ai));msg=label+"\nPackage: "+pi.packageName+"\nVersion: "+pi.versionName+" ("+(Build.VERSION.SDK_INT>=28?pi.getLongVersionCode():pi.versionCode)+")\nSize: "+fmt(f.length());}new AlertDialog.Builder(this).setTitle("APK").setMessage(msg).setPositiveButton("Install",(d,w)->installApk(f)).setNegativeButton("Close",null).show();}
    private void installApk(File f){if(Build.VERSION.SDK_INT>=26&&!getPackageManager().canRequestPackageInstalls()){try{startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,Uri.parse("package:"+getPackageName())));toast("Allow BlazeFM to install unknown apps, then retry the APK.");}catch(Exception e){toast("Enable 'Install unknown apps' for BlazeFM in Android settings.");}return;}Intent i=new Intent(Intent.ACTION_VIEW);i.setDataAndType(BlazeProvider.uriFor(f),"application/vnd.android.package-archive");i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_ACTIVITY_NEW_TASK);try{startActivity(i);}catch(Exception e){toast("Installer unavailable: "+e.getMessage());}}

    private void storageInfo(){File e=Environment.getExternalStorageDirectory();long total=e.getTotalSpace(),free=e.getFreeSpace();new AlertDialog.Builder(this).setTitle("Storage").setMessage("Total: "+fmt(total)+"\nUsed: "+fmt(total-free)+"\nFree: "+fmt(free)+"\n\nCurrent folder:\n"+cwd.getAbsolutePath()).setPositiveButton("OK",null).show();}
    private void about(){String m="BlazeFM 1.2.0\ncom.blazefm.blazesystems\nAndroid 5.0+ (API 21)\n\nLocal file manager, batch copy/move, ZIP, Trash, hidden files, favorites/recent, exact SHA-256 duplicates, similar photos, analyzer, APK manager/backup, media/text preview, SAF cloud providers, SMB/FTP/SFTP, and optional root browser.\n\nNo ads or analytics.";new AlertDialog.Builder(this).setTitle("About BlazeFM").setMessage(m).setPositiveButton("OK",null).show();}

    private void progress(String s){cancel=false;status.setText(s+" · tap status to cancel");status.setOnClickListener(v->{cancel=true;status.setText("Cancelling…");});}
    private void setProgress(String s){runOnUiThread(()->status.setText(s));}
    private class P implements FileEngine.Progress{public void update(String s,int d,int t){setProgress(s);}public boolean cancelled(){return cancel;}}
    private void err(Exception e){runOnUiThread(()->{status.setText("Error");new AlertDialog.Builder(this).setTitle("Operation failed").setMessage(e.getClass().getSimpleName()+": "+e.getMessage()).setPositiveButton("OK",null).show();});}
    private String fmt(long n){String[]u={"B","KB","MB","GB","TB"};double v=n;int i=0;while(v>=1024&&i<u.length-1){v/=1024;i++;}return String.format(Locale.US,v>=10?"%.0f %s":"%.1f %s",v,u[i]);}
    private void toast(String s){Toast.makeText(this,s,Toast.LENGTH_LONG).show();}
    @Override public void onBackPressed(){if(!selected.isEmpty()){clearSelection();return;}if(showingResults){showDir(cwd);return;}File home=Environment.getExternalStorageDirectory();if(cwd!=null&&!cwd.equals(home)&&cwd.getParentFile()!=null)up();else super.onBackPressed();}
}
