package com.blazefm.blazesystems;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Build;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

final class Ui {
    static final int ORANGE = Color.rgb(255,107,0);
    static final int ORANGE_DARK = Color.rgb(24,27,34);
    static final int HEADER = Color.rgb(24,27,34);
    static final int HEADER_MUTED = Color.rgb(184,190,201);
    static final int BG = Color.rgb(246,247,249);
    static final int SURFACE = Color.WHITE;
    static final int SURFACE_2 = Color.rgb(240,242,245);
    static final int TEXT = Color.rgb(25,28,33);
    static final int MUTED = Color.rgb(101,109,122);
    static final int BORDER = Color.rgb(226,229,234);
    static final int ACCENT_SOFT = Color.rgb(255,237,224);
    static final int DANGER = Color.rgb(196,43,28);

    static int dp(Activity a,int n){return Math.round(n*a.getResources().getDisplayMetrics().density);}

    static TextView text(Activity a,String s,int sp){
        TextView v=new TextView(a);v.setText(s);v.setTextSize(sp);v.setTextColor(TEXT);v.setGravity(Gravity.CENTER_VERTICAL);
        v.setPadding(dp(a,14),dp(a,9),dp(a,14),dp(a,9));return v;
    }

    static Button button(Activity a,String s){
        Button b=baseButton(a,s,11);b.setTextColor(TEXT);b.setBackground(rounded(a,SURFACE_2,12,BORDER,1));return b;
    }

    static Button chipButton(Activity a,String s){
        Button b=baseButton(a,s,11);b.setTextColor(Color.WHITE);b.setBackground(rounded(a,Color.rgb(43,47,57),20,Color.rgb(60,65,76),1));return b;
    }

    static Button primaryButton(Activity a,String s){
        Button b=baseButton(a,s,11);b.setTypeface(Typeface.DEFAULT_BOLD);b.setTextColor(Color.WHITE);b.setBackground(rounded(a,ORANGE,20,ORANGE,0));return b;
    }

    static Button iconButton(Activity a,String s){
        Button b=baseButton(a,s,22);b.setTextColor(Color.WHITE);b.setBackground(rounded(a,Color.TRANSPARENT,20,Color.TRANSPARENT,0));return b;
    }

    static Button navButton(Activity a,String s,boolean active){
        Button b=baseButton(a,s,10);b.setTypeface(active?Typeface.DEFAULT_BOLD:Typeface.DEFAULT);b.setTextColor(active?ORANGE:MUTED);
        b.setBackground(rounded(a,active?ACCENT_SOFT:Color.TRANSPARENT,14,Color.TRANSPARENT,0));return b;
    }

    private static Button baseButton(Activity a,String s,int sp){
        Button b=new Button(a);b.setText(s);b.setTextSize(sp);b.setAllCaps(false);b.setGravity(Gravity.CENTER);
        b.setMinWidth(0);b.setMinimumWidth(0);b.setMinHeight(0);b.setMinimumHeight(0);b.setPadding(dp(a,8),0,dp(a,8),0);
        if(Build.VERSION.SDK_INT>=21)b.setStateListAnimator(null);return b;
    }

    static GradientDrawable bg(int color,float radius){GradientDrawable g=new GradientDrawable();g.setColor(color);g.setCornerRadius(radius);return g;}

    static GradientDrawable rounded(Activity a,int color,int radiusDp,int strokeColor,int strokeDp){
        GradientDrawable g=new GradientDrawable();g.setColor(color);g.setCornerRadius(dp(a,radiusDp));
        if(strokeDp>0)g.setStroke(dp(a,strokeDp),strokeColor);return g;
    }

    static LinearLayout screenHeader(Activity a,String title,String subtitle){
        LinearLayout box=new LinearLayout(a);box.setOrientation(LinearLayout.VERTICAL);box.setGravity(Gravity.CENTER_VERTICAL);
        box.setPadding(dp(a,16),dp(a,14),dp(a,16),dp(a,12));box.setBackgroundColor(HEADER);box.setElevation(dp(a,4));
        TextView t=text(a,title,20);t.setTextColor(Color.WHITE);t.setTypeface(Typeface.DEFAULT_BOLD);t.setPadding(0,0,0,0);box.addView(t);
        TextView s=text(a,subtitle,11);s.setTextColor(HEADER_MUTED);s.setPadding(0,dp(a,3),0,0);box.addView(s);return box;
    }

    static void prepareList(Activity a,android.widget.ListView list){
        list.setDivider(null);list.setDividerHeight(0);list.setClipToPadding(false);list.setPadding(dp(a,8),dp(a,8),dp(a,8),dp(a,10));list.setBackgroundColor(BG);
    }

    static TextView header(Activity a,String title,String subtitle){
        LinearLayout box=new LinearLayout(a);box.setOrientation(LinearLayout.VERTICAL);box.setGravity(Gravity.CENTER_VERTICAL);
        box.setPadding(dp(a,16),dp(a,12),dp(a,16),dp(a,10));box.setBackgroundColor(HEADER);
        TextView t=text(a,title,20);t.setTextColor(Color.WHITE);t.setTypeface(Typeface.DEFAULT_BOLD);t.setPadding(0,0,0,0);box.addView(t);
        TextView s=text(a,subtitle,11);s.setTextColor(HEADER_MUTED);s.setPadding(0,dp(a,2),0,0);box.addView(s);
        TextView holder=new TextView(a);holder.setTag(box);return holder;
    }

    static LinearLayout toolbar(Activity a){
        LinearLayout l=new LinearLayout(a);l.setOrientation(LinearLayout.HORIZONTAL);l.setPadding(dp(a,8),dp(a,6),dp(a,8),dp(a,6));l.setBackgroundColor(SURFACE);return l;
    }

    static void equalAdd(LinearLayout bar,View v){bar.addView(v,new LinearLayout.LayoutParams(0,dp((Activity)bar.getContext(),44),1));}
}
