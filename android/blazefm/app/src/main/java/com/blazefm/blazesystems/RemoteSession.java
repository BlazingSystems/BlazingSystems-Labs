package com.blazefm.blazesystems;

import com.jcraft.jsch.*;
import org.apache.commons.net.ftp.*;
import java.io.*;
import java.util.*;
import jcifs.CIFSContext;
import jcifs.config.PropertyConfiguration;
import jcifs.context.BaseContext;
import jcifs.smb.*;

interface RemoteSession extends Closeable {
    final class Entry { final String name,path; final boolean dir; final long size; Entry(String n,String p,boolean d,long s){name=n;path=p;dir=d;size=s;} }
    List<Entry> list(String path)throws Exception;
    void download(String path,OutputStream out)throws Exception;
    void upload(String path,InputStream in)throws Exception;
    void mkdir(String path)throws Exception;
    void delete(String path,boolean dir)throws Exception;
    void rename(String from,String to)throws Exception;
    String parent(String path);
}

interface HostKeyPrompt { boolean confirm(String message); }\n\nfinal class RemoteFactory {
    static RemoteSession connect(String proto,String host,int port,String user,String pass,String share,String domain,File appFiles,HostKeyPrompt hostKeyPrompt)throws Exception{
        if("FTP".equals(proto))return new FtpSession(host,port<=0?21:port,user,pass);
        if("SFTP".equals(proto))return new SftpSession(host,port<=0?22:port,user,pass,appFiles,hostKeyPrompt);
        if("SMB".equals(proto))return new SmbSession(host,share,user,pass,domain);
        throw new IOException("Unsupported protocol");
    }
    static String join(String base,String name){if(base==null||base.isEmpty()||"/".equals(base))return "/"+name;return base+(base.endsWith("/")?"":"/")+name;}
}

final class FtpSession implements RemoteSession {
    private final FTPClient c=new FTPClient();
    FtpSession(String host,int port,String user,String pass)throws Exception{c.setConnectTimeout(15000);c.connect(host,port);if(!FTPReply.isPositiveCompletion(c.getReplyCode()))throw new IOException("FTP connect rejected: "+c.getReplyString());if(!c.login(user==null?"anonymous":user,pass==null?"":pass))throw new IOException("FTP login failed");c.enterLocalPassiveMode();c.setFileType(FTP.BINARY_FILE_TYPE);c.setDataTimeout(15000);}
    public List<Entry>list(String path)throws Exception{FTPFile[]a=c.listFiles(path);ArrayList<Entry>r=new ArrayList<>();if(a!=null)for(FTPFile f:a){String n=f.getName();if(".".equals(n)||"..".equals(n))continue;r.add(new Entry(n,RemoteFactory.join(path,n),f.isDirectory(),f.getSize()));}sort(r);return r;}
    public void download(String p,OutputStream out)throws Exception{if(!c.retrieveFile(p,out))throw new IOException(c.getReplyString());}
    public void upload(String p,InputStream in)throws Exception{if(!c.storeFile(p,in))throw new IOException(c.getReplyString());}
    public void mkdir(String p)throws Exception{if(!c.makeDirectory(p))throw new IOException(c.getReplyString());}
    public void delete(String p,boolean dir)throws Exception{boolean ok=dir?c.removeDirectory(p):c.deleteFile(p);if(!ok)throw new IOException(c.getReplyString());}
    public void rename(String a,String b)throws Exception{if(!c.rename(a,b))throw new IOException(c.getReplyString());}
    public String parent(String p){return parentPath(p);}
    public void close(){try{if(c.isConnected()){c.logout();c.disconnect();}}catch(Exception ignored){}}
    static void sort(List<Entry>r){Collections.sort(r,(a,b)->{if(a.dir!=b.dir)return a.dir?-1:1;return a.name.compareToIgnoreCase(b.name);});}
    static String parentPath(String p){if(p==null||p.isEmpty()||"/".equals(p))return "/";String x=p.endsWith("/")?p.substring(0,p.length()-1):p;int i=x.lastIndexOf('/');return i<=0?"/":x.substring(0,i);}
}

final class SftpSession implements RemoteSession {
    private final Session session; private final ChannelSftp sftp;
    SftpSession(String host,int port,String user,String pass,File files,HostKeyPrompt hostKeyPrompt)throws Exception{
        JSch j=new JSch();File known=new File(files,"known_hosts");if(!known.exists())known.createNewFile();j.setKnownHosts(known.getAbsolutePath());session=j.getSession(user,host,port);session.setPassword(pass);session.setConfig("StrictHostKeyChecking","ask");session.setConfig("PreferredAuthentications","publickey,password");session.setTimeout(15000);session.setUserInfo(new UserInfo(){public String getPassphrase(){return null;}public String getPassword(){return pass;}public boolean promptPassword(String m){return true;}public boolean promptPassphrase(String m){return false;}public boolean promptYesNo(String m){return hostKeyPrompt!=null&&hostKeyPrompt.confirm(m);}public void showMessage(String m){}});session.connect(15000);sftp=(ChannelSftp)session.openChannel("sftp");sftp.connect(15000);
    }
    public List<Entry>list(String path)throws Exception{Vector<?>v=sftp.ls(path);ArrayList<Entry>r=new ArrayList<>();for(Object o:v){ChannelSftp.LsEntry e=(ChannelSftp.LsEntry)o;String n=e.getFilename();if(".".equals(n)||"..".equals(n))continue;r.add(new Entry(n,RemoteFactory.join(path,n),e.getAttrs().isDir(),e.getAttrs().getSize()));}FtpSession.sort(r);return r;}
    public void download(String p,OutputStream out)throws Exception{sftp.get(p,out);}
    public void upload(String p,InputStream in)throws Exception{sftp.put(in,p);}
    public void mkdir(String p)throws Exception{sftp.mkdir(p);}
    public void delete(String p,boolean dir)throws Exception{if(dir)sftp.rmdir(p);else sftp.rm(p);}
    public void rename(String a,String b)throws Exception{sftp.rename(a,b);}
    public String parent(String p){return FtpSession.parentPath(p);}
    public void close(){try{sftp.disconnect();}catch(Exception ignored){}try{session.disconnect();}catch(Exception ignored){}}
}

final class SmbSession implements RemoteSession {
    private final CIFSContext ctx; private final String base;
    SmbSession(String host,String share,String user,String pass,String domain)throws Exception{if(share==null||share.trim().isEmpty())throw new IOException("SMB share is required");Properties p=new Properties();p.setProperty("jcifs.smb.client.enableSMB2","true");p.setProperty("jcifs.smb.client.disableSMB1","true");CIFSContext b=new BaseContext(new PropertyConfiguration(p));ctx=b.withCredentials(new NtlmPasswordAuthenticator(domain==null?"":domain,user==null?"":user,pass==null?"":pass));base="smb://"+host+"/"+share.replaceAll("^/+|/+$","")+"/";new SmbFile(base,ctx).connect();}
    private SmbFile f(String p)throws Exception{String x=p==null?"":p;if(x.startsWith("/"))x=x.substring(1);return new SmbFile(base+x,ctx);}
    public List<Entry>list(String path)throws Exception{SmbFile d=f(path);SmbFile[]a=d.listFiles();ArrayList<Entry>r=new ArrayList<>();if(a!=null)for(SmbFile x:a){String n=x.getName();if(n.endsWith("/"))n=n.substring(0,n.length()-1);r.add(new Entry(n,RemoteFactory.join(path,n),x.isDirectory(),x.isFile()?x.length():0));}FtpSession.sort(r);return r;}
    public void download(String p,OutputStream out)throws Exception{try(InputStream in=new SmbFileInputStream(f(p))){byte[]b=new byte[64*1024];int n;while((n=in.read(b))!=-1)out.write(b,0,n);}}
    public void upload(String p,InputStream in)throws Exception{try(OutputStream out=new SmbFileOutputStream(f(p))){byte[]b=new byte[64*1024];int n;while((n=in.read(b))!=-1)out.write(b,0,n);}}
    public void mkdir(String p)throws Exception{f(p).mkdir();}
    public void delete(String p,boolean dir)throws Exception{f(p).delete();}
    public void rename(String a,String b)throws Exception{f(a).renameTo(f(b));}
    public String parent(String p){return FtpSession.parentPath(p);}
    public void close(){}
}
