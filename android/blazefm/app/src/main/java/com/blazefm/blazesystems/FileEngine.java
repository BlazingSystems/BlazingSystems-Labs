package com.blazefm.blazesystems;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import java.io.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.zip.*;

public final class FileEngine {
    private FileEngine() {}
    public interface Progress { void update(String text, int done, int total); boolean cancelled(); }
    public static final class DupGroup { public final String key; public final long size; public final List<File> files; DupGroup(String k,long s,List<File> f){key=k;size=s;files=f;} }
    public static List<File> walk(File root, Progress p) {
        ArrayList<File> out=new ArrayList<>(); ArrayDeque<File> q=new ArrayDeque<>(); if(root!=null)q.add(root); int n=0;
        while(!q.isEmpty() && !p.cancelled()) { File f=q.removeFirst(); File[] a;
            try { a=f.listFiles(); } catch(SecurityException e){continue;} if(a==null)continue;
            for(File x:a){ if(p.cancelled())break; if(x.isDirectory())q.addLast(x); else {out.add(x); if((++n&127)==0)p.update("Scanning… "+n,0,0);} }
        } return out;
    }
    public static List<DupGroup> exactDuplicates(File root, Progress p) throws Exception {
        List<File> all=walk(root,p); Map<Long,List<File>> sizes=new HashMap<>(); int i=0;
        for(File f:all){ if(f.length()>0) sizes.computeIfAbsent(f.length(),k->new ArrayList<>()).add(f); }
        Map<String,List<File>> quick=new HashMap<>();
        for(List<File> g:sizes.values()) if(g.size()>1) for(File f:g){ if(p.cancelled())return Collections.emptyList(); String k=f.length()+":"+sampleHash(f); quick.computeIfAbsent(k,x->new ArrayList<>()).add(f); p.update("Quick hashing: "+f.getName(),++i,all.size()); }
        Map<String,List<File>> full=new LinkedHashMap<>();
        for(List<File> g:quick.values()) if(g.size()>1) for(File f:g){ if(p.cancelled())return Collections.emptyList(); String h=sha256(f); full.computeIfAbsent(h,x->new ArrayList<>()).add(f); p.update("Verifying: "+f.getName(),i,all.size()); }
        ArrayList<DupGroup> r=new ArrayList<>(); for(Map.Entry<String,List<File>> e:full.entrySet()) if(e.getValue().size()>1) r.add(new DupGroup(e.getKey(),e.getValue().get(0).length(),e.getValue()));
        Collections.sort(r,(a,b)->Long.compare(b.size*(b.files.size()-1L),a.size*(a.files.size()-1L))); return r;
    }
    private static String sampleHash(File f)throws Exception { MessageDigest md=MessageDigest.getInstance("SHA-256"); long len=f.length(); byte[] b=new byte[8192]; try(RandomAccessFile r=new RandomAccessFile(f,"r")){ int n=r.read(b); if(n>0)md.update(b,0,n); if(len>16384){r.seek(Math.max(0,len/2-4096));n=r.read(b);if(n>0)md.update(b,0,n);r.seek(Math.max(0,len-8192));n=r.read(b);if(n>0)md.update(b,0,n);} } return hex(md.digest()); }
    public static String sha256(File f)throws Exception { MessageDigest md=MessageDigest.getInstance("SHA-256"); byte[] b=new byte[64*1024]; try(InputStream in=new BufferedInputStream(new FileInputStream(f),64*1024)){int n;while((n=in.read(b))!=-1)md.update(b,0,n);} return hex(md.digest()); }
    private static String hex(byte[] b){StringBuilder s=new StringBuilder(b.length*2);for(byte x:b)s.append(String.format(Locale.US,"%02x",x&255));return s.toString();}
    public static long dHash(File f){ BitmapFactory.Options o=new BitmapFactory.Options();o.inJustDecodeBounds=true;BitmapFactory.decodeFile(f.getAbsolutePath(),o);int m=Math.max(o.outWidth,o.outHeight);o.inSampleSize=1;while(m/o.inSampleSize>256)o.inSampleSize*=2;o.inJustDecodeBounds=false;o.inPreferredConfig=Bitmap.Config.RGB_565;Bitmap src=BitmapFactory.decodeFile(f.getAbsolutePath(),o);if(src==null)return Long.MIN_VALUE;Bitmap b=Bitmap.createScaledBitmap(src,9,8,true);if(b!=src)src.recycle();long h=0;for(int y=0;y<8;y++)for(int x=0;x<8;x++){int a=b.getPixel(x,y),c=b.getPixel(x+1,y);int ga=((a>>16)&255)*30+((a>>8)&255)*59+(a&255)*11;int gc=((c>>16)&255)*30+((c>>8)&255)*59+(c&255)*11;h=(h<<1)|(ga>gc?1:0);}b.recycle();return h; }
    public static int hamming(long a,long b){return Long.bitCount(a^b);}
    public static boolean isImage(File f){String n=f.getName().toLowerCase(Locale.US);return n.endsWith(".jpg")||n.endsWith(".jpeg")||n.endsWith(".png")||n.endsWith(".webp")||n.endsWith(".bmp");}
    public static void copy(File src,File dst)throws IOException{if(src.isDirectory()){if(!dst.exists()&&!dst.mkdirs())throw new IOException("Cannot create "+dst);File[] a=src.listFiles();if(a!=null)for(File f:a)copy(f,new File(dst,f.getName()));}else{File par=dst.getParentFile();if(par!=null&&!par.exists())par.mkdirs();try(InputStream in=new BufferedInputStream(new FileInputStream(src));OutputStream out=new BufferedOutputStream(new FileOutputStream(dst))){byte[] b=new byte[64*1024];int n;while((n=in.read(b))!=-1)out.write(b,0,n);}}}
    public static boolean delete(File f){if(f.isDirectory()){File[]a=f.listFiles();if(a!=null)for(File x:a)if(!delete(x))return false;}return f.delete();}
    public static void zip(List<File> files,File out)throws IOException{try(ZipOutputStream z=new ZipOutputStream(new BufferedOutputStream(new FileOutputStream(out)))){byte[]buf=new byte[64*1024];for(File f:files)zipOne(z,f,f.getName(),buf);}}
    private static void zipOne(ZipOutputStream z,File f,String name,byte[]buf)throws IOException{if(f.isDirectory()){File[]a=f.listFiles();if(a!=null)for(File x:a)zipOne(z,x,name+"/"+x.getName(),buf);return;}z.putNextEntry(new ZipEntry(name));try(InputStream in=new BufferedInputStream(new FileInputStream(f))){int n;while((n=in.read(buf))!=-1)z.write(buf,0,n);}z.closeEntry();}
}
