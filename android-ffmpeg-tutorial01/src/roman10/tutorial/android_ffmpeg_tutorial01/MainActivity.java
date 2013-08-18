package roman10.tutorial.android_ffmpeg_tutorial01;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.app.Activity;
import android.app.ProgressDialog;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Bitmap.CompressFormat;
import android.graphics.drawable.Drawable;
import android.support.v4.view.PagerAdapter;
import android.support.v4.view.ViewPager;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;

public class MainActivity extends Activity {
	private static final String FRAME_DUMP_FOLDER_PATH = Environment.getExternalStorageDirectory() 
			+ File.separator + "android-ffmpeg-tutorial01";
	private EditText mEditNumOfFrames;
	private ViewPager mViewPagerFrames;
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);
		//create directory for the tutorial
		File dumpFolder = new File(FRAME_DUMP_FOLDER_PATH);
		if (!dumpFolder.exists()) {
			dumpFolder.mkdirs();
		}
		//copy input video file from assets folder to directory
		Utils.copyAssets(this, "1.mp4", FRAME_DUMP_FOLDER_PATH);
		mEditNumOfFrames = (EditText) this.findViewById(R.id.editNumOfFrames);
		mViewPagerFrames = (ViewPager) this.findViewById(R.id.viewPagerFrames);
		Button btnStart = (Button) this.findViewById(R.id.buttonStart);
		btnStart.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				//get number of frames to grab
				String numText = mEditNumOfFrames.getText().toString();
				int numOfFrames;
				try {
					numOfFrames = Integer.valueOf(numText);
				} catch (Exception e) {
					e.printStackTrace();
					numOfFrames = 5;
				}
				//limit number of frames to grab to 20
				if (numOfFrames > 20) {
					numOfFrames = 20;
				}
				//start processing using asynctask
				DumpFrameTask task = new DumpFrameTask(MainActivity.this, numOfFrames);
				task.execute();
			}
		});
	}
	
	private void displayDumpedFrames(){
		//populate the view pager
		if (null != mViewPagerFrames) {
			VideoFrameAdapter adapter = new VideoFrameAdapter(MainActivity.this, 
					FRAME_DUMP_FOLDER_PATH);
			mViewPagerFrames.setAdapter(adapter);
		}
	}
	
	private static class DumpFrameTask extends AsyncTask<Void, Integer, Void> {
		int mlNumOfFrames;
		ProgressDialog mlDialog;
		MainActivity mlOuterAct;
		DumpFrameTask(MainActivity pContext, int pNumOfFrames) {
			mlNumOfFrames = pNumOfFrames;
			mlOuterAct = pContext;
		}
		@Override
        protected void onPreExecute() {
			mlDialog = ProgressDialog.show(mlOuterAct, "Dump Frames","Processing..Wait.." , false);
        }
		@Override
		protected Void doInBackground(Void... params) {
			naMain(mlOuterAct, FRAME_DUMP_FOLDER_PATH + File.separator + "1.mp4", mlNumOfFrames);
			return null;
		}
	    @Override
	    protected void onPostExecute(Void param) {
	    	if (null != mlDialog && mlDialog.isShowing()) {
	    		mlDialog.dismiss();
	    	}
	    	mlOuterAct.displayDumpedFrames();
	    }
	  }
	
	private void saveFrameToPath(Bitmap bitmap, String pPath) {
		int BUFFER_SIZE = 1024 * 8;
	    try {
	    	File file = new File(pPath);
	        file.createNewFile();
	        FileOutputStream fos = new FileOutputStream(file);
	        final BufferedOutputStream bos = new BufferedOutputStream(fos, BUFFER_SIZE);
	        bitmap.compress(CompressFormat.JPEG, 100, bos);
	        bos.flush();
	        bos.close();
	        fos.close();
	    } catch (FileNotFoundException e) {
	        e.printStackTrace();
	    } catch (IOException e) {
	    	e.printStackTrace();
	    }
	}
	

	private static class VideoFrameAdapter extends PagerAdapter {
		List<String> mlFramePaths;
		MainActivity mlAct;
		VideoFrameAdapter(MainActivity pAct, String pFrameFolderPath) {
			mlAct = pAct;
			File frameFolder = new File(pFrameFolderPath);
			File[] framePaths = frameFolder.listFiles();
			mlFramePaths = new ArrayList<String>();
			for (File aFile : framePaths) {
				if (aFile.getAbsolutePath().endsWith(".jpg")) {
					mlFramePaths.add(aFile.getAbsolutePath());
				}
			}
		}
		public Object instantiateItem(View collection, int position) {
			ImageView view = new ImageView(mlAct);
			view.setLayoutParams(new LayoutParams(LayoutParams.FILL_PARENT,
			    LayoutParams.FILL_PARENT));
			view.setScaleType(ScaleType.CENTER_INSIDE);
			view.setImageDrawable(Drawable.createFromPath(mlFramePaths.get(position)));
			((ViewPager) collection).addView(view);
			return view;
		}

		@Override
		public void destroyItem(View arg0, int arg1, Object arg2) {
			((ViewPager) arg0).removeView((View) arg2);
		}
		
		@Override
		public int getCount() {
			if (null != mlFramePaths) {
				return mlFramePaths.size();
			} else {
				return 0;
			}
		}

		@Override
		public boolean isViewFromObject(View view, Object object) {
			return view == object;
		}
	}
	
	private static native int naMain(MainActivity pObject, String pVideoFileName, int pNumOfFrames);
	
    static {
    	System.loadLibrary("avutil-52");
        System.loadLibrary("avcodec-55");
        System.loadLibrary("avformat-55");
        System.loadLibrary("swscale-2");
    	System.loadLibrary("tutorial01");
    }
}
