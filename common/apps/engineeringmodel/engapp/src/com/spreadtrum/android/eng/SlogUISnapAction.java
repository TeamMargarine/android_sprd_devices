package com.spreadtrum.android.eng;

import com.spreadtrum.android.eng.R;

import android.app.Activity;
import android.os.Bundle;
import android.widget.Toast;

/* SlogUI Added by Yuntao.xiao*/

public class SlogUISnapAction extends Activity {
    private static final String TAG = "SlogUISnapAction";
    @Override
    protected void onCreate(Bundle snap) {
        super.onCreate(snap);
        //if (true)
        try {
            SlogAction.snap(this);
        } catch (ExceptionInInitializerError error) {
            // TODO: NEED IMPROVE. If the activity have not started, we can't
            // send message to the main thread of application, because it have
            // not init yet.
            // The sender must be fixed.
            android.util.Log.e(TAG, "Illegal state because the activity was uninitialized. Need improve");
        }
        finish();
        //
        //setTheme()
    }
}
