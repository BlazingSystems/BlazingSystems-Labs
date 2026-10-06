package com.blazefm.blazesystems;

import android.content.Context;

final class Ui {
    private Ui(){}

    static int dp(Context context,int value){
        return Math.round(value*context.getResources().getDisplayMetrics().density);
    }
}
