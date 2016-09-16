
package com.spreadtrum.android.eng;

import android.app.ProgressDialog;
import android.content.Context;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.Debug;
import android.os.Handler;
import android.os.Message;
import android.preference.CheckBoxPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceScreen;
import android.util.Log;

public class WifiCMCCTest extends PreferenceActivity {
    private static final boolean DEBUG = Debug.isDebug();
    private static final String TAG = "WifiCMCCTest";

    private static final boolean debug = true;

    private static final int ENABLE_PREF = 1;

    private static final int DISABLE_PREF = 2;

    private static final int DISMISS_DIALOG = 3;

    private static final int WIFI_ON_FAIL = 4;

    private static final int WIFI_OFF_FAIL = 5;

    private static final int CMCC_START = 6;

    private static final int CMCC_STOP = 7;

    private static final int TEST_ENABLE = 8;

    private static final int TEST_DISABLE = 9;

    private engfetch mEf;

    private boolean isRunning = false;

    private WifiManager wifiManager;

    private int outWifiState;

    private int waitTimes = 10;

    private CheckBoxPreference mAnritsuCheckbox;

    private CheckBoxPreference mNonAnritsuCheckbox;

    private CheckBoxPreference mRunTestCheckbox;

    private ProgressDialog mDialog;

    public boolean isAnritsu;

    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            CheckBoxPreference checkbox;
            int what = msg.what;
            if (isAnritsu) {
                checkbox = mAnritsuCheckbox;
            } else {
                checkbox = mNonAnritsuCheckbox;
            }
            switch (what) {
                case ENABLE_PREF:

                    break;
                case DISABLE_PREF:

                    break;
                case DISMISS_DIALOG:
                    if (mDialog != null) {
                        mDialog.dismiss();
                    }
                    break;
                case WIFI_ON_FAIL:
                    checkbox.setSummary("wifi open failed");
                    break;
                case WIFI_OFF_FAIL:
                    checkbox.setSummary("wifi close failed");
                    break;
                case CMCC_START:
                    checkbox.setSummary("");
                    break;
                case CMCC_STOP:
                    checkbox.setSummary("");
                    break;
                case TEST_ENABLE:
                    mRunTestCheckbox.setEnabled(true);
                    break;
                case TEST_DISABLE:
                    mRunTestCheckbox.setChecked(false);
                    mRunTestCheckbox.setEnabled(false);
                    break;

                default:
                    break;
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.layout.prefs_wifi_cmcc_test);
        mAnritsuCheckbox = (CheckBoxPreference) findPreference("anritsu_test");
        mNonAnritsuCheckbox = (CheckBoxPreference) findPreference("non_anritsu_test");
        mRunTestCheckbox = (CheckBoxPreference) findPreference("run_test");

        mAnritsuCheckbox.setSummary("");
        mNonAnritsuCheckbox.setSummary("");
        mRunTestCheckbox.setSummary("iwconfig when power off");
        mAnritsuCheckbox.setEnabled(false);
        mNonAnritsuCheckbox.setEnabled(false);
        mRunTestCheckbox.setEnabled(true);

        wifiManager = (WifiManager) getSystemService(Context.WIFI_SERVICE);
        outWifiState = wifiManager.getWifiState();
    }

    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen, Preference preference) {
        String key = preference.getKey();
/*
        if ("anritsu_test".equals(key)) {
            isAnritsu = true;
            CheckBoxPreference checkbox = (CheckBoxPreference) preference;
            if (checkbox.isChecked()) {
                radioCheckbox(key);
                mDialog = ProgressDialog.show(this, "wifi cmcc test", "opening,please wait!");
                new Thread(new Runnable() {
                    public void run() {
                        cmccStart();
                        mHandler.sendEmptyMessage(TEST_ENABLE);
                        // cmccTest();
                        isRunning = true;
                        mHandler.sendEmptyMessage(DISMISS_DIALOG);
                    }
                }).start();
            } else {
                releaseCheckbox();
                mDialog = ProgressDialog.show(this, "wifi cmcc test", "closing,please wait!");
                new Thread(new Runnable() {

                    @Override
                    public void run() {
                        cmccStop();
                        mHandler.sendEmptyMessage(TEST_DISABLE);
                        isRunning = false;
                        mHandler.sendEmptyMessage(DISMISS_DIALOG);
                    }
                }).start();
            }
        } else if ("non_anritsu_test".equals(key)) {
            CheckBoxPreference checkbox = (CheckBoxPreference) preference;
            isAnritsu = false;
            if (checkbox.isChecked()) {
                radioCheckbox(key);
                mDialog = ProgressDialog.show(this, "wifi cmcc test", "opening,please wait!");
                new Thread(new Runnable() {
                    public void run() {
                        cmccStart();
                        // cmccTest();
                        mHandler.sendEmptyMessage(TEST_ENABLE);
                        isRunning = true;
                        mHandler.sendEmptyMessage(DISMISS_DIALOG);
                    }
                }).start();
            } else {
                releaseCheckbox();
                mDialog = ProgressDialog.show(this, "wifi cmcc test", "closing,please wait!");
                new Thread(new Runnable() {

                    @Override
                    public void run() {
                        cmccStop();
                        mHandler.sendEmptyMessage(TEST_DISABLE);
                        isRunning = false;
                        mHandler.sendEmptyMessage(DISMISS_DIALOG);
                    }
                }).start();
            }
        } else if ("run_test".equals(key)) {
            CheckBoxPreference checkbox = (CheckBoxPreference) preference;
            if (checkbox.isChecked()) {
                cmccTest();
                checkbox.setEnabled(false);
            }
        }*/

        if ("run_test".equals(key)) {
	    if(preference instanceof CheckBoxPreference){
                CheckBoxPreference checkbox = (CheckBoxPreference) preference;
                if (checkbox.isChecked()) {
                    cmccTest();
                    checkbox.setEnabled(false);
                }
            }
        }
        return super.onPreferenceTreeClick(preferenceScreen, preference);
    }

    private void radioCheckbox(String prefsKey) {
        if ("anritsu_test".equals(prefsKey)) {
            mNonAnritsuCheckbox.setEnabled(false);
        } else {
            mAnritsuCheckbox.setEnabled(false);
        }
    }

    private void releaseCheckbox() {
        mNonAnritsuCheckbox.setEnabled(true);
        mAnritsuCheckbox.setEnabled(true);
    }

    @Override
    protected void onStop() {
        if (isRunning) {
            cmccStop();
        }

        if(DEBUG) Log.d(TAG, "outWifiState=" + outWifiState);
        if (outWifiState == WifiManager.WIFI_STATE_ENABLED
                || outWifiState == WifiManager.WIFI_STATE_ENABLING) {
            wifiManager.setWifiEnabled(true);
        } else {
            wifiManager.setWifiEnabled(false);
        }
        super.onStop();
    }

    private void cmccStop() {
        // // turn off wifi
        // wifiManager.setWifiEnabled(false);
        // // CMD stop
        // waitTimes = 10;
        // if (!waitForWifiOff()) {
        // return;
        // }
        writeCmd("CMCC STOP");
        mHandler.sendEmptyMessage(CMCC_STOP);
        if(DEBUG) Log.d(TAG, "xbin:CMCC STOP");

        // turn off wifi
        wifiManager.setWifiEnabled(false);
        waitTimes = 10;
        waitForWifiOff();

    }

    private void cmccStart() {
        // // turn off wifi
        // if (debug)
        // if(DEBUG) Log.d(TAG, "cmcc start");
        // wifiManager.setWifiEnabled(false);
        // // CMD start
        // waitTimes = 10;
        // if (!waitForWifiOff()) {
        // if (debug)
        // if(DEBUG) Log.d(TAG, "waitForWifiOff = false");
        // mHandler.sendEmptyMessage(WIFI_OFF_FAIL);
        // return;
        // }
        if (isAnritsu) {
            writeCmd("CMCC START");
        } else {
            writeCmd("NONANRITSU START");
        }
        if(DEBUG) Log.d(TAG, "xbin:CMCC START");
        // sleep 10s
        try {
            Thread.sleep(10000);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    private void cmccTest() {
        // // turn on wifi
        // wifiManager.setWifiEnabled(true);
        // // CMD Test
        // waitTimes = 10;
        // if (!waitForWifiOn()) {
        // if (debug)
        // if(DEBUG) Log.d(TAG, "waitForWifiOn = false");
        // mHandler.sendEmptyMessage(WIFI_ON_FAIL);
        // return;
        // }
        writeCmd("CMCC TEST");
        mHandler.sendEmptyMessage(CMCC_START);
        if(DEBUG) Log.d(TAG, "xbin:CMCC TEST");

    }

    private void writeCmd(String cmd) {

        if (mEf == null) {
            mEf = new engfetch();
        }
        mEf.writeCmd(cmd);

    }

    private boolean waitForWifiOn() {
        int wifiState;
        while (waitTimes-- > 0) {
            if(DEBUG) Log.d(TAG, "on waitTimes=" + waitTimes);
            wifiState = wifiManager.getWifiState();
            if(DEBUG) Log.d(TAG, " wait on wifiState=" + wifiState);
            if (wifiState == WifiManager.WIFI_STATE_ENABLED) {
                break;
            } else {
                try {
                    Thread.sleep(2000);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        }
        if (waitTimes > 0) {
            return true;
        } else {
            return false;
        }
    }

    private boolean waitForWifiOff() {
        int wifiState;
        while (waitTimes-- > 0) {
            if(DEBUG) Log.d(TAG, "off waitTimes=" + waitTimes);
            wifiState = wifiManager.getWifiState();
            if(DEBUG) Log.d(TAG, "off wifiState=" + wifiState);
            if (wifiState == WifiManager.WIFI_STATE_DISABLED) {
                break;
            } else {
                try {
                    Thread.sleep(2000);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        }

        if (waitTimes > 0) {
            return true;
        } else {
            return false;
        }

    }

}
