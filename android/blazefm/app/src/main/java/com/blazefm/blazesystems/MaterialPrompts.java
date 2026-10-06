package com.blazefm.blazesystems;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

final class MaterialPrompts {
    interface TextAction{void run(String value);}
    private MaterialPrompts(){}

    static void text(Activity activity,String title,String hint,String initial,String positive,TextAction action){
        View content=LayoutInflater.from(activity).inflate(R.layout.dialog_text_input,null,false);
        TextInputLayout layout=content.findViewById(R.id.dialog_input_layout);
        TextInputEditText input=content.findViewById(R.id.dialog_input);
        layout.setHint(hint);
        input.setText(initial==null?"":initial);
        input.setSelectAllOnFocus(initial!=null&&!initial.isEmpty());

        androidx.appcompat.app.AlertDialog dialog=new MaterialAlertDialogBuilder(activity)
                .setTitle(title)
                .setView(content)
                .setNegativeButton("Cancel",null)
                .setPositiveButton(positive,null)
                .create();

        dialog.setOnShowListener(d->{
            Button ok=dialog.getButton(androidx.appcompat.app.AlertDialog.BUTTON_POSITIVE);
            ok.setOnClickListener(v->{
                String value=input.getText()==null?"":input.getText().toString().trim();
                if(value.isEmpty()){
                    layout.setError("Required");
                    return;
                }
                layout.setError(null);
                dialog.dismiss();
                if(action!=null)action.run(value);
            });
            input.requestFocus();
        });
        dialog.getWindow();
        dialog.show();
    }
}
