package com.blazefm.blazesystems;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

final class Ui {
    static final int ORANGE = Color.rgb(255,109,0);
    static final int ORANGE_DARK = Color.rgb(230,81,0);
    static final int BG = Color.rgb(245,246,248);
    static final int TEXT = Color.rgb(28,27,31);
    static int dp(Activity a, int n){ return Math.round(n*a.getResources().getDisplayMetrics().density); }
    static TextView text(Activity a,String s,int sp){
        TextView v=new TextView(a); v.setText(s); v.setTextSize(sp); v.setTextColor(TEXT);
        v.setPadding(dp(a,14),dp(a,10),dp(a,14),dp(a,10)); return v;
    }
    static Button button(Activity a,String s){
        Button b=new Button(a); b.setText(s); b.setTextSize(11); b.setAllCaps(false);
        b.setMinWidth(0); b.setMinimumWidth(0); b.setPadding(dp(a,8),0,dp(a,8),0); return b;
    }
    static GradientDrawable bg(int color,float radius){ GradientDrawable g=new GradientDrawable(); g.setColor(color); g.setCornerRadius(radius); return g; }
    static TextView header(Activity a,String title,String subtitle){
        LinearLayout box=new LinearLayout(a); box.setOrientation(LinearLayout.VERTICAL); box.setGravity(Gravity.CENTER_VERTICAL);
        box.setPadding(dp(a,16),dp(a,10),dp(a,16),dp(a,8)); box.setBackgroundColor(ORANGE_DARK);
        TextView t=text(a,title,20); t.setTextColor(Color.WHITE); t.setPadding(0,0,0,0); box.addView(t);
        TextView s=text(a,subtitle,11); s.setTextColor(0xFFECECEC); s.setPadding(0,dp(a,2),0,0); box.addView(s);
        TextView holder=new TextView(a); holder.setTag(box); return holder;
    }
    static LinearLayout toolbar(Activity a){ LinearLayout l=new LinearLayout(a); l.setOrientation(LinearLayout.HORIZONTAL); l.setPadding(dp(a,4),dp(a,3),dp(a,4),dp(a,3)); l.setBackgroundColor(Color.WHITE); return l; }
    static void equalAdd(LinearLayout bar, View v){ bar.addView(v,new LinearLayout.LayoutParams(0,dp((Activity)bar.getContext(),44),1)); }
}
