package com.blazefm.blazesystems;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Typeface;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.LinearLayout;
import android.widget.TextView;

final class ModernListAdapter extends BaseAdapter {
    private final Activity a;private final String[] rows;private final String badge;
    ModernListAdapter(Activity activity,String[] data,String icon){a=activity;rows=data;badge=icon;}
    public int getCount(){return rows.length;}
    public String getItem(int p){return rows[p];}
    public long getItemId(int p){return p;}
    public View getView(int p,View old,ViewGroup parent){
        String raw=rows[p]==null?"":rows[p];int split=raw.indexOf('\n');String first=split>=0?raw.substring(0,split):raw;String second=split>=0?raw.substring(split+1):"";
        LinearLayout outer=new LinearLayout(a);outer.setPadding(Ui.dp(a,4),Ui.dp(a,3),Ui.dp(a,4),Ui.dp(a,3));
        LinearLayout row=new LinearLayout(a);row.setOrientation(LinearLayout.HORIZONTAL);row.setGravity(Gravity.CENTER_VERTICAL);row.setPadding(Ui.dp(a,12),Ui.dp(a,10),Ui.dp(a,10),Ui.dp(a,10));
        row.setBackground(Ui.rounded(a,Ui.SURFACE,16,Ui.BORDER,1));row.setElevation(Ui.dp(a,1));outer.addView(row,new LinearLayout.LayoutParams(-1,-2));
        TextView icon=Ui.text(a,badge,16);icon.setGravity(Gravity.CENTER);icon.setTypeface(Typeface.DEFAULT_BOLD);icon.setTextColor(Ui.ORANGE);icon.setPadding(0,0,0,0);icon.setBackground(Ui.rounded(a,Ui.ACCENT_SOFT,14,Color.TRANSPARENT,0));row.addView(icon,new LinearLayout.LayoutParams(Ui.dp(a,44),Ui.dp(a,44)));
        LinearLayout info=new LinearLayout(a);info.setOrientation(LinearLayout.VERTICAL);info.setPadding(Ui.dp(a,12),0,Ui.dp(a,8),0);
        TextView t=Ui.text(a,first,14);t.setTypeface(Typeface.DEFAULT_BOLD);t.setSingleLine(true);t.setEllipsize(android.text.TextUtils.TruncateAt.MIDDLE);t.setPadding(0,0,0,0);info.addView(t);
        if(!second.isEmpty()){TextView s=Ui.text(a,second,11);s.setTextColor(Ui.MUTED);s.setSingleLine(true);s.setEllipsize(android.text.TextUtils.TruncateAt.MIDDLE);s.setPadding(0,Ui.dp(a,3),0,0);info.addView(s);}
        row.addView(info,new LinearLayout.LayoutParams(0,-2,1));
        TextView more=Ui.text(a,"›",22);more.setTextColor(Ui.MUTED);more.setGravity(Gravity.CENTER);more.setPadding(0,0,0,0);row.addView(more,new LinearLayout.LayoutParams(Ui.dp(a,30),Ui.dp(a,44)));
        return outer;
    }
}
