package com.spreadtrum.android.eng;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.AsyncTask;
import android.os.Debug;
import android.os.IBinder;
import android.os.ServiceManager;
import android.util.Log;
import android.content.SharedPreferences;
import android.os.SystemProperties;
import android.preference.PreferenceManager;
import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.nio.charset.Charset;
import java.util.Locale;



public class SetSlogService extends Service{
    private static final boolean DEBUG = Debug.isDebug();
    private final String TAG = "SetSlogService";
 
    private int mSocketID = 0;
    private engfetch mEf;
    private String mATline;
    private String mATResponse; 
    private setSlogAsyncTask msetSlogAsyncTask;
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onCreate(){
        super.onCreate();
    }

    @Override
    public void onStart(Intent intent,int startId) {
        super.onStart(intent, startId);
        
        msetSlogAsyncTask = new setSlogAsyncTask();
        msetSlogAsyncTask.execute();
        
     }

    @Override
    public void onDestroy() {
        super.onDestroy();         
    }
    
    
    
    public class setSlogAsyncTask extends AsyncTask<Void, Void, Void>{       
        @Override
        protected Void doInBackground(Void... arg0) {
            // TODO Auto-generated method stub
            //138559
            while(!atSlog()){
                try {
                    Thread.sleep(1000);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }            
            return null;
        }
    
    }
    
    
     private boolean atSlog() {
               
        mEf = new engfetch();
        mSocketID = mEf.engopen();
        mATline = new String();
        String re = SystemProperties.get("persist.sys.modem_slog");
        if(re.isEmpty()&&SystemProperties.get("ro.build.type").equalsIgnoreCase("user")){
            re="0";
        }else if(re.isEmpty()&&SystemProperties.get("ro.build.type").equalsIgnoreCase("userdebug")) {
            re="1";
        }
        int state = Integer.parseInt(re);

        ByteArrayOutputStream outputBuffer = new ByteArrayOutputStream();
        DataOutputStream outputBufferStream = new DataOutputStream(outputBuffer);
        /*Modify 20130527 spreadst of 166285:close the modem log in user-version start*/
        if(DEBUG) Log.d(TAG, "Engmode socket open, id:" + mSocketID);
        if (state == 1) {
            SystemProperties.set("persist.sys.modem_slog", "1");
            mATline = String.format("%d,%d,%s", engconstents.ENG_AT_NOHANDLE_CMD, 1, "AT+SLOG=2");
            if(DEBUG) Log.d(TAG, "set persist.sys.modem_slog 1");
        } else {
            SystemProperties.set("persist.sys.modem_slog", "0");
            mATline = String.format("%d,%d,%s", engconstents.ENG_AT_NOHANDLE_CMD, 1, "AT+SLOG=3");
            //add for bug 252284
            setModemARMLog(false);  //turn off modem-arm log to transfer on user-version
            if(DEBUG) Log.d(TAG, "set persist.sys.modem_slog 0");
        }
        /*Modify 20130527 spreadst of 166285:close the modem log in user-version end*/
        try {
            outputBufferStream.writeBytes(mATline);
        } catch (IOException e) {
            Log.e(TAG, "writeBytes() error!");
            return false;
        }
        mEf.engwrite(mSocketID, outputBuffer.toByteArray(), outputBuffer.toByteArray().length);
        int dataSize = 128;
        byte[] inputBytes = new byte[dataSize];
        int showlen = mEf.engread(mSocketID, inputBytes, dataSize);
        mATResponse = new String(inputBytes, 0, showlen);
        
        mEf.engclose(mSocketID);
        
        if(DEBUG) Log.d(TAG, "AT response:" + mATResponse);
        if (mATResponse.contains("OK")) {
            return true;
        }
        /* Modify 20130311 Spreadst of 127737 start the slog when boot start */
//        if (mATResponse.contains("OK")) {
//            Toast.makeText(context, "Success!", Toast.LENGTH_SHORT).show();
//        } else {
//            Toast.makeText(context, "Fail!", Toast.LENGTH_SHORT).show();
//        }        
        return false;
        /* Modify 20130311 Spreadst of 127737 start the slog when boot end  */
    }
    /**
    * Send AT cmd to open/close modem arm log
    *
    * @param status The status need to change
    * add for bug 252284
    * */
    private void setModemARMLog(boolean status){
        ByteArrayOutputStream outputBuffer = new ByteArrayOutputStream();
        DataOutputStream outputBufferStream = new DataOutputStream(outputBuffer);
        String mCmd = String.format(Locale.US, "%d,%d,%s", engconstents.ENG_AT_NOHANDLE_CMD, 1,
                "AT+ARMLOG=" + (status == true ? "1" : "0"));
        try {
            outputBufferStream.writeBytes(mCmd);
        } catch (IOException e) {
            if(DEBUG) Log.e(TAG, "atModemARMLog writebytes error: " + e);
            return;
        }
        mEf.engwrite(mSocketID,outputBuffer.toByteArray(),outputBuffer.toByteArray().length);
        int dataSize = 128;
        byte[] inputBytes = new byte[dataSize];
        int showlen = mEf.engread(mSocketID, inputBytes, dataSize);
        String readATResponse = new String(inputBytes, 0 ,showlen, Charset.defaultCharset());
        if(DEBUG) Log.d(TAG, "atModemARMLog write " + readATResponse);
        try {
            if (outputBuffer != null) {
                outputBuffer.close();
                outputBuffer = null;
            }
            if (outputBufferStream != null) {
                outputBufferStream.close();
                outputBufferStream = null;
            }
        } catch (Exception e) {
        }
    }
}
