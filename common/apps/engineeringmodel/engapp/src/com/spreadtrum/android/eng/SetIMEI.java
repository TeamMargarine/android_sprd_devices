package com.spreadtrum.android.eng;

import android.app.Activity;
import android.telephony.TelephonyManager;
import android.util.Log;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;
import android.view.View.OnClickListener;
import android.os.Bundle;
import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import android.os.Debug;


import android.view.View;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import com.android.internal.telephony.PhoneFactory;

public class SetIMEI extends Activity implements OnClickListener {
    private static final boolean DEBUG = Debug.isDebug();
    private static final String LOG_TAG = "engineeringmodel";
    private EditText mIMEIEdit, mIMEIEdit2;

    private int[] mSocketIDs = new int[2];
    private engfetch mEf;
    private String mATline = null;
    private String mATResponse;
    private String mIMEIInput = null, mIMEIInput2 = null;

    private ByteArrayOutputStream outputBuffer;
    private DataOutputStream outputBufferStream;
    private static final int IMEI_LENGTH = 15;
    private static final int ENG_AT_REQUEST_IMEI = 1;
    private static final int ENG_AT_NOHANDLE_CMD = 39;
    private TelephonyManager telMgr;
    private int phoneCount = 1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.setimei);
        mIMEIEdit = (EditText) findViewById(R.id.imei_edit);
        mIMEIEdit.setText("");
        mIMEIEdit2 = (EditText) findViewById(R.id.imei_edit2);
        mIMEIEdit2.setText("");
        phoneCount = PhoneFactory.getPhoneCount();

        mEf = new engfetch();
        mSocketIDs[0] = mEf.engopen(0);
        mSocketIDs[1] = mEf.engopen(1);
        mReadIMEI.start();
    }

    Handler mHandler = new Handler() {

        @Override
        public void handleMessage(Message msg) {
            if (msg.what == 0 && mIMEIInput != null) {
                mIMEIEdit.setText(mIMEIInput);
            } else if (msg.what == 1 && mIMEIInput2 != null) {
                mIMEIEdit2.setText(mIMEIInput2);
            } else if (msg.what == 2) {
                hideSim2();
            }
        }
    };

    Thread mReadIMEI = new Thread() {

        @Override
        public void run() {
            mIMEIInput = readIMEI(0);
            mHandler.sendEmptyMessage(0);
            if (phoneCount > 1) {
                mIMEIInput2 = readIMEI(1);
                mHandler.sendEmptyMessage(1);
            } else {
                mHandler.sendEmptyMessage(2);
            }
        }

    };

    private void hideSim2() {
        mIMEIEdit2.setVisibility(View.GONE);
        findViewById(R.id.wbutton2).setVisibility(View.GONE);
        findViewById(R.id.rbutton2).setVisibility(View.GONE);
        findViewById(R.id.sim2).setVisibility(View.GONE);
    }

    @Override
    public void onClick(View v) {
        if (v.getId() == R.id.wbutton) {
            if (checkInvalid(0)) {
                String imei = mIMEIEdit.getText().toString();
                if (writeIMEI(imei, 0)) {
                    showToast("success!");
                } else {
                    showToast("failed!");
                }
            }
        } else if (v.getId() == R.id.rbutton) {
            String imei = readIMEI(0);
            if (imei != null) {
                mIMEIEdit.setText(imei);
            } else {
                showToast("read IMEI1 failed!");
            }
        }
        if (v.getId() == R.id.wbutton2) {
            if (checkInvalid(1)) {
                String imei = mIMEIEdit2.getText().toString();
                if (writeIMEI(imei, 1)) {
                    showToast("success!");
                } else {
                    showToast("failed!");
                }
            }
        } else if (v.getId() == R.id.rbutton2) {
            String imei = readIMEI(1);
            if (imei != null) {
                mIMEIEdit2.setText(imei);
            } else {
                showToast("read IMEI2 failed!");
            }
        }
    }

    private String readIMEI(int i) {
        outputBuffer = new ByteArrayOutputStream();
        outputBufferStream = new DataOutputStream(outputBuffer);

        Log.d(LOG_TAG, "Engmode socket open, id:" + mSocketIDs[i]);
        /*Modify 20130205 Spreadst of 125480 change the method of creating cmd start*/
        //mATline =String.format("%d,%d", engconstents.ENG_AT_REQUEST_IMEI, 0);
        mATline =new StringBuilder().append(engconstents.ENG_AT_REQUEST_IMEI).append(",")
                 .append(0).toString();
        /*Modify 20130205 Spreadst of 125480 change the method of creating cmd end*/

        try {
            outputBufferStream.writeBytes(mATline);
        } catch (IOException e) {
            Log.e(LOG_TAG, "writeBytes() error!");
            return null;
        }
        mEf.engwrite(mSocketIDs[i], outputBuffer.toByteArray(),
                outputBuffer.toByteArray().length);

        int dataSize = 128;
        byte[] inputBytes = new byte[dataSize];

        int showlen = mEf.engread(mSocketIDs[i], inputBytes, dataSize);
        mATResponse = new String(inputBytes, 0, showlen);
        if(DEBUG) Log.d(LOG_TAG, "Read IMEI: " + i + mATResponse);
        if (!mATResponse.equals("Error")) {
            return mATResponse;
        } else
            return null;
    }

    private boolean checkInvalid(int i) {
        String imei = null;
        if (i == 0) {
            imei = mIMEIEdit.getText().toString();
        } else if (i == 1) {
            imei = mIMEIEdit2.getText().toString();
        }
        if (imei == null || imei.equals("")) {
            showToast("empty input!");
            return false;
        }
        if (imei.trim().length() != IMEI_LENGTH) {
            showToast("must be 15 digits!");
            return false;
        }
        return true;
    }

    private boolean writeIMEI(String imei, int id) {
        outputBuffer = new ByteArrayOutputStream();
        outputBufferStream = new DataOutputStream(outputBuffer);

        if(DEBUG) Log.e(LOG_TAG, "Engmode socket open, id:" + mSocketIDs[id]);
        mATline = String.format("%d,%d,%s", ENG_AT_NOHANDLE_CMD, 1,
                "AT+SPIMEI=\"" + imei + "\"");
        try {
            outputBufferStream.writeBytes(mATline);
        } catch (IOException e) {
            return false;
        }
        mEf.engwrite(mSocketIDs[id], outputBuffer.toByteArray(),
                outputBuffer.toByteArray().length);

        int dataSize = 256;
        byte[] inputBytes = new byte[dataSize];

        int showlen = mEf.engread(mSocketIDs[id], inputBytes, dataSize);
        mATResponse = new String(inputBytes, 0, showlen);
        // Toast.makeText(this, "é¤ˆî????" + mATResponse, Toast.LENGTH_LONG).show();
        if (mATResponse.equals("Error")) {
            return false;
        } else
            return true;
    }

    @Override
    protected void onDestroy() {
        for (int i = 0; i < 2; i++) {
            mEf.engclose(mSocketIDs[i]);
        }
        super.onDestroy();
    }

    private void showToast(String msg) {
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show();
    }

}



