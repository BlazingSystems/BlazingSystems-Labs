package com.blazefm.blazesystems;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.button.MaterialButton;
import com.google.android.material.card.MaterialCardView;

import java.util.ArrayList;
import java.util.List;

final class SimpleRowAdapter extends RecyclerView.Adapter<SimpleRowAdapter.Holder> {
    static final class Row {
        final int iconRes;
        final String title;
        final String subtitle;
        Row(int icon,String t,String s){iconRes=icon;title=t;subtitle=s;}
    }

    interface Listener {
        void onClick(int position);
        void onMore(int position);
        boolean onLongClick(int position);
    }

    private final ArrayList<Row> rows=new ArrayList<>();
    private final Listener listener;

    SimpleRowAdapter(Listener l){listener=l;setHasStableIds(true);}

    void submit(List<Row> data){
        rows.clear();
        if(data!=null)rows.addAll(data);
        notifyDataSetChanged();
    }

    @Override public long getItemId(int position){
        Row r=rows.get(position);
        return (r.title+"|"+r.subtitle).hashCode();
    }

    @Override public int getItemCount(){return rows.size();}

    @Override public Holder onCreateViewHolder(ViewGroup parent,int viewType){
        return new Holder(LayoutInflater.from(parent.getContext()).inflate(R.layout.item_simple_row,parent,false));
    }

    @Override public void onBindViewHolder(Holder h,int position){
        Row row=rows.get(position);
        h.icon.setImageResource(row.iconRes);
        h.title.setText(row.title);
        h.subtitle.setText(row.subtitle==null?"":row.subtitle);
        h.subtitle.setVisibility(row.subtitle==null||row.subtitle.isEmpty()?View.GONE:View.VISIBLE);
        h.card.setOnClickListener(v->listener.onClick(h.getBindingAdapterPosition()));
        h.more.setOnClickListener(v->listener.onMore(h.getBindingAdapterPosition()));
        h.card.setOnLongClickListener(v->listener.onLongClick(h.getBindingAdapterPosition()));
    }

    static final class Holder extends RecyclerView.ViewHolder{
        final MaterialCardView card;
        final ImageView icon;
        final TextView title,subtitle;
        final MaterialButton more;
        Holder(View item){
            super(item);
            card=item.findViewById(R.id.simple_card);
            icon=item.findViewById(R.id.simple_icon);
            title=item.findViewById(R.id.simple_title);
            subtitle=item.findViewById(R.id.simple_subtitle);
            more=item.findViewById(R.id.simple_more);
        }
    }
}
