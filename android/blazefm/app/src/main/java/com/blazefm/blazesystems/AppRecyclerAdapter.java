package com.blazefm.blazesystems;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.button.MaterialButton;
import com.google.android.material.card.MaterialCardView;

import java.util.List;

final class AppRecyclerAdapter extends RecyclerView.Adapter<AppRecyclerAdapter.Holder> {
    interface Listener { void onApp(ApplicationInfo app); }

    private final PackageManager pm;
    private final List<ApplicationInfo> apps;
    private final Listener listener;

    AppRecyclerAdapter(PackageManager packageManager,List<ApplicationInfo> items,Listener l){
        pm=packageManager;apps=items;listener=l;setHasStableIds(true);
    }

    @Override public long getItemId(int position){return apps.get(position).packageName.hashCode();}
    @Override public int getItemCount(){return apps.size();}

    @Override public Holder onCreateViewHolder(ViewGroup parent,int viewType){
        return new Holder(LayoutInflater.from(parent.getContext()).inflate(R.layout.item_app,parent,false));
    }

    @Override public void onBindViewHolder(Holder h,int position){
        ApplicationInfo app=apps.get(position);
        CharSequence label=pm.getApplicationLabel(app);
        h.name.setText(label);
        boolean system=(app.flags&ApplicationInfo.FLAG_SYSTEM)!=0;
        h.pkg.setText(app.packageName+(system?" · system":""));
        try{
            Drawable icon=pm.getApplicationIcon(app);
            h.icon.setImageDrawable(icon);
        }catch(Exception e){
            h.icon.setImageResource(R.drawable.ic_apps);
        }
        View.OnClickListener click=v->listener.onApp(app);
        h.card.setOnClickListener(click);
        h.more.setOnClickListener(click);
    }

    static final class Holder extends RecyclerView.ViewHolder{
        final MaterialCardView card;
        final ImageView icon;
        final TextView name,pkg;
        final MaterialButton more;
        Holder(View item){
            super(item);
            card=item.findViewById(R.id.app_card);
            icon=item.findViewById(R.id.app_icon);
            name=item.findViewById(R.id.app_name);
            pkg=item.findViewById(R.id.app_package);
            more=item.findViewById(R.id.app_more);
        }
    }
}
