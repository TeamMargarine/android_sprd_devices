/**
 * add by zhaoty 12-01-16 for flash block information
 */
package com.spreadtrum.android.eng;

import java.io.File;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.List;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.widget.TextView;

public class FlashBlockInfo extends Activity {

	private static final String LOG_TAG = "FlashBlockInfo";
	private TextView mFlashblock;
	private final String MTD_PATH = "/proc/mtd";
	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.flashblockinfo);

		mFlashblock = (TextView) findViewById(R.id.flashblock);
	}

	@Override
	protected void onResume() {
		super.onResume();
		File file = new File(MTD_PATH);
        BufferedReader reader = null;
        try {
        	reader = new BufferedReader(new FileReader(file));
        	String tmpString = null;
        	String tmpRead = null;
        	int line = 1;
        	while((tmpRead = reader.readLine())!= null){
        		if(line == 2){
        			tmpString += "\n" + tmpRead;
        			break;
        		}else{
        			tmpString = tmpRead;
        		}
    			tmpRead = null;
        		line ++;
        	}
        	reader.close();
        	reader = null;
        	mFlashblock.setText(tmpString);
        } catch (IOException e) {
            Log.e(LOG_TAG,"Read file failed.");
        }finally{
            if(reader != null){
                try {
                	reader.close();
	            } catch (IOException e) {
					Log.e(LOG_TAG,"Read file failed.");
				}
             }
        }
	}
    
}
