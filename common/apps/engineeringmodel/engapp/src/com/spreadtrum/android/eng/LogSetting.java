package com.spreadtrum.android.eng;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;

import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Bundle;
import android.os.Debug;
import android.os.SystemProperties;
import android.preference.CheckBoxPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceManager;
import android.preference.PreferenceScreen;
import android.util.Log;
import android.widget.Toast;

import android.app.Dialog;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;


public class LogSetting extends PreferenceActivity implements OnSharedPreferenceChangeListener, OnCancelListener{ 
    private static final boolean DEBUG = Debug.isDebug();
    private static final String LOG_TAG = "LogSetting";

    private static final int LOG_APP   = 0;
    private static final int LOG_MODEM = 1;
    private static final int LOG_DSP   = 2;
    private static final int LOG_MODEM_ARM = 3;
    private static final int LOG_ANDROID = 4;
    private static final int LOG_MODEM_SLOG = 5;
    private static final int LOG_IQ_LOG = 6;
   //modified by yingmin.piao for #Bug189515 kdump switch 20130715 -->
    private static final int LOG_KDUMP = 7;
    private static final int LOG_MANUAL_PANIC = 8;
    //<--

    private static final String PROPERTY_LOGCAT = "persist.sys.logstate";
    private static final String KEY_ANDROID_LOG = "android_log_enable";
    private static final String KEY_KDUMP = "kdump_enable";
    //modified by yingmin.piao for #Bug189515 kdump switch 20130715 -->
    private static final String KEY_MANUAL_PANIC = "manual_panic";
    private static final String KEY_DSP_LOG = "dsplog_enable";
    private static final String KEY_IQ_LOG= "iq_log_enable";
    private static final int DLG_MANUAL_PANIC = 1;
    //add for bug 252284
    private static final String KEY_MODEM_ARM_LOG= "modemlog_enable";
    private CheckBoxPreference modemARMPrefs;

    private CheckBoxPreference kdumpPrefs;
    //<--

    private CheckBoxPreference androidLogPrefs;
    private ListPreference DspPrefs;
    private CheckBoxPreference slogPreference;
    private CheckBoxPreference iqLogPrefs;

    private int mSocketID = 0;
    private engfetch mEf;
    private String   mATline;
    private String   mATResponse;
    private ByteArrayOutputStream outputBuffer;
    private DataOutputStream outputBufferStream;

    private int oldDSPValue = 0;

	/** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.layout.logsetting);

        if(DEBUG) Log.d(LOG_TAG, "logsetting activity onCreate.");
        androidLogPrefs = (CheckBoxPreference)findPreference(KEY_ANDROID_LOG);
		//modified by yingmin.piao for #Bug189515 kdump switch 20130715 -->
        kdumpPrefs =  (CheckBoxPreference)findPreference(KEY_KDUMP);
        //<--
        /*Add 20130530 spreadst of 171854 add dump iq checkbox start*/
        iqLogPrefs = (CheckBoxPreference)findPreference(KEY_IQ_LOG);
        /*Add 20130530 spreadst of 171854 add dump iq checkbox end*/
        DspPrefs = (ListPreference)findPreference(KEY_DSP_LOG);
        modemARMPrefs = (CheckBoxPreference)findPreference(KEY_MODEM_ARM_LOG); //add for bug 252284
	/* initilize modem communication */
    	mEf = new engfetch();
    	mSocketID = mEf.engopen();

	mATline = new String();

        // register preference change listener
        SharedPreferences defaultPrefs = PreferenceManager.getDefaultSharedPreferences(this);
        defaultPrefs.registerOnSharedPreferenceChangeListener(this);
        /*Add 20130311 Spreadst of 135491 remove slog item when the phone is not 77xx start*/
        slogPreference = (CheckBoxPreference)findPreference("modem_slog_enable");
        String mode = SystemProperties.get("ro.product.hardware");
        /*Modify 20130527 spreadst of 166285:close the modem log in user-version start*/
        String re = SystemProperties.get("persist.sys.modem_slog");
        if(re.isEmpty()&&SystemProperties.get("ro.build.type").equalsIgnoreCase("user")){
            re="0";
        }else if(re.isEmpty()&&SystemProperties.get("ro.build.type").equalsIgnoreCase("userdebug")) {
            re="1";
        }
        slogPreference.setChecked("1".equals(re));
        /*Modify 20130527 spreadst of 166285:close the modem log in user-version end*/
        if(mode==null){// || !mode.contains("77")){
            getPreferenceScreen().removePreference(slogPreference);
            if(DEBUG) Log.d(LOG_TAG, "remove the preference");
        }
        /*Add 20130311 Spreadst of 135491 remove slog item when the phone is not 77xx end*/
        /*Add 20130530 spreadst of 171854 add dump iq checkbox start*/
        iqLogPrefs.setChecked(LogSettingGetLogState(LOG_IQ_LOG)==1);
        /*Add 20130530 spreadst of 171854 add dump iq checkbox end*/

    }
    @Override
    protected void onStart() {
    if(DEBUG) Log.d(LOG_TAG, "logsetting activity onStart.");
        int androidLogState = LogSettingGetLogState(LOG_ANDROID);
        androidLogPrefs.setChecked(androidLogState == 1);
        //modified by yingmin.piao for #Bug189515 kdump switch 20130715 -->
        kdumpPrefs.setChecked(LogSettingGetLogState(LOG_KDUMP) == 1);
        //<--
        modemARMPrefs.setChecked(LogSettingGetLogState(LOG_MODEM) == 1);  //add for bug 252284
        oldDSPValue = LogSettingGetLogState(LOG_DSP);
        updataDSPOption(oldDSPValue);
        super.onStart();
    }

    private void updataDSPOption(int selectedId) {
        // TODO Auto-generated method stub
        if(DEBUG) Log.d(LOG_TAG, "updataDSPOption selectedId=["+selectedId+"]");
        /*Add 20130320 Spreadst of 139908 check the selected ID start */
        if(selectedId == -1){
            return;
        }
        /*Add 20130320 Spreadst of 139908 check the selected ID end */
        DspPrefs.setValueIndex(selectedId);
        DspPrefs.setSummary(DspPrefs.getEntry());
    }

	/**  Activity stop */
    	@Override
	protected void onDestroy() {
		mEf.engclose(mSocketID);
		super.onDestroy();
		if(DEBUG) Log.d(LOG_TAG, "logsetting activity onDestroy.");
	}

    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen, Preference preference) {
        String key = preference.getKey();
	if(DEBUG) Log.d(LOG_TAG, "[TreeCllik]onPreferenceTreeClick  key="+key);
        int logType = 0;

        if (key == null) {
            return false;
        }

        int newstate = 0;
        if(preference instanceof CheckBoxPreference){
            CheckBoxPreference CheckBox = (CheckBoxPreference) preference;
            newstate = CheckBox.isChecked() ? 1 : 0;
        }else if(preference instanceof ListPreference){
            return false;
        }

        if (key.equals("applog_enable")) {
            logType = LOG_APP;
        } else if (key.equals("modemlog_enable")) {
            logType = LOG_MODEM;
        } else if("modem_arm_log".equals(key)){
            logType = LOG_MODEM_ARM;
        } else if("android_log_enable".equals(key)){
            logType = LOG_ANDROID;
		 //modified by yingmin.piao for #Bug189515 kdump switch 20130715 -->
        }else if(KEY_KDUMP.equals(key)){
            logType = LOG_KDUMP;
        } else if(KEY_MANUAL_PANIC.equals(key)){
            logType = LOG_MANUAL_PANIC;
        //<--
        } else if("modem_slog_enable".equals(key)){
            logType = LOG_MODEM_SLOG;
        /*Add 20130530 spreadst of 171854 add dump iq checkbox start*/
        } else if ("iq_log_enable".equals(key)) {
            logType = LOG_IQ_LOG;
        /*Add 20130530 spreadst of 171854 add dump iq checkbox end*/
        }else {
            Log.e(LOG_TAG, "Unknown type!");
            return false;
        }

        int oldstate = LogSettingGetLogState(logType);
        if (oldstate < 0) {
            Log.e(LOG_TAG, "Invalid log state.");
            return false;
        }

        if (oldstate == newstate) {
            Toast.makeText(getApplicationContext(), "Replicated setting!", Toast.LENGTH_SHORT).show();
        } else {
            LogSettingSaveLogState(logType, newstate);

            String msg ;
            msg = String.format("Log state changed, new state:%d, old state:%d", newstate, oldstate);
            if(DEBUG) Log.d(LOG_TAG, msg);
        }

        return false;
    }

    private int LogSettingGetLogState(int logType) {
        int state = 1; // log enable default.

        switch (logType) {

            case LOG_APP:
                String property = new String();
                //property = System.getProperty(PROPERTY_LOGCAT);
                property = SystemProperties.get(PROPERTY_LOGCAT, "CCC");
                if (property != "CCC") {
                state = property.compareTo("disable");
                }
                else {
                Log.e(LOG_TAG, "logcat property no exist.");
                }
            break;

            case LOG_MODEM:
                {
                    outputBuffer = new ByteArrayOutputStream();
                    outputBufferStream = new DataOutputStream(outputBuffer);

                    try {
                        /*Modify 20130205 Spreadst of 125480 change the method of creating cmd start*/
                    Log.e(LOG_TAG, "Engmode socket open, id:" + mSocketID);
                    //mATline =String.format("%d,%d", engconstents.ENG_AT_GETARMLOG, 0);
                    mATline = new StringBuilder().append(engconstents.ENG_AT_GETARMLOG).append(",")
                              .append(0).toString();
                    /*Modify 20130205 Spreadst of 125480 change the method of creating cmd end*/

                    outputBufferStream.writeBytes(mATline);
                    } catch (IOException e) {
                    Log.e(LOG_TAG, "writeBytes() error!");
                    return -1;
                    } catch (java.lang.NumberFormatException nfe) {
                    Log.e(LOG_TAG, "at command return error");
                    return -1;
                    }
                    mEf.engwrite(mSocketID,outputBuffer.toByteArray(),outputBuffer.toByteArray().length);

                    int dataSize = 128;
                    byte[] inputBytes = new byte[dataSize];

                    int showlen= mEf.engread(mSocketID, inputBytes, dataSize);
                    mATResponse =  new String(inputBytes, 0, showlen);
					try {
						state = Integer.parseInt(mATResponse);
					} catch (Exception e) {
						Log.e(LOG_TAG, "NumberFormatException! : mATResponse = "
								+ mATResponse);
						return -1;
					}
                    if (state > 1) state = -1;
                }
            break;

            case LOG_DSP:
            {
                outputBuffer = new ByteArrayOutputStream();
                outputBufferStream = new DataOutputStream(outputBuffer);

                Log.e(LOG_TAG, "Engmode socket open, id:" + mSocketID);
                /*
                 * Modify 20130205 Spreadst of 125480 change the method of
                 * creating cmd start
                 */
                // mATline =String.format("%d,%d",
                // engconstents.ENG_AT_GETDSPLOG, 0);
                mATline = new StringBuilder().append(engconstents.ENG_AT_GETDSPLOG).append(",")
                        .append(0).toString();
                /*
                 * Modify 20130205 Spreadst of 125480 change the method of
                 * creating cmd end
                 */

                try {
                    outputBufferStream.writeBytes(mATline);
                } catch (IOException e) {
                    Log.e(LOG_TAG, "writeBytes() error!");
                    return -1;
                }
                mEf.engwrite(mSocketID, outputBuffer.toByteArray(),
                        outputBuffer.toByteArray().length);

                int dataSize = 128;
                byte[] inputBytes = new byte[dataSize];

                int showlen = mEf.engread(mSocketID, inputBytes, dataSize);
                mATResponse = new String(inputBytes, 0, showlen);
                /*Add 20130307 Spreadst of 134187 number format error start */
                try {
                    state = Integer.parseInt(mATResponse);
                } catch (NumberFormatException e) {
                    Log.e(LOG_TAG, "NumberFormatException! : mATResponse = " + mATResponse);
                    return -1;
                }
                /*Add 20130307 Spreadst of 134187 number format error end  */
                if (state > 2 || state < 0)
                    state = 0;
            }
        break;

        case LOG_MODEM_ARM:
	{
            String re = SystemProperties.get("persist.sys.cardlog", "0");
            try {
                state = Integer.parseInt(re);
            } catch (Exception e) {
                state = 0;
            }
        }
        break;

        case LOG_ANDROID:
	{
            String re = SystemProperties.get("init.svc.logs4android","");
            state = "running".equals(re)?1:0;
        }
        break;
       //modified by yingmin.piao for #Bug189515 kdump switch 20130715 -->
        case LOG_KDUMP:
	{
            String re = SystemProperties.get("persist.sys.kdump.enable", "0");
            try {
                state = Integer.parseInt(re);
            } catch (Exception e) {
                state = 0;
            }
        }
        break;
        case LOG_MANUAL_PANIC:
            state = 1;
        break;
        //<--
        case LOG_MODEM_SLOG: {
            /*Modify 20130527 spreadst of 166285:close the modem log in user-version start*/
            String re = SystemProperties.get("persist.sys.modem_slog");
            if(re.isEmpty()&&SystemProperties.get("ro.build.type").equalsIgnoreCase("user")){
                re="0";
            }else if(re.isEmpty()&&SystemProperties.get("ro.build.type").equalsIgnoreCase("userdebug")) {
                re="1";
            }
            /*Modify 20130527 spreadst of 166285:close the modem log in user-version end */
            try {
                state = Integer.parseInt(re);
            }catch(Exception e){
                state =0;
            }
            break ;
        }
        /*Add 20130530 spreadst of 171854 add dump iq checkbox start*/
        case LOG_IQ_LOG :{
            String re=SystemProperties.get("sys.iq_slog", "0");
            try{
                state= Integer.parseInt(re);
            }catch(Exception e){
                state=0;
            }
            break;
        }
        /*Add 20130530 spreadst of 171854 add dump iq checkbox end*/
        default:
            break;
        }

		return state;
	}

    private void LogSettingSaveLogState(int logType, int state) {
        switch (logType) {

            case LOG_APP:
            String property = (state == 1 ? "enable":"disable");

            SystemProperties.set(PROPERTY_LOGCAT, property);
            if(DEBUG) Log.d(LOG_TAG, "Set logcat property:" + property);
            break;

            case LOG_MODEM:
            {
                outputBuffer = new ByteArrayOutputStream();
                outputBufferStream = new DataOutputStream(outputBuffer);

                Log.e(LOG_TAG, "Engmode socket open, id:" + mSocketID);
                /*Modify 20130205 Spreadst of 125480 change the method of creating cmd start*/
                //mATline =String.format("%d,%d,%d", engconstents.ENG_AT_SETARMLOG,1, state);
                mATline = new StringBuilder().append(engconstents.ENG_AT_SETARMLOG).append(",")
                         .append(1).append(",").append(state).toString();
                /*Modify 20130205 Spreadst of 125480 change the method of creating cmd end*/

                try {
                    outputBufferStream.writeBytes(mATline);
                } catch (IOException e) {
                    Log.e(LOG_TAG, "writeBytes() error!");
                    return;
                }
                mEf.engwrite(mSocketID,outputBuffer.toByteArray(),outputBuffer.toByteArray().length);

                int dataSize = 128;
                byte[] inputBytes = new byte[dataSize];

                int showlen= mEf.engread(mSocketID, inputBytes, dataSize);
                mATResponse =  new String(inputBytes, 0, showlen);
                if(DEBUG) Log.d(LOG_TAG, "AT response:" + mATResponse);
                if (mATResponse.equals("OK"))
                    Toast.makeText(getApplicationContext(), "Success!",  Toast.LENGTH_SHORT).show();
                else
                    Toast.makeText(getApplicationContext(), "Fail!",  Toast.LENGTH_SHORT).show();
            }
            break;

            case LOG_DSP:
            {
                outputBuffer = new ByteArrayOutputStream();
                outputBufferStream = new DataOutputStream(outputBuffer);

                Log.e(LOG_TAG, "Engmode socket open, id:" + mSocketID);
                /*Modify 20130205 Spreadst of 125480 change the method of creating cmd start*/
                //mATline =String.format("%d,%d,%d", engconstents.ENG_AT_SETDSPLOG,1, state);
                mATline = new StringBuilder().append(engconstents.ENG_AT_SETDSPLOG).append(",")
                          .append(1).append(",").append(state).toString();
                /*Modify 20130205 Spreadst of 125480 change the method of creating cmd end*/

                try {
                    outputBufferStream.writeBytes(mATline);
                } catch (IOException e) {
                    Log.e(LOG_TAG, "writeBytes() error!");
                    return;
                }
                mEf.engwrite(mSocketID,outputBuffer.toByteArray(),outputBuffer.toByteArray().length);

                int dataSize = 128;
                byte[] inputBytes = new byte[dataSize];

                int showlen= mEf.engread(mSocketID, inputBytes, dataSize);
                mATResponse =  new String(inputBytes, 0, showlen);
                if(DEBUG) Log.d(LOG_TAG, "AT response:" + mATResponse);

                if (mATResponse.equals("OK")){
                    Toast.makeText(getApplicationContext(), "Success!",  Toast.LENGTH_SHORT).show();
                    updataDSPOption(state);
                    oldDSPValue = state;
                }else{
                    Toast.makeText(getApplicationContext(), "Fail!",  Toast.LENGTH_SHORT).show();
                    updataDSPOption(oldDSPValue);
                }
            }
            break;

            case LOG_MODEM_ARM:
                SystemProperties.set("persist.sys.cardlog", state == 1 ? "1" : "0");
            break;

            case LOG_ANDROID:
                if(state == 1){
                    SystemProperties.set("ctl.start", "logs4android");
                }else {
                    SystemProperties.set("ctl.stop", "logs4android");
                }
            break;
            //modified by yingmin.piao for #Bug189515 kdump switch 20130715 -->
            case LOG_KDUMP:
                if(state == 1){
                    SystemProperties.set("persist.sys.kdump.enable", "1");
                }else {
                    SystemProperties.set("persist.sys.kdump.enable", "0");
                }
            break;
            case LOG_MANUAL_PANIC:
                doManualPanic();
            break;
            //<--
            case LOG_MODEM_SLOG: {
                SystemProperties.set("persist.sys.modem_slog",String.valueOf(state));
                outputBuffer = new ByteArrayOutputStream();
                outputBufferStream = new DataOutputStream(outputBuffer);
                Log.e(LOG_TAG, "Engmode socket open, id:" + mSocketID);
                /*Modify 20130205 Spreadst of 125480 change the method of creating cmd start*/
                if (state == 1) {
                  // mATline = String.format("%d,%d,%s",engconstents.ENG_AT_NOHANDLE_CMD, 1, "AT+SLOG=2");
                    mATline = new StringBuilder().append(engconstents.ENG_AT_NOHANDLE_CMD).append(",")
                             .append(1).append(",").append("AT+SLOG=2").toString();
                }else {
                  //  mATline = String.format("%d,%d,%s",engconstents.ENG_AT_NOHANDLE_CMD, 1, "AT+SLOG=3");
                    mATline = new StringBuilder().append(engconstents.ENG_AT_NOHANDLE_CMD).append(",")
                             .append(1).append(",").append("AT+SLOG=3").toString();
                 /*Modify 20130205 Spreadst of 125480 change the method of creating cmd end*/
                }
                Log.e(LOG_TAG, "cmd" + mATline);
                try{
                    outputBufferStream.writeBytes(mATline);
                }catch (IOException e) {
                    Log.e(LOG_TAG, "writeBytes() error!");
                    return ;
                }
                mEf.engwrite(mSocketID, outputBuffer.toByteArray(),
                        outputBuffer.toByteArray().length);

                int dataSize = 128;
                byte[] inputBytes = new byte[dataSize];
                int showlen = mEf.engread(mSocketID, inputBytes, dataSize);
                mATResponse = new String(inputBytes, 0, showlen);
                if(DEBUG) Log.d(LOG_TAG, "AT response:" + mATResponse);
                if (mATResponse.contains("OK")) {
                    Toast.makeText(getApplicationContext(), "Success!",
                            Toast.LENGTH_SHORT).show();
                }else {
                    Toast.makeText(getApplicationContext(), "Fail!",
                            Toast.LENGTH_SHORT).show();
                }
                break;
            }
            /*Add 20130530 spreadst of 171854 add dump iq checkbox start*/
            case LOG_IQ_LOG :{
                outputBuffer = new ByteArrayOutputStream();
                outputBufferStream = new DataOutputStream(outputBuffer);
                if(DEBUG) Log.d(LOG_TAG, "Engmode socket open, id:" + mSocketID);
                /*Modify 20130205 Spreadst of 125480 change the method of creating cmd start*/
                if (state == 1) {
                  // mATline = String.format("%d,%d,%s",engconstents.ENG_AT_NOHANDLE_CMD, 1, "AT+SLOG=2");
                    mATline = new StringBuilder().append(engconstents.ENG_AT_NOHANDLE_CMD).append(",")
                             .append(1).append(",").append("AT+SPDSP = 1,1,0,0").toString();
                }else {
                  //  mATline = String.format("%d,%d,%s",engconstents.ENG_AT_NOHANDLE_CMD, 1, "AT+SLOG=3");
                    mATline = new StringBuilder().append(engconstents.ENG_AT_NOHANDLE_CMD).append(",")
                             .append(1).append(",").append("AT+SPDSP = 1,0,0,0").toString();
                 /*Modify 20130205 Spreadst of 125480 change the method of creating cmd end*/
                }
                if(DEBUG) Log.d(LOG_TAG, "cmd" + mATline);
                try{
                    outputBufferStream.writeBytes(mATline);
                }catch (IOException e) {
                    Log.e(LOG_TAG, "writeBytes() error!");
                    return ;
                }
                mEf.engwrite(mSocketID, outputBuffer.toByteArray(),
                        outputBuffer.toByteArray().length);

                int dataSize = 128;
                byte[] inputBytes = new byte[dataSize];
                int showlen = mEf.engread(mSocketID, inputBytes, dataSize);
                mATResponse = new String(inputBytes, 0, showlen);
                if(DEBUG) Log.d(LOG_TAG, "AT response:" + mATResponse);
                if (mATResponse.contains("OK")) {
                    Toast.makeText(getApplicationContext(), "Success!",
                            Toast.LENGTH_SHORT).show();
                    SystemProperties.set("sys.iq_slog",String.valueOf(state));
                }else {
                    Toast.makeText(getApplicationContext(), "Fail!",
                            Toast.LENGTH_SHORT).show();
                }
                break;
            }
            /*Add 20130530 spreadst of 171854 add dump iq checkbox end*/
            default:
            break;
        }
    }
    //modified by yingmin.piao for #Bug189515 kdump switch 20130715 -->
    private void doManualPanic() {
        removeDialog(DLG_MANUAL_PANIC);
        showDialog(DLG_MANUAL_PANIC);
    }

    @Override
    public Dialog onCreateDialog(int id, Bundle args) {
        switch (id) {
        case DLG_MANUAL_PANIC:
        return new AlertDialog.Builder(this)
            .setTitle(R.string.manual_panic_title)
            .setPositiveButton(R.string.dlg_ok, new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int which) {
                    SystemProperties.set("sys.manual.panic", "1");
                }})
            .setNegativeButton(R.string.dlg_cancel, null)
            .setMessage(R.string.manual_panic_message)
            .setOnCancelListener(this)
            .create();
        }
        return null;
    }

    @Override
    public void onCancel(DialogInterface dialog) {
        finish();
    }
    //<--

    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
        if(key.equals("modem_arm_log")){
            String re = sharedPreferences.getString(key, "");
            if(DEBUG) Log.d(LOG_TAG, "onSharedPreferenceChanged key="+key+" value="+re);
            LogSettingSaveLogState(LOG_MODEM_ARM,Integer.parseInt(re));
        }else if(key.equals(KEY_DSP_LOG)){
            String re = sharedPreferences.getString(key, "");
            if(DEBUG) Log.d(LOG_TAG, "onSharedPreferenceChanged key="+key+" value="+re);
            if(Integer.parseInt(re) != oldDSPValue)
            LogSettingSaveLogState(LOG_DSP,Integer.parseInt(re));
        }

    }

}
