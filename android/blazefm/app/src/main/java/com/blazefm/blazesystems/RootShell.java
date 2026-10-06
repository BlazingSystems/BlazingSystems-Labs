package com.blazefm.blazesystems;

import java.io.*;
import java.util.*;

final class RootShell {
    static final class Entry{final String name,path;final boolean dir;final long size;Entry(String n,String p,boolean d,long s){name=n;path=p;dir=d;size=s;}}
    static boolean available(){try{Process p=new ProcessBuilder("su","-c","id").redirectErrorStream(true).start();String s=readText(p.getInputStream());return p.waitFor()==0&&s.contains("uid=0");}catch(Exception e){return false;}}
    static List<Entry> list(String dir)throws Exception{String q=quote(dir);String script="d="+q+"; for f in \"$d\"/* \"$d\"/.[!.]* \"$d\"/..?*; do [ -e \"$f\" ] || continue; if [ -d \"$f\" ]; then t=D; else t=F; fi; s=$(stat -c %s \"$f\" 2>/dev/null || echo 0); n=${f##*/}; printf '%s\\t%s\\t%s\\n' \"$t\" \"$s\" \"$n\"; done";String out=execText(script);ArrayList<Entry>r=new ArrayList<>();for(String line:out.split("\\n")){String[]p=line.split("\\t",3);if(p.length<3)continue;long z=0;try{z=Long.parseLong(p[1]);}catch(Exception ignored){}r.add(new Entry(p[2],join(dir,p[2]),"D".equals(p[0]),z));}Collections.sort(r,(a,b)->{if(a.dir!=b.dir)return a.dir?-1:1;return a.name.compareToIgnoreCase(b.name);});return r;}
    static void delete(String path)throws Exception{execChecked("rm -rf -- "+quote(path));}
    static void rename(String from,String to)throws Exception{execChecked("mv -- "+quote(from)+" "+quote(to));}
    static void copyFile(String from,File to)throws Exception{try(OutputStream out=new BufferedOutputStream(new FileOutputStream(to))){copyTo(from,out);}}
    static void copyTo(String from,OutputStream out)throws Exception{Process p=new ProcessBuilder("su","-c","cat -- "+quote(from)).redirectErrorStream(false).start();try{byte[]b=new byte[64*1024];int n;while((n=p.getInputStream().read(b))!=-1)out.write(b,0,n);out.flush();String err=readText(p.getErrorStream());if(p.waitFor()!=0)throw new IOException(err);}finally{try{p.getInputStream().close();}catch(Exception ignored){}try{p.getErrorStream().close();}catch(Exception ignored){}}}
    static String parent(String p){if(p==null||p.isEmpty()||"/".equals(p))return "/";String x=p.endsWith("/")?p.substring(0,p.length()-1):p;int i=x.lastIndexOf('/');return i<=0?"/":x.substring(0,i);}
    private static String join(String a,String b){return "/".equals(a)?"/"+b:a+"/"+b;}
    private static String quote(String s){return "'"+s.replace("'","'\\''")+"'";}
    private static String execText(String cmd)throws Exception{Process p=new ProcessBuilder("su","-c",cmd).redirectErrorStream(true).start();String s=readText(p.getInputStream());if(p.waitFor()!=0)throw new IOException(s);return s;}
    private static void execChecked(String cmd)throws Exception{execText(cmd);}
    private static String readText(InputStream in)throws IOException{ByteArrayOutputStream o=new ByteArrayOutputStream();byte[]b=new byte[8192];int n;while((n=in.read(b))!=-1)o.write(b,0,n);return new String(o.toByteArray(),"UTF-8");}
}
