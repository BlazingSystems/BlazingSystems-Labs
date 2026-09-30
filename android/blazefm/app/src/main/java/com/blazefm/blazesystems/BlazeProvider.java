package com.blazefm.blazesystems;
import android.content.*;import android.database.Cursor;import android.database.MatrixCursor;import android.net.Uri;import android.os.*;import android.provider.OpenableColumns;import java.io.*;import java.net.*;
public class BlazeProvider extends ContentProvider {
 static final String AUTH="com.blazefm.blazesystems.files";
 public static Uri uriFor(File f){try{return Uri.parse("content://"+AUTH+"/f?path="+URLEncoder.encode(f.getCanonicalPath(),"UTF-8"));}catch(Exception e){return Uri.EMPTY;}}
 private File file(Uri u)throws FileNotFoundException{try{String p=u.getQueryParameter("path");if(p==null)throw new FileNotFoundException();File f=new File(p).getCanonicalFile();if(!f.exists()||!f.isFile())throw new FileNotFoundException();return f;}catch(IOException e){throw new FileNotFoundException();}}
 public boolean onCreate(){return true;}
 public String getType(Uri u){String n;try{n=file(u).getName();}catch(Exception e){return "application/octet-stream";}String ext=android.webkit.MimeTypeMap.getFileExtensionFromUrl(n);String m=android.webkit.MimeTypeMap.getSingleton().getMimeTypeFromExtension(ext.toLowerCase());return m==null?"application/octet-stream":m;}
 public Cursor query(Uri u,String[] projection,String sel,String[] args,String sort){try{File f=file(u);MatrixCursor c=new MatrixCursor(new String[]{OpenableColumns.DISPLAY_NAME,OpenableColumns.SIZE});c.addRow(new Object[]{f.getName(),f.length()});return c;}catch(Exception e){return null;}}
 public android.os.ParcelFileDescriptor openFile(Uri u,String mode)throws FileNotFoundException{return ParcelFileDescriptor.open(file(u),ParcelFileDescriptor.MODE_READ_ONLY);}
 public int delete(Uri u,String s,String[]a){throw new UnsupportedOperationException();} public int update(Uri u,ContentValues v,String s,String[]a){throw new UnsupportedOperationException();} public Uri insert(Uri u,ContentValues v){throw new UnsupportedOperationException();}
}
