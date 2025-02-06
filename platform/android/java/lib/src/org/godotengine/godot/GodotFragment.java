/**************************************************************************/
/*  GodotFragment.java                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

package org.godotengine.godot;

import org.godotengine.godot.error.Error;
import org.godotengine.godot.plugin.GodotPlugin;
import org.godotengine.godot.utils.BenchmarkUtils;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.os.Messenger;
import android.text.TextUtils;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.google.android.vending.expansion.downloader.DownloadProgressInfo;
import com.google.android.vending.expansion.downloader.DownloaderClientMarshaller;
import com.google.android.vending.expansion.downloader.DownloaderServiceMarshaller;
import com.google.android.vending.expansion.downloader.Helpers;
import com.google.android.vending.expansion.downloader.IDownloaderClient;
import com.google.android.vending.expansion.downloader.IDownloaderService;
import com.google.android.vending.expansion.downloader.IStub;

import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/**
 * Base fragment for Android apps intending to use Godot for part of the app's UI.
 */
public class GodotFragment extends Fragment implements IDownloaderClient, GodotHost {
	private static final String TAG = GodotFragment.class.getSimpleName();

	private IStub mDownloaderClientStub;
	private TextView mStatusText;
	private TextView mProgressFraction;
	private TextView mProgressPercent;
	private TextView mAverageSpeed;
	private TextView mTimeRemaining;
	private ProgressBar mPB;

	private View mDashboard;
	private View mCellMessage;

	private Button mPauseButton;
	private Button mWiFiSettingsButton;

	private FrameLayout godotContainerLayout;
	private boolean mStatePaused;
	private int mState;

	@Nullable
	private GodotHost parentHost;
	private Godot godot;

	static private Intent mCurrentIntent;

	public void onNewIntent(Intent intent) {
		mCurrentIntent = intent;
	}

	static public Intent getCurrentIntent() {
		return mCurrentIntent;
	}

	private void setState(int newState) {
		if (mState != newState) {
			mState = newState;
			mStatusText.setText(Helpers.getDownloaderStringResourceIDFromState(newState));
		}
	}

	private void setButtonPausedState(boolean paused) {
		mStatePaused = paused;
		int stringResourceID = paused ? R.string.text_button_resume : R.string.text_button_pause;
		mPauseButton.setText(stringResourceID);
	}

	public interface ResultCallback {
		void callback(int requestCode, int resultCode, Intent data);
	}
	public ResultCallback resultCallback;

	@Override
	public Godot getGodot() {
		return godot;
	}

	@Override
	public void onAttach(@NonNull Context context) {
		super.onAttach(context);
		if (getParentFragment() instanceof GodotHost) {
			parentHost = (GodotHost)getParentFragment();
		} else if (getActivity() instanceof GodotHost) {
			parentHost = (GodotHost)getActivity();
		}
	}

	@Override
	public void onDetach() {
		super.onDetach();
		parentHost = null;
	}

	@CallSuper
	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
		godot.onConfigurationChanged(newConfig);
	}

	@CallSuper
	@Override
	public void onActivityResult(int requestCode, int resultCode, Intent data) {
		super.onActivityResult(requestCode, resultCode, data);
		if (resultCallback != null) {
			resultCallback.callback(requestCode, resultCode, data);
			resultCallback = null;
		}

		godot.onActivityResult(requestCode, resultCode, data);
	}

	@CallSuper
	@Override
	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
		super.onRequestPermissionsResult(requestCode, permissions, grantResults);
		godot.onRequestPermissionsResult(requestCode, permissions, grantResults);
	}

	@Override
	public void onServiceConnected(Messenger m) {
		IDownloaderService remoteService = DownloaderServiceMarshaller.CreateProxy(m);
		remoteService.onClientUpdated(mDownloaderClientStub.getMessenger());
	}

	@Override
	public void onCreate(Bundle icicle) {
		BenchmarkUtils.beginBenchmarkMeasure("Startup", "GodotFragment::onCreate");
		super.onCreate(icicle);

		final Activity activity = getActivity();
		mCurrentIntent = activity.getIntent();

		if (parentHost != null) {
			godot = parentHost.getGodot();
		}
		if (godot == null) {
			godot = new Godot(requireContext());
		}
		performEngineInitialization();
		BenchmarkUtils.endBenchmarkMeasure("Startup", "GodotFragment::onCreate");
	}

	private void performEngineInitialization() {
		try {
			godot.onCreate(this);

			if (!godot.onInitNativeLayer(this)) {
				throw new IllegalStateException("Unable to initialize engine native layer");
			}

			godotContainerLayout = godot.onInitRenderView(this);
			if (godotContainerLayout == null) {
				throw new IllegalStateException("Unable to initialize engine render view");
			}
		} catch (IllegalStateException e) {
			Log.e(TAG, "Engine initialization failed", e);
			final String errorMessage = TextUtils.isEmpty(e.getMessage())
					? getString(R.string.error_engine_setup_message)
					: e.getMessage();
			godot.alert(errorMessage, getString(R.string.text_error_title), godot::destroyAndKillProcess);
		} catch (IllegalArgumentException ignored) {
			final Activity activity = getActivity();
			Intent notifierIntent = new Intent(activity, activity.getClass());
			notifierIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);

			PendingIntent pendingIntent;
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
				pendingIntent = PendingIntent.getActivity(activity, 0,
						notifierIntent, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
			} else {
				pendingIntent = PendingIntent.getActivity(activity, 0,
						notifierIntent, PendingIntent.FLAG_UPDATE_CURRENT);
			}

			int startResult;
			try {
				startResult = DownloaderClientMarshaller.startDownloadServiceIfRequired(getContext(), pendingIntent, GodotDownloaderService.class);

				if (startResult != DownloaderClientMarshaller.NO_DOWNLOAD_REQUIRED) {
					// This is where you do set up to display the download
					// progress (next step in onCreateView)
					mDownloaderClientStub = DownloaderClientMarshaller.CreateStub(this, GodotDownloaderService.class);
					return;
				}

				// Restart engine initialization
				performEngineInitialization();
			} catch (PackageManager.NameNotFoundException e) {
				Log.e(TAG, "Unable to start download service", e);
			}
		}
	}

	@Override
	public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle icicle) {
		if (mDownloaderClientStub != null) {
			View downloadingExpansionView =
					inflater.inflate(R.layout.downloading_expansion, container, false);
			mPB = (ProgressBar)downloadingExpansionView.findViewById(R.id.progressBar);
			mStatusText = (TextView)downloadingExpansionView.findViewById(R.id.statusText);
			mProgressFraction = (TextView)downloadingExpansionView.findViewById(R.id.progressAsFraction);
			mProgressPercent = (TextView)downloadingExpansionView.findViewById(R.id.progressAsPercentage);
			mAverageSpeed = (TextView)downloadingExpansionView.findViewById(R.id.progressAverageSpeed);
			mTimeRemaining = (TextView)downloadingExpansionView.findViewById(R.id.progressTimeRemaining);
			mDashboard = downloadingExpansionView.findViewById(R.id.downloaderDashboard);
			mCellMessage = downloadingExpansionView.findViewById(R.id.approveCellular);
			mPauseButton = (Button)downloadingExpansionView.findViewById(R.id.pauseButton);
			mWiFiSettingsButton = (Button)downloadingExpansionView.findViewById(R.id.wifiSettingsButton);

			return downloadingExpansionView;
		}

		return godotContainerLayout;
	}

	@Override
	public void onDestroy() {
		godot.onDestroy(this);
		super.onDestroy();
	}

	@Override
	public void onPause() {
		super.onPause();

		if (!godot.isInitialized()) {
			if (null != mDownloaderClientStub) {
				mDownloaderClientStub.disconnect(getActivity());
			}
			return;
		}

		godot.onPause(this);
	}

	@Override
	public void onStop() {
		super.onStop();
		if (!godot.isInitialized()) {
			if (null != mDownloaderClientStub) {
				mDownloaderClientStub.disconnect(getActivity());
			}
			return;
		}

		godot.onStop(this);
	}

	@Override
	public void onStart() {
		super.onStart();
		if (!godot.isInitialized()) {
			if (null != mDownloaderClientStub) {
				mDownloaderClientStub.connect(getActivity());
			}
			return;
		}

		godot.onStart(this);
	}

	@Override
	public void onResume() {
		super.onResume();
		if (!godot.isInitialized()) {
			if (null != mDownloaderClientStub) {
				mDownloaderClientStub.connect(getActivity());
			}
			return;
		}

		godot.onResume(this);
	}

	public void onBackPressed() {
		godot.onBackPressed();
	}

	/**
	 * The download state should trigger changes in the UI --- it may be useful
	 * to show the state as being indeterminate at times. This sample can be
	 * considered a guideline.
	 */
	@Override
	public void onDownloadStateChanged(int newState) {
		setState(newState);
		boolean showDashboard = true;
		boolean showCellMessage = false;
		boolean paused;
		boolean indeterminate;
		switch (newState) {
			case IDownloaderClient.STATE_IDLE:
				// STATE_IDLE means the service is listening, so it's
				// safe to start making remote service calls.
				paused = false;
				indeterminate = true;
				break;
			case IDownloaderClient.STATE_CONNECTING:
			case IDownloaderClient.STATE_FETCHING_URL:
				showDashboard = true;
				paused = false;
				indeterminate = true;
				break;
			case IDownloaderClient.STATE_DOWNLOADING:
				paused = false;
				showDashboard = true;
				indeterminate = false;
				break;

			case IDownloaderClient.STATE_FAILED_CANCELED:
			case IDownloaderClient.STATE_FAILED:
			case IDownloaderClient.STATE_FAILED_FETCHING_URL:
			case IDownloaderClient.STATE_FAILED_UNLICENSED:
				paused = true;
				showDashboard = false;
				indeterminate = false;
				break;
			case IDownloaderClient.STATE_PAUSED_NEED_CELLULAR_PERMISSION:
			case IDownloaderClient.STATE_PAUSED_WIFI_DISABLED_NEED_CELLULAR_PERMISSION:
				showDashboard = false;
				paused = true;
				indeterminate = false;
				showCellMessage = true;
				break;

			case IDownloaderClient.STATE_PAUSED_BY_REQUEST:
				paused = true;
				indeterminate = false;
				break;
			case IDownloaderClient.STATE_PAUSED_ROAMING:
			case IDownloaderClient.STATE_PAUSED_SDCARD_UNAVAILABLE:
				paused = true;
				indeterminate = false;
				break;
			case IDownloaderClient.STATE_COMPLETED:
				showDashboard = false;
				paused = false;
				indeterminate = false;
				performEngineInitialization();
				return;
			default:
				paused = true;
				indeterminate = true;
				showDashboard = true;
		}
		int newDashboardVisibility = showDashboard ? View.VISIBLE : View.GONE;
		if (mDashboard.getVisibility() != newDashboardVisibility) {
			mDashboard.setVisibility(newDashboardVisibility);
		}
		int cellMessageVisibility = showCellMessage ? View.VISIBLE : View.GONE;
		if (mCellMessage.getVisibility() != cellMessageVisibility) {
			mCellMessage.setVisibility(cellMessageVisibility);
		}

		mPB.setIndeterminate(indeterminate);
		setButtonPausedState(paused);
	}

	@Override
	public void onDownloadProgress(DownloadProgressInfo progress) {
		mAverageSpeed.setText(getString(R.string.kilobytes_per_second,
				Helpers.getSpeedString(progress.mCurrentSpeed)));
		mTimeRemaining.setText(getString(R.string.time_remaining,
				Helpers.getTimeRemaining(progress.mTimeRemaining)));

		mPB.setMax((int)(progress.mOverallTotal >> 8));
		mPB.setProgress((int)(progress.mOverallProgress >> 8));
		mProgressPercent.setText(String.format(Locale.ENGLISH, "%d %%", progress.mOverallProgress * 100 / progress.mOverallTotal));
		mProgressFraction.setText(Helpers.getDownloadProgressString(progress.mOverallProgress,
				progress.mOverallTotal));
	}

	@CallSuper
	@Override
	public List<String> getCommandLine() {
		return parentHost != null ? parentHost.getCommandLine() : Collections.emptyList();
	}

	@CallSuper
	@Override
	public void onGodotSetupCompleted() {
		if (parentHost != null) {
			parentHost.onGodotSetupCompleted();
		}
	}

	@CallSuper
	@Override
	public void onGodotMainLoopStarted() {
		if (parentHost != null) {
			parentHost.onGodotMainLoopStarted();
		}
	}

	@Override
	public void onGodotForceQuit(Godot instance) {
		if (parentHost != null) {
			parentHost.onGodotForceQuit(instance);
		}
	}

	@Override
	public boolean onGodotForceQuit(int godotInstanceId) {
		return parentHost != null && parentHost.onGodotForceQuit(godotInstanceId);
	}

	@Override
	public void onGodotRestartRequested(Godot instance) {
		if (parentHost != null) {
			parentHost.onGodotRestartRequested(instance);
		}
	}

	@Override
	public int onNewGodotInstanceRequested(String[] args) {
		if (parentHost != null) {
			return parentHost.onNewGodotInstanceRequested(args);
		}
		return -1;
	}

	@Override
	@CallSuper
	public Set<GodotPlugin> getHostPlugins(Godot engine) {
		if (parentHost != null) {
			return parentHost.getHostPlugins(engine);
		}
		return Collections.emptySet();
	}

	@Override
	public Error signApk(@NonNull String inputPath, @NonNull String outputPath, @NonNull String keystorePath, @NonNull String keystoreUser, @NonNull String keystorePassword) {
		if (parentHost != null) {
			return parentHost.signApk(inputPath, outputPath, keystorePath, keystoreUser, keystorePassword);
		}
		return Error.ERR_UNAVAILABLE;
	}

	@Override
	public Error verifyApk(@NonNull String apkPath) {
		if (parentHost != null) {
			return parentHost.verifyApk(apkPath);
		}
		return Error.ERR_UNAVAILABLE;
	}

	@Override
	public boolean supportsFeature(String featureTag) {
		if (parentHost != null) {
			return parentHost.supportsFeature(featureTag);
		}
		return false;
	}

	@Override
	public void onEditorWorkspaceSelected(String workspace) {
		if (parentHost != null) {
			parentHost.onEditorWorkspaceSelected(workspace);
		}
	}
}
