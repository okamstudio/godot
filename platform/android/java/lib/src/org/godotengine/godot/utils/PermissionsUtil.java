/**************************************************************************/
/*  PermissionsUtil.java                                                  */
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

package org.godotengine.godot.utils;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PermissionInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * This class includes utility functions for Android permissions related operations.
 */
public final class PermissionsUtil {
	private static final String TAG = PermissionsUtil.class.getSimpleName();

	public static final int REQUEST_RECORD_AUDIO_PERMISSION = 1;
	public static final int REQUEST_CAMERA_PERMISSION = 2;
	public static final int REQUEST_VIBRATE_PERMISSION = 3;
	public static final int REQUEST_ALL_PERMISSION_REQ_CODE = 1001;
	public static final int REQUEST_SINGLE_PERMISSION_REQ_CODE = 1002;
	public static final int REQUEST_MANAGE_EXTERNAL_STORAGE_REQ_CODE = 2002;
	public static final int REQUEST_INSTALL_PACKAGES_REQ_CODE = 3002;

	private PermissionsUtil() {
	}

	/**
	 * Request a list of dangerous permissions. The requested permissions must be included in the app's AndroidManifest
	 * @param permissions list of the permissions to request.
	 * @param activity the caller activity for this method.
	 * @return true/false. "true" if permissions are already granted, "false" if a permissions request was dispatched.
	 */
	public static boolean requestPermissions(@NonNull Activity activity, List<String> permissions) {
		return requestPermissions(activity, permissions, REQUEST_ALL_PERMISSION_REQ_CODE);
	}

	private static boolean requestPermissions(@NonNull Activity activity, List<String> permissions, int requestCode) {
		if (permissions == null || permissions.isEmpty()) {
			return true;
		}

		if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
			// Not necessary, asked on install already
			return true;
		}

		boolean dispatchedPermissionsRequest = false;
		Set<String> requestedPermissions = new HashSet<>();
		for (String permission : permissions) {
			try {
				if (permission.equals(Manifest.permission.MANAGE_EXTERNAL_STORAGE)) {
					if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && !Environment.isExternalStorageManager()) {
						Log.d(TAG, "Requesting permission " + permission);
						try {
							Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
							intent.setData(Uri.parse(String.format("package:%s", activity.getPackageName())));
							activity.startActivityForResult(intent, REQUEST_MANAGE_EXTERNAL_STORAGE_REQ_CODE);
						} catch (Exception ignored) {
							Intent intent = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
							activity.startActivityForResult(intent, REQUEST_MANAGE_EXTERNAL_STORAGE_REQ_CODE);
						}
						dispatchedPermissionsRequest = true;
					}
				} else if (permission.equals(Manifest.permission.REQUEST_INSTALL_PACKAGES)) {
					if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && !activity.getPackageManager().canRequestPackageInstalls()) {
						try {
							Intent intent = new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES);
							intent.setData(Uri.parse(String.format("package:%s", activity.getPackageName())));
							activity.startActivityForResult(intent, REQUEST_INSTALL_PACKAGES_REQ_CODE);
							dispatchedPermissionsRequest = true;
						} catch (Exception e) {
							Log.e(TAG, "Unable to request permission " + Manifest.permission.REQUEST_INSTALL_PACKAGES);
						}
					}
				} else {
					PermissionInfo permissionInfo = getPermissionInfo(activity, permission);
					int protectionLevel = Build.VERSION.SDK_INT >= Build.VERSION_CODES.P ? permissionInfo.getProtection() : permissionInfo.protectionLevel;
					if ((protectionLevel & PermissionInfo.PROTECTION_DANGEROUS) == PermissionInfo.PROTECTION_DANGEROUS && ContextCompat.checkSelfPermission(activity, permission) != PackageManager.PERMISSION_GRANTED) {
						Log.d(TAG, "Requesting permission " + permission);
						requestedPermissions.add(permission);
					}
				}
			} catch (PackageManager.NameNotFoundException e) {
				// Skip this permission and continue.
				Log.w(TAG, "Unable to identify permission " + permission, e);
			}
		}

		if (!requestedPermissions.isEmpty()) {
			activity.requestPermissions(requestedPermissions.toArray(new String[0]), requestCode);
			dispatchedPermissionsRequest = true;
		}

		return !dispatchedPermissionsRequest;
	}

	/**
	 * Request a dangerous permission. The requested permission must be included in the app's AndroidManifest
	 * @param permissionName the name of the permission to request.
	 * @param activity the caller activity for this method.
	 * @return true/false. "true" if permission is already granted, "false" if a permission request was dispatched.
	 */
	public static boolean requestPermission(String permissionName, @NonNull Activity activity) {
		if (TextUtils.isEmpty(permissionName)) {
			return true;
		}

		final int requestCode;
		final String updatedPermissionName;
		switch (permissionName) {
			case "RECORD_AUDIO":
				updatedPermissionName = Manifest.permission.RECORD_AUDIO;
				requestCode = REQUEST_RECORD_AUDIO_PERMISSION;
				break;

			case "CAMERA":
				updatedPermissionName = Manifest.permission.CAMERA;
				requestCode = REQUEST_CAMERA_PERMISSION;
				break;

			case "VIBRATE":
				updatedPermissionName = Manifest.permission.VIBRATE;
				requestCode = REQUEST_VIBRATE_PERMISSION;
				break;

			default:
				updatedPermissionName = permissionName;
				requestCode = REQUEST_SINGLE_PERMISSION_REQ_CODE;
				break;
		}

		List<String> permissions = Collections.singletonList(updatedPermissionName);
		return requestPermissions(activity, permissions, requestCode);
	}

	/**
	 * Request dangerous permissions which are defined in the Android manifest file from the user.
	 * @param activity the caller activity for this method.
	 * @return true/false. "true" if all permissions were already granted, returns "false" if permissions requests were dispatched.
	 */
	public static boolean requestManifestPermissions(Activity activity) {
		return requestManifestPermissions(activity, null);
	}

	/**
	 * Request dangerous permissions which are defined in the Android manifest file from the user.
	 * @param activity the caller activity for this method.
	 * @param excludes Set of permissions to exclude from the request
	 * @return true/false. "true" if all permissions were already granted, returns "false" if permissions requests were dispatched.
	 */
	public static boolean requestManifestPermissions(Activity activity, @Nullable Set<String> excludes) {
		if (activity == null) {
			return false;
		}

		if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
			return true;
		}

		List<String> manifestPermissions;
		try {
			manifestPermissions = getManifestPermissions(activity);
		} catch (PackageManager.NameNotFoundException e) {
			Log.e(TAG, "Unable to retrieve manifest permissions", e);
			return false;
		}

		if (manifestPermissions.isEmpty()) {
			return true;
		}

		if (excludes != null && !excludes.isEmpty()) {
			for (String excludedPermission : excludes) {
				manifestPermissions.remove(excludedPermission);
			}
		}

		return requestPermissions(activity, manifestPermissions);
	}

	/**
	 * With this function you can get the list of dangerous permissions that have been granted to the Android application.
	 * @param context the caller context for this method.
	 * @return granted permissions list
	 */
	public static String[] getGrantedPermissions(Context context) {
		List<String> manifestPermissions;
		try {
			manifestPermissions = getManifestPermissions(context);
		} catch (PackageManager.NameNotFoundException e) {
			Log.e(TAG, "Unable to retrieve manifest permissions", e);
			return new String[0];
		}
		if (manifestPermissions.isEmpty()) {
			return new String[0];
		}

		List<String> grantedPermissions = new ArrayList<>();
		for (String manifestPermission : manifestPermissions) {
			try {
				if (manifestPermission.equals(Manifest.permission.MANAGE_EXTERNAL_STORAGE)) {
					if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && Environment.isExternalStorageManager()) {
						grantedPermissions.add(manifestPermission);
					}
				} else {
					PermissionInfo permissionInfo = getPermissionInfo(context, manifestPermission);
					int protectionLevel = Build.VERSION.SDK_INT >= Build.VERSION_CODES.P ? permissionInfo.getProtection() : permissionInfo.protectionLevel;
					if ((protectionLevel & PermissionInfo.PROTECTION_DANGEROUS) == PermissionInfo.PROTECTION_DANGEROUS && ContextCompat.checkSelfPermission(context, manifestPermission) == PackageManager.PERMISSION_GRANTED) {
						grantedPermissions.add(manifestPermission);
					}
				}
			} catch (PackageManager.NameNotFoundException e) {
				// Skip this permission and continue.
				Log.w(TAG, "Unable to identify permission " + manifestPermission, e);
			}
		}

		return grantedPermissions.toArray(new String[0]);
	}

	/**
	 * Check if the given permission is in the AndroidManifest.xml file.
	 * @param context the caller context for this method.
	 * @param permission the permission to look for in the manifest file.
	 * @return "true" if the permission is in the manifest file of the activity, "false" otherwise.
	 */
	public static boolean hasManifestPermission(Context context, String permission) {
		try {
			for (String p : getManifestPermissions(context)) {
				if (permission.equals(p)) {
					return true;
				}
			}
		} catch (PackageManager.NameNotFoundException ignored) {
		}

		return false;
	}

	/**
	 * Returns the permissions defined in the AndroidManifest.xml file.
	 * @param context the caller context for this method.
	 * @return mutable copy of manifest permissions list
	 * @throws PackageManager.NameNotFoundException the exception is thrown when a given package, application, or component name cannot be found.
	 */
	public static ArrayList<String> getManifestPermissions(Context context) throws PackageManager.NameNotFoundException {
		PackageManager packageManager = context.getPackageManager();
		PackageInfo packageInfo = packageManager.getPackageInfo(context.getPackageName(), PackageManager.GET_PERMISSIONS);
		if (packageInfo.requestedPermissions == null) {
			return new ArrayList<>();
		}
		return new ArrayList<>(Arrays.asList(packageInfo.requestedPermissions));
	}

	/**
	 * Returns the information of the desired permission.
	 * @param context the caller context for this method.
	 * @param permission the name of the permission.
	 * @return permission info object
	 * @throws PackageManager.NameNotFoundException the exception is thrown when a given package, application, or component name cannot be found.
	 */
	private static PermissionInfo getPermissionInfo(Context context, String permission) throws PackageManager.NameNotFoundException {
		PackageManager packageManager = context.getPackageManager();
		return packageManager.getPermissionInfo(permission, 0);
	}
}
