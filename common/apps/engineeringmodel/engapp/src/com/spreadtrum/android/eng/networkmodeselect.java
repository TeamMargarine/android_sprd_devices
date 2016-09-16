package com.spreadtrum.android.eng;

import android.os.AsyncResult;
import android.os.Bundle;
import android.os.Debug;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.telephony.TelephonyManager;
import android.util.Log;

import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneFactory;

public class networkmodeselect extends PreferenceActivity
implements Preference.OnPreferenceChangeListener{
    private static final boolean DEBUG = Debug.isDebug();
    private static final String LOG_TAG = "networkmodeselect";
    private static final boolean DBG = true;
    private static final String KEY_NETW = "preferred_network_mode_key";
    static final int preferredNetworkMode = Phone.PREFERRED_NT_MODE;
    private int valueofsms = 0;
    private int mModemType = 0;
//    private Phone mPhone;
    private Phone mPhone;
    private MyHandler mHandler;
    private ListPreference mButtonPreferredNetworkMode;

    private boolean mNetworkModereferenceEnabled = true;
    protected static final int NETWORKMODE_MESSAGE_WAIT_LONG_TIME = 3000; // time (msec)
    protected static final int NETWORKMODE_MESSAGE_WAIT_SHORT_TIME = 1000; // time (msec)
    static final int PREFERRED_NT_MODE_SEARCHING_STATE = 0xFF;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
//    addPreferencesFromResource(R.layout.networkselect);
        mModemType = TelephonyManager.getDefault().getModemType();
        log("modem type: " + mModemType);
        if ((mModemType == TelephonyManager.MODEM_TYPE_TDSCDMA)
            || (mModemType == TelephonyManager.MODEM_TYPE_WCDMA)) {
            addPreferencesFromResource(R.layout.networkselect);
        } else {
            addPreferencesFromResource(R.layout.networkselect_gsm_only);
        }
        mButtonPreferredNetworkMode = (ListPreference) findPreference(KEY_NETW);
        mButtonPreferredNetworkMode.setOnPreferenceChangeListener(this);
        if (mModemType == TelephonyManager.MODEM_TYPE_WCDMA) {
            mButtonPreferredNetworkMode.setEntries(R.array.preferred_network_mode_choices_wcdma);
        }

        Looper looper;
        looper = Looper.myLooper();
        mHandler = new MyHandler(looper);

        //mPhone = new Phone[2];
        //phone = PhoneFactory.getDefaultPhone();
        boolean isCardReady = PhoneFactory.isCardReady(0);
        if (isCardReady) {
            mPhone = PhoneFactory.getPhone(0);
        } else {
            mPhone = null;
            mNetworkModereferenceEnabled = false;
            mButtonPreferredNetworkMode.setEnabled(false);
        }

        if(mPhone != null) {
            mPhone.getPreferredNetworkType(mHandler
                 .obtainMessage(MyHandler.MESSAGE_GET_PREFERRED_NETWORK_TYPE));
        }
    }

    private class MyHandler extends Handler {

        private static final int MESSAGE_GET_PREFERRED_NETWORK_TYPE = 0;
        private static final int MESSAGE_SET_PREFERRED_NETWORK_TYPE = 1;
        private static final int MESSAGE_RELOAD_PREFERRED_NETWORK_TYPE = 2;
        private static final int MESSAGE_REQUEST_GET_PREFERRED_NETWORK_TYPE = 3;
        private static final int MESSAGE_REQUEST_SET_PREFERRED_NETWORK_TYPE = 4;
        public MyHandler(Looper looper) {
            super(looper);
        }
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MESSAGE_GET_PREFERRED_NETWORK_TYPE:
                    handleGetPreferredNetworkTypeResponse(msg);
                    break;

                case MESSAGE_SET_PREFERRED_NETWORK_TYPE:
                    handleSetPreferredNetworkTypeResponse(msg);
                    break;

                case MESSAGE_RELOAD_PREFERRED_NETWORK_TYPE:
                    handleReloadPreferredNetworkTypeResponse(msg);
                    break;

                case MESSAGE_REQUEST_GET_PREFERRED_NETWORK_TYPE:
                    requestGetPreferredNetworkType();
                    break;

                case MESSAGE_REQUEST_SET_PREFERRED_NETWORK_TYPE:
                    requestSetPreferredNetworkType();
                    break;
            }
        }

        private void handleGetPreferredNetworkTypeResponse(Message msg) {
            AsyncResult ar = (AsyncResult) msg.obj;
            if (ar.exception == null) {
                int modemNetworkMode = ((int[])ar.result)[0];

                if (DBG) {
                    log ("handleGetPreferredNetworkTypeResponse: modemNetworkMode = " + modemNetworkMode);
                }

                int settingsNetworkMode = 0;
                if(mPhone != null) {
                    settingsNetworkMode = android.provider.Settings.Secure.getInt(
                        mPhone.getContext().getContentResolver(),
                        android.provider.Settings.Secure.PREFERRED_NETWORK_MODE,
                        preferredNetworkMode);
                }

                if (DBG) {
                    log("handleGetPreferredNetworkTypeReponse: settingsNetworkMode = " +
                        settingsNetworkMode);
                    log ("handleGetPreferredNetworkTypeResponse: modemNetworkMode = " +
                        modemNetworkMode);
                }

                if (modemNetworkMode != settingsNetworkMode) {
                    UpdatePreferredNetworkModeSummary(PREFERRED_NT_MODE_SEARCHING_STATE);
                    if (DBG) {
                        log ("handleGetPreferredNetworkTypeResponse: settingsNetworkMode not the same ");
                        log ("sendEmptyMessageDelayed MESSAGE_REQUEST_SET_PREFERRED_NETWORK_TYPE: sleep NETWORKMODE_MESSAGE_WAIT_SHORT_TIME");
                    }
                    // We should set templory
                    mButtonPreferredNetworkMode.setEnabled(false);
                    mNetworkModereferenceEnabled = false;
                    mHandler.sendEmptyMessageDelayed(MyHandler.MESSAGE_REQUEST_SET_PREFERRED_NETWORK_TYPE, NETWORKMODE_MESSAGE_WAIT_SHORT_TIME);
                    return;
                } else {
                    if (DBG) {
                        log ("handleGetPreferredNetworkTypeResponse: mNetworkModereferenceEnabled = " + mNetworkModereferenceEnabled );
                    }
                    mNetworkModereferenceEnabled = true;
                }

				if (modemNetworkMode == Phone.PREFERRED_NT_MODE ||
					modemNetworkMode == Phone.NT_MODE_GSM_ONLY ||
					modemNetworkMode == Phone.NT_MODE_WCDMA_ONLY ) {
					if (DBG) {
						log("handleGetPreferredNetworkTypeResponse: if 1: modemNetworkMode = " +
							modemNetworkMode);
					}

					//check changes in modemNetworkMode and updates settingsNetworkMode
					if (modemNetworkMode != settingsNetworkMode) {
						if (DBG) {
							log("handleGetPreferredNetworkTypeResponse: if 2: " +
										"modemNetworkMode != settingsNetworkMode");
						}

						settingsNetworkMode = modemNetworkMode;

						if (DBG) { log("handleGetPreferredNetworkTypeResponse: if 2: " +
									"settingsNetworkMode = " + settingsNetworkMode);
						}

						//changes the Settings.System accordingly to modemNetworkMode
						if(mPhone != null) {
							android.provider.Settings.Secure.putInt(
								mPhone.getContext().getContentResolver(),
								android.provider.Settings.Secure.PREFERRED_NETWORK_MODE,
								settingsNetworkMode );
                        }
                    }
                    if (DBG) { log (" 4 "); }
                    UpdatePreferredNetworkModeSummary(modemNetworkMode);
                    // changes the mButtonPreferredNetworkMode accordingly to modemNetworkMode
                    mButtonPreferredNetworkMode.setValue(Integer.toString(modemNetworkMode));
                } else {
                    if (DBG) log("handleGetPreferredNetworkTypeResponse: else: reset to default");
                    resetNetworkModeToDefault();
                }
            }
        }

        private void handleResetPreferredNetworkTypeResponse() {

                int settingsNetworkMode = 0;
                if(mPhone != null) {
                    settingsNetworkMode = android.provider.Settings.Secure.getInt(
                        mPhone.getContext().getContentResolver(),
                        android.provider.Settings.Secure.PREFERRED_NETWORK_MODE,
                        preferredNetworkMode);
                }

                if(mPhone != null) {
                    android.provider.Settings.Secure.putInt(mPhone.getContext().getContentResolver(),
                    android.provider.Settings.Secure.PREFERRED_NETWORK_MODE,
                    settingsNetworkMode );
                }
                if (DBG) {
                    log ("handleResetPreferredNetworkTypeResponse: send PREFERRED_NETWORK_MODE");
                }

        }

        private void handleSetPreferredNetworkTypeResponse(Message msg) {
            AsyncResult ar = (AsyncResult) msg.obj;
            if (ar.exception == null) {
                int networkMode = Integer.valueOf(
                    mButtonPreferredNetworkMode.getValue()).intValue();
                if(mPhone != null) {
                    android.provider.Settings.Secure.putInt(mPhone.getContext().getContentResolver(),
                    android.provider.Settings.Secure.PREFERRED_NETWORK_MODE,
                    networkMode );
                }
                // dobule check again if the value is correct or not
                if(mPhone != null) {
                    mPhone.getPreferredNetworkType(mHandler
                         .obtainMessage(MyHandler.MESSAGE_RELOAD_PREFERRED_NETWORK_TYPE));
                }

            } else {
                if(mPhone != null) {
                    mPhone.getPreferredNetworkType(obtainMessage(MESSAGE_GET_PREFERRED_NETWORK_TYPE));
                }
            }
        }


        private void handleReloadPreferredNetworkTypeResponse(Message msg) {
            AsyncResult ar = (AsyncResult) msg.obj;
            if (ar.exception == null) {
                int modemNetworkMode = ((int[])ar.result)[0];

                if (DBG) {
                    log ("handleReloadPreferredNetworkTypeResponse: modemNetworkMode = " + modemNetworkMode);
                }

                int settingsNetworkMode = 0;
                if(mPhone != null) {
                    settingsNetworkMode = android.provider.Settings.Secure.getInt(
                        mPhone.getContext().getContentResolver(),
                        android.provider.Settings.Secure.PREFERRED_NETWORK_MODE,
                        preferredNetworkMode);
                }

                if (DBG) {
                    log("handleReloadPreferredNetworkTypeResponse: settingsNetworkMode = " +
                        settingsNetworkMode);
                }

                if (modemNetworkMode != settingsNetworkMode) {
                    UpdatePreferredNetworkModeSummary(PREFERRED_NT_MODE_SEARCHING_STATE);
                    if (DBG) {
                        log ("handleReloadPreferredNetworkTypeResponse: settingsNetworkMode not the same ");
                        log ("sendEmptyMessageDelayed MESSAGE_SET_PREFERRED_NETWORK_TYPE: sleep NETWORKMODE_MESSAGE_WAIT_LONG_TIME");
                    }
                    // We should set templory
                    if (mNetworkModereferenceEnabled == false) {
                        mButtonPreferredNetworkMode.setEnabled(false);
                    } else {
                        mButtonPreferredNetworkMode.setEnabled(true);
                    }
                    mHandler.sendEmptyMessageDelayed(MyHandler.MESSAGE_REQUEST_SET_PREFERRED_NETWORK_TYPE, NETWORKMODE_MESSAGE_WAIT_LONG_TIME);

                    return;
                } else {
                    if (DBG) {
                        log ("handleReloadPreferredNetworkTypeResponse: mNetworkModereferenceEnabled = " + mNetworkModereferenceEnabled );
                    }
                    mNetworkModereferenceEnabled = true;
                    UpdatePreferredNetworkModeSummary(modemNetworkMode);
                    mButtonPreferredNetworkMode.setEnabled(true);
                }
            }
        }

        private void requestGetPreferredNetworkType() {
            if(mPhone != null) {
                mPhone.getPreferredNetworkType(mHandler
                     .obtainMessage(MyHandler.MESSAGE_GET_PREFERRED_NETWORK_TYPE));
            }
        }

        private void requestSetPreferredNetworkType() {
            int settingsNetworkMode = 0;
            if(mPhone != null) {
                settingsNetworkMode = android.provider.Settings.Secure.getInt(
                    mPhone.getContext().getContentResolver(),
                    android.provider.Settings.Secure.PREFERRED_NETWORK_MODE,
                    preferredNetworkMode);
            }

            if(mPhone != null) {
                mPhone.setPreferredNetworkType(settingsNetworkMode,
                        this.obtainMessage(MyHandler.MESSAGE_SET_PREFERRED_NETWORK_TYPE));
            }
        }

        private void resetNetworkModeToDefault() {
            //set the mButtonPreferredNetworkMode
            mButtonPreferredNetworkMode.setValue(Integer.toString(preferredNetworkMode));
            //set the Settings.System
            if(mPhone != null) {
                android.provider.Settings.Secure.putInt(mPhone.getContext().getContentResolver(),
                    android.provider.Settings.Secure.PREFERRED_NETWORK_MODE,
                    preferredNetworkMode );
                //Set the Modem
                mPhone.setPreferredNetworkType(preferredNetworkMode,
                    this.obtainMessage(MyHandler.MESSAGE_SET_PREFERRED_NETWORK_TYPE));
             }
        }

    }


    private void UpdatePreferredNetworkModeSummary(int NetworkMode) {
        switch(NetworkMode) {
            case Phone.PREFERRED_NT_MODE:
                mButtonPreferredNetworkMode.setSummary("Preferred network mode: Auto");
                break;
            case Phone.NT_MODE_GSM_ONLY:
                mButtonPreferredNetworkMode.setSummary("Preferred network mode: GSM only");
                break;
            case Phone.NT_MODE_WCDMA_ONLY:
                if (mModemType == TelephonyManager.MODEM_TYPE_TDSCDMA) {
                    mButtonPreferredNetworkMode.setSummary("Preferred network mode: TD-SCDMA only");
                } else if (mModemType == TelephonyManager.MODEM_TYPE_WCDMA) {
                    mButtonPreferredNetworkMode.setSummary("Preferred network mode: WCDMA only");
                }
                break;
            case PREFERRED_NT_MODE_SEARCHING_STATE:
                mButtonPreferredNetworkMode.setSummary("Preferred network mode: Searching");
                break;
            default:
                mButtonPreferredNetworkMode.setSummary("Preferred network mode: Auto");
                break;
        }
    }

    public boolean onPreferenceChange(Preference preference, Object objValue){
        // TODO Auto-generated method stub
        if (preference == mButtonPreferredNetworkMode) {
            //NOTE onPreferenceChange seems to be called even if there is no change
            //Check if the button value is changed from the System.Setting
            mButtonPreferredNetworkMode.setValue((String) objValue);
            int buttonNetworkMode;
            buttonNetworkMode = Integer.valueOf((String) objValue).intValue();
            int settingsNetworkMode = 0;
            if(mPhone != null) {
                settingsNetworkMode = android.provider.Settings.Secure.getInt(
                    mPhone.getContext().getContentResolver(),
                    android.provider.Settings.Secure.PREFERRED_NETWORK_MODE, preferredNetworkMode);
            }
            
            if (buttonNetworkMode != settingsNetworkMode) {
                int modemNetworkMode;
                switch(buttonNetworkMode) {
                case Phone.PREFERRED_NT_MODE:
                    modemNetworkMode = Phone.PREFERRED_NT_MODE;
                    break;
                case Phone.NT_MODE_GSM_ONLY:
                    modemNetworkMode = Phone.NT_MODE_GSM_ONLY;
                    break;
                case Phone.NT_MODE_WCDMA_ONLY:
                    modemNetworkMode = Phone.NT_MODE_WCDMA_ONLY;
                    break;
                default:
                    modemNetworkMode = Phone.PREFERRED_NT_MODE;
                }
                UpdatePreferredNetworkModeSummary(buttonNetworkMode);

                if(mPhone != null) {
                    android.provider.Settings.Secure.putInt(mPhone.getContext().getContentResolver(),
                        android.provider.Settings.Secure.PREFERRED_NETWORK_MODE,
                        buttonNetworkMode );
                    //Set the modem network mode
                    mPhone.setPreferredNetworkType(modemNetworkMode, mHandler
                         .obtainMessage(MyHandler.MESSAGE_SET_PREFERRED_NETWORK_TYPE));
                }

                mNetworkModereferenceEnabled = false;
            }
            /*Add 20130129 Spreadst of 121769 check whether the dialog is dismiss start  */
            if(mButtonPreferredNetworkMode.getDialog()!=null){
                mButtonPreferredNetworkMode.getDialog().dismiss();
            }
            /*Add 20130129 Spreadst of 121769 check whether the dialog is dismiss end   */
            finish();
        }

        // always let the preference setting proceed.
        return true;
    }

    private static void log(String msg) {
        if(DEBUG) Log.d(LOG_TAG, msg);
    }

}

