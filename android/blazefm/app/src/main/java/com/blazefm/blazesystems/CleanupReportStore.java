package com.blazefm.blazesystems;

import android.content.Context;
import android.net.Uri;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

final class CleanupReportStore {
    private static final String PREFIX="cleanup_report_";
    private CleanupReportStore(){}

    static File saveDuplicates(Context context,List<FileEngine.DupGroup> groups)throws IOException{
        cleanupOld(context);
        File out=new File(context.getCacheDir(),PREFIX+System.currentTimeMillis()+".txt");
        long reclaim=0;
        for(FileEngine.DupGroup group:groups){
            reclaim+=group.size*Math.max(0,group.files.size()-1L);
        }

        try(BufferedWriter w=new BufferedWriter(new FileWriter(out))){
            w.write("DUP\t"+reclaim+"\t"+groups.size());
            w.newLine();
            for(FileEngine.DupGroup group:groups){
                w.write("G\t"+group.size+"\t"+Uri.encode(group.key));
                w.newLine();
                for(File file:group.files){
                    w.write("F\t"+Uri.encode(file.getAbsolutePath()));
                    w.newLine();
                }
                w.write("E");
                w.newLine();
            }
        }
        return out;
    }

    static File saveSimilar(Context context,FileEngine.SimilarResult result)throws IOException{
        cleanupOld(context);
        File out=new File(context.getCacheDir(),PREFIX+System.currentTimeMillis()+".txt");
        try(BufferedWriter w=new BufferedWriter(new FileWriter(out))){
            w.write("SIM\t"+result.totalPairs+"\t"+result.truncated);
            w.newLine();
            for(FileEngine.SimilarPair pair:result.pairs){
                w.write("P\t"+pair.distance+"\t"+Uri.encode(pair.a.getAbsolutePath())+"\t"+Uri.encode(pair.b.getAbsolutePath()));
                w.newLine();
            }
        }
        return out;
    }

    static DuplicateReport loadDuplicates(File report)throws IOException{
        ArrayList<FileEngine.DupGroup> groups=new ArrayList<>();
        long reclaim=0;
        try(BufferedReader r=new BufferedReader(new FileReader(report))){
            String line=r.readLine();
            if(line==null||!line.startsWith("DUP\t"))throw new IOException("Not a duplicate report");
            String[] header=line.split("\t");
            if(header.length>1)reclaim=parseLong(header[1]);

            String key="";
            long size=0;
            ArrayList<File> files=null;
            while((line=r.readLine())!=null){
                String[] parts=line.split("\t");
                if(parts.length==0)continue;
                if("G".equals(parts[0])){
                    size=parts.length>1?parseLong(parts[1]):0;
                    key=parts.length>2?Uri.decode(parts[2]):"";
                    files=new ArrayList<>();
                }else if("F".equals(parts[0])&&files!=null&&parts.length>1){
                    files.add(new File(Uri.decode(parts[1])));
                }else if("E".equals(parts[0])&&files!=null){
                    groups.add(new FileEngine.DupGroup(key,size,files));
                    files=null;
                }
            }
        }
        return new DuplicateReport(groups,reclaim);
    }

    static SimilarReport loadSimilar(File report)throws IOException{
        ArrayList<FileEngine.SimilarPair> pairs=new ArrayList<>();
        long total=0;
        boolean truncated=false;
        try(BufferedReader r=new BufferedReader(new FileReader(report))){
            String line=r.readLine();
            if(line==null||!line.startsWith("SIM\t"))throw new IOException("Not a similar-photo report");
            String[] header=line.split("\t");
            if(header.length>1)total=parseLong(header[1]);
            if(header.length>2)truncated=Boolean.parseBoolean(header[2]);

            while((line=r.readLine())!=null){
                String[] parts=line.split("\t");
                if(parts.length>=4&&"P".equals(parts[0])){
                    int distance=parseInt(parts[1]);
                    File a=new File(Uri.decode(parts[2]));
                    File b=new File(Uri.decode(parts[3]));
                    pairs.add(new FileEngine.SimilarPair(a,b,distance));
                }
            }
        }
        return new SimilarReport(pairs,total,truncated);
    }

    static final class DuplicateReport {
        final List<FileEngine.DupGroup> groups;
        final long reclaim;
        DuplicateReport(List<FileEngine.DupGroup> g,long r){groups=g;reclaim=r;}
    }

    static final class SimilarReport {
        final List<FileEngine.SimilarPair> pairs;
        final long totalPairs;
        final boolean truncated;
        SimilarReport(List<FileEngine.SimilarPair> p,long t,boolean tr){pairs=p;totalPairs=t;truncated=tr;}
    }

    private static long parseLong(String value){
        try{return Long.parseLong(value);}catch(Exception e){return 0;}
    }

    private static int parseInt(String value){
        try{return Integer.parseInt(value);}catch(Exception e){return 0;}
    }

    private static void cleanupOld(Context context){
        File[] files=context.getCacheDir().listFiles();
        if(files==null)return;
        long cutoff=System.currentTimeMillis()-24L*60L*60L*1000L;
        for(File file:files){
            if(file.getName().startsWith(PREFIX)&&file.lastModified()<cutoff)file.delete();
        }
    }
}
