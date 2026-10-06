package com.blazefm.blazesystems;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.bottomsheet.BottomSheetBehavior;
import com.google.android.material.bottomsheet.BottomSheetDialog;

import java.util.ArrayList;
import java.util.List;

final class ActionSheet {
    static final class Item {
        final int iconRes;
        final String title;
        final String subtitle;
        final boolean danger;
        final Runnable action;

        Item(int icon,String t,String s,boolean d,Runnable a){
            iconRes=icon;title=t;subtitle=s;danger=d;action=a;
        }
    }

    private ActionSheet(){}

    static Item item(int icon,String title,String subtitle,Runnable action){
        return new Item(icon,title,subtitle,false,action);
    }

    static Item danger(int icon,String title,String subtitle,Runnable action){
        return new Item(icon,title,subtitle,true,action);
    }

    static void show(Activity activity,String title,String subtitle,List<Item> items){
        BottomSheetDialog dialog=new BottomSheetDialog(activity);
        View root=LayoutInflater.from(activity).inflate(R.layout.sheet_action_list,null,false);

        TextView titleView=root.findViewById(R.id.sheet_title);
        TextView subtitleView=root.findViewById(R.id.sheet_subtitle);
        titleView.setText(title);
        subtitleView.setText(subtitle==null?"":subtitle);
        subtitleView.setVisibility(subtitle==null||subtitle.isEmpty()?View.GONE:View.VISIBLE);

        RecyclerView list=root.findViewById(R.id.sheet_list);
        list.setLayoutManager(new LinearLayoutManager(activity));
        list.setItemAnimator(null);
        list.setAdapter(new Adapter(activity,dialog,items));

        dialog.setContentView(root);
        dialog.setOnShowListener(d->{
            View sheet=dialog.findViewById(com.google.android.material.R.id.design_bottom_sheet);
            if(sheet!=null){
                BottomSheetBehavior<View> behavior=BottomSheetBehavior.from(sheet);
                behavior.setState(BottomSheetBehavior.STATE_EXPANDED);
                behavior.setSkipCollapsed(true);
            }
        });
        dialog.show();
    }

    private static final class Adapter extends RecyclerView.Adapter<Holder> {
        private final Activity activity;
        private final BottomSheetDialog dialog;
        private final ArrayList<Item> items=new ArrayList<>();

        Adapter(Activity a,BottomSheetDialog d,List<Item> data){
            activity=a;dialog=d;if(data!=null)items.addAll(data);
        }

        @Override public int getItemCount(){return items.size();}

        @Override public Holder onCreateViewHolder(ViewGroup parent,int viewType){
            return new Holder(LayoutInflater.from(parent.getContext()).inflate(R.layout.item_sheet_action,parent,false));
        }

        @Override public void onBindViewHolder(Holder h,int position){
            Item item=items.get(position);
            h.icon.setImageResource(item.iconRes);
            h.title.setText(item.title);
            h.subtitle.setText(item.subtitle==null?"":item.subtitle);
            h.subtitle.setVisibility(item.subtitle==null||item.subtitle.isEmpty()?View.GONE:View.VISIBLE);

            int tint=ContextCompat.getColor(activity,item.danger?R.color.blaze_error:R.color.blaze_primary);
            h.icon.setImageTintList(ColorStateList.valueOf(tint));
            h.title.setTextColor(ContextCompat.getColor(activity,item.danger?R.color.blaze_error:R.color.blaze_on_surface));

            h.row.setOnClickListener(v->{
                dialog.dismiss();
                if(item.action!=null)item.action.run();
            });
        }
    }

    private static final class Holder extends RecyclerView.ViewHolder{
        final View row;
        final ImageView icon;
        final TextView title,subtitle;
        Holder(View item){
            super(item);
            row=item.findViewById(R.id.action_row);
            icon=item.findViewById(R.id.action_icon);
            title=item.findViewById(R.id.action_title);
            subtitle=item.findViewById(R.id.action_subtitle);
        }
    }
}
