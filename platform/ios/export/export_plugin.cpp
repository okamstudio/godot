/**************************************************************************/
/*  export_plugin.cpp                                                     */
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

#include "export_plugin.h"

#include "logo_svg.gen.h"
#include "run_icon_svg.gen.h"

#include "core/io/json.h"
#include "core/io/plist.h"
#include "core/string/translation.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/editor_string_names.h"
#include "editor/export/editor_export.h"
#include "editor/export/lipo.h"
#include "editor/export/macho.h"
#include "editor/import/resource_importer_texture_settings.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/themes/editor_scale.h"

#include "modules/modules_enabled.gen.h" // For mono and svg.
#ifdef MODULE_SVG_ENABLED
#include "modules/svg/image_loader_svg.h"
#endif

void EditorExportPlatformIOS::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const {
	// Vulkan and OpenGL ES 3.0 both mandate ETC2 support.
	r_features->push_back("etc2");
	r_features->push_back("astc");

	Vector<String> architectures = _get_preset_architectures(p_preset);
	for (int i = 0; i < architectures.size(); ++i) {
		r_features->push_back(architectures[i]);
	}
}

Vector<EditorExportPlatformIOS::ExportArchitecture> EditorExportPlatformIOS::_get_supported_architectures() const {
	Vector<ExportArchitecture> archs;
	archs.push_back(ExportArchitecture("arm64", true));
	return archs;
}

struct IconInfo {
	const char *preset_key;
	const char *idiom;
	const char *export_name;
	const char *actual_size_side;
	const char *scale;
	const char *unscaled_size;
	const bool force_opaque;
};

static const IconInfo icon_infos[] = {
	// Settings on iPhone, iPad Pro, iPad, iPad mini
	{ PNAME("icons/settings_58x58"), "universal", "Icon-58", "58", "2x", "29x29", false },
	{ PNAME("icons/settings_87x87"), "universal", "Icon-87", "87", "3x", "29x29", false },

	// Notifications on iPhone, iPad Pro, iPad, iPad mini
	{ PNAME("icons/notification_40x40"), "universal", "Icon-40", "40", "2x", "20x20", false },
	{ PNAME("icons/notification_60x60"), "universal", "Icon-60", "60", "3x", "20x20", false },
	{ PNAME("icons/notification_76x76"), "universal", "Icon-76", "76", "2x", "38x38", false },
	{ PNAME("icons/notification_114x114"), "universal", "Icon-114", "114", "3x", "38x38", false },

	// Spotlight on iPhone, iPad Pro, iPad, iPad mini
	{ PNAME("icons/spotlight_80x80"), "universal", "Icon-80", "80", "2x", "40x40", false },
	{ PNAME("icons/spotlight_120x120"), "universal", "Icon-120", "120", "3x", "40x40", false },

	// Home Screen on iPhone
	{ PNAME("icons/iphone_120x120"), "universal", "Icon-120-1", "120", "2x", "60x60", false },
	{ PNAME("icons/iphone_180x180"), "universal", "Icon-180", "180", "3x", "60x60", false },

	// Home Screen on iPad Pro
	{ PNAME("icons/ipad_167x167"), "universal", "Icon-167", "167", "2x", "83.5x83.5", false },

	// Home Screen on iPad, iPad mini
	{ PNAME("icons/ipad_152x152"), "universal", "Icon-152", "152", "2x", "76x76", false },

	{ PNAME("icons/ios_128x128"), "universal", "Icon-128", "128", "2x", "64x64", false },
	{ PNAME("icons/ios_192x192"), "universal", "Icon-192", "192", "3x", "64x64", false },

	{ PNAME("icons/ios_136x136"), "universal", "Icon-136", "136", "2x", "68x68", false },

	// App Store
	{ PNAME("icons/app_store_1024x1024"), "universal", "Icon-1024", "1024", "1x", "1024x1024", true },
};

struct APIAccessInfo {
	String prop_name;
	String type_name;
	Vector<String> prop_flag_value;
	Vector<String> prop_flag_name;
	int default_value;
};

static const APIAccessInfo api_info[] = {
	{ "file_timestamp",
			"NSPrivacyAccessedAPICategoryFileTimestamp",
			{ "DDA9.1", "C617.1", "3B52.1" },
			{ "Display to user on-device:", "Inside app or group container", "Files provided to app by user" },
			3 },
	{ "system_boot_time",
			"NSPrivacyAccessedAPICategorySystemBootTime",
			{ "35F9.1", "8FFB.1", "3D61.1" },
			{ "Measure time on-device", "Calculate absolute event timestamps", "User-initiated bug report" },
			1 },
	{ "disk_space",
			"NSPrivacyAccessedAPICategoryDiskSpace",
			{ "E174.1", "85F4.1", "7D9E.1", "B728.1" },
			{ "Write or delete file on-device", "Display to user on-device", "User-initiated bug report", "Health research app" },
			3 },
	{ "active_keyboard",
			"NSPrivacyAccessedAPICategoryActiveKeyboards",
			{ "3EC4.1", "54BD.1" },
			{ "Custom keyboard app on-device", "Customize UI on-device:2" },
			0 },
	{ "user_defaults",
			"NSPrivacyAccessedAPICategoryUserDefaults",
			{ "1C8F.1", "AC6B.1", "CA92.1" },
			{ "Access info from same App Group", "Access managed app configuration", "Access info from same app" },
			0 }
};

struct DataCollectionInfo {
	String prop_name;
	String type_name;
};

static const DataCollectionInfo data_collect_type_info[] = {
	{ "name", "NSPrivacyCollectedDataTypeName" },
	{ "email_address", "NSPrivacyCollectedDataTypeEmailAddress" },
	{ "phone_number", "NSPrivacyCollectedDataTypePhoneNumber" },
	{ "physical_address", "NSPrivacyCollectedDataTypePhysicalAddress" },
	{ "other_contact_info", "NSPrivacyCollectedDataTypeOtherUserContactInfo" },
	{ "health", "NSPrivacyCollectedDataTypeHealth" },
	{ "fitness", "NSPrivacyCollectedDataTypeFitness" },
	{ "payment_info", "NSPrivacyCollectedDataTypePaymentInfo" },
	{ "credit_info", "NSPrivacyCollectedDataTypeCreditInfo" },
	{ "other_financial_info", "NSPrivacyCollectedDataTypeOtherFinancialInfo" },
	{ "precise_location", "NSPrivacyCollectedDataTypePreciseLocation" },
	{ "coarse_location", "NSPrivacyCollectedDataTypeCoarseLocation" },
	{ "sensitive_info", "NSPrivacyCollectedDataTypeSensitiveInfo" },
	{ "contacts", "NSPrivacyCollectedDataTypeContacts" },
	{ "emails_or_text_messages", "NSPrivacyCollectedDataTypeEmailsOrTextMessages" },
	{ "photos_or_videos", "NSPrivacyCollectedDataTypePhotosorVideos" },
	{ "audio_data", "NSPrivacyCollectedDataTypeAudioData" },
	{ "gameplay_content", "NSPrivacyCollectedDataTypeGameplayContent" },
	{ "customer_support", "NSPrivacyCollectedDataTypeCustomerSupport" },
	{ "other_user_content", "NSPrivacyCollectedDataTypeOtherUserContent" },
	{ "browsing_history", "NSPrivacyCollectedDataTypeBrowsingHistory" },
	{ "search_hhistory", "NSPrivacyCollectedDataTypeSearchHistory" },
	{ "user_id", "NSPrivacyCollectedDataTypeUserID" },
	{ "device_id", "NSPrivacyCollectedDataTypeDeviceID" },
	{ "purchase_history", "NSPrivacyCollectedDataTypePurchaseHistory" },
	{ "product_interaction", "NSPrivacyCollectedDataTypeProductInteraction" },
	{ "advertising_data", "NSPrivacyCollectedDataTypeAdvertisingData" },
	{ "other_usage_data", "NSPrivacyCollectedDataTypeOtherUsageData" },
	{ "crash_data", "NSPrivacyCollectedDataTypeCrashData" },
	{ "performance_data", "NSPrivacyCollectedDataTypePerformanceData" },
	{ "other_diagnostic_data", "NSPrivacyCollectedDataTypeOtherDiagnosticData" },
	{ "environment_scanning", "NSPrivacyCollectedDataTypeEnvironmentScanning" },
	{ "hands", "NSPrivacyCollectedDataTypeHands" },
	{ "head", "NSPrivacyCollectedDataTypeHead" },
	{ "other_data_types", "NSPrivacyCollectedDataTypeOtherDataTypes" },
};

static const DataCollectionInfo data_collect_purpose_info[] = {
	{ "Analytics", "NSPrivacyCollectedDataTypePurposeAnalytics" },
	{ "App Functionality", "NSPrivacyCollectedDataTypePurposeAppFunctionality" },
	{ "Developer Advertising", "NSPrivacyCollectedDataTypePurposeDeveloperAdvertising" },
	{ "Third-party Advertising", "NSPrivacyCollectedDataTypePurposeThirdPartyAdvertising" },
	{ "Product Personalization", "NSPrivacyCollectedDataTypePurposeProductPersonalization" },
	{ "Other", "NSPrivacyCollectedDataTypePurposeOther" },
};

String EditorExportPlatformIOS::get_export_option_warning(const EditorExportPreset *p_preset, const StringName &p_name) const {
	if (p_preset) {
		if (p_name == "application/app_store_team_id") {
			String team_id = p_preset->get("application/app_store_team_id");
			if (team_id.is_empty()) {
				return TTR("App Store Team ID not specified.") + "\n";
			}
		} else if (p_name == "application/bundle_identifier") {
			String identifier = p_preset->get("application/bundle_identifier");
			String pn_err;
			if (!is_package_name_valid(identifier, &pn_err)) {
				return TTR("Invalid Identifier:") + " " + pn_err;
			}
		} else if (p_name == "privacy/file_timestamp_access_reasons") {
			int access = p_preset->get("privacy/file_timestamp_access_reasons");
			if (access == 0) {
				return TTR("At least one file timestamp access reason should be selected.");
			}
		} else if (p_name == "privacy/disk_space_access_reasons") {
			int access = p_preset->get("privacy/disk_space_access_reasons");
			if (access == 0) {
				return TTR("At least one disk space access reason should be selected.");
			}
		} else if (p_name == "privacy/system_boot_time_access_reasons") {
			int access = p_preset->get("privacy/system_boot_time_access_reasons");
			if (access == 0) {
				return TTR("At least one system boot time access reason should be selected.");
			}
		}
	}
	return String();
}

void EditorExportPlatformIOS::_notification(int p_what) {
#ifdef MACOS_ENABLED
	if (p_what == NOTIFICATION_POSTINITIALIZE) {
		if (EditorExport::get_singleton()) {
			EditorExport::get_singleton()->connect_presets_runnable_updated(callable_mp(this, &EditorExportPlatformIOS::_update_preset_status));
		}
	}
#endif
}

bool EditorExportPlatformIOS::get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const {
	// Hide unsupported .NET embedding option.
	if (p_option == "dotnet/embed_build_outputs") {
		return false;
	}

	if (p_preset == nullptr) {
		return true;
	}

	bool advanced_options_enabled = p_preset->are_advanced_options_enabled();
	if (p_option.begins_with("privacy") || p_option == "application/generate_simulator_library_if_missing" || (p_option.begins_with("icons/") && !p_option.begins_with("icons/icon") && !p_option.begins_with("icons/app_store"))) {
		return advanced_options_enabled;
	}

	return true;
}

void EditorExportPlatformIOS::get_export_options(List<ExportOption> *r_options) const {
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_template/debug", PROPERTY_HINT_GLOBAL_FILE, "*.zip"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_template/release", PROPERTY_HINT_GLOBAL_FILE, "*.zip"), ""));

	Vector<ExportArchitecture> architectures = _get_supported_architectures();
	for (int i = 0; i < architectures.size(); ++i) {
		r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, vformat("%s/%s", PNAME("architectures"), architectures[i].name)), architectures[i].is_default));
	}

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/app_store_team_id"), "", false, true));

	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "application/export_method_debug", PROPERTY_HINT_ENUM, "App Store,Development,Ad-Hoc,Enterprise"), 1));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/code_sign_identity_debug", PROPERTY_HINT_PLACEHOLDER_TEXT, "iPhone Developer"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/code_sign_identity_release", PROPERTY_HINT_PLACEHOLDER_TEXT, "iPhone Distribution"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/provisioning_profile_uuid_debug", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/provisioning_profile_uuid_release", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/provisioning_profile_specifier_debug", PROPERTY_HINT_PLACEHOLDER_TEXT, ""), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/provisioning_profile_specifier_release", PROPERTY_HINT_PLACEHOLDER_TEXT, ""), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "application/export_method_release", PROPERTY_HINT_ENUM, "App Store,Development,Ad-Hoc,Enterprise"), 0));

	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "application/targeted_device_family", PROPERTY_HINT_ENUM, "iPhone,iPad,iPhone & iPad"), 2));

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/bundle_identifier", PROPERTY_HINT_PLACEHOLDER_TEXT, "com.example.game"), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/signature"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/short_version", PROPERTY_HINT_PLACEHOLDER_TEXT, "Leave empty to use project version"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/version", PROPERTY_HINT_PLACEHOLDER_TEXT, "Leave empty to use project version"), ""));

	// TODO(sgc): set to iOS 14.0 for Metal
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/min_ios_version"), "12.0"));

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/additional_plist_content", PROPERTY_HINT_MULTILINE_TEXT), ""));

	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "application/icon_interpolation", PROPERTY_HINT_ENUM, "Nearest neighbor,Bilinear,Cubic,Trilinear,Lanczos"), 4));

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "application/export_project_only"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "application/delete_old_export_files_unconditionally"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "application/generate_simulator_library_if_missing"), true));

	Vector<PluginConfigIOS> found_plugins = get_plugins();
	for (int i = 0; i < found_plugins.size(); i++) {
		r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, vformat("%s/%s", PNAME("plugins"), found_plugins[i].name)), false));
	}

	HashSet<String> plist_keys;

	for (int i = 0; i < found_plugins.size(); i++) {
		// Editable plugin plist values
		PluginConfigIOS plugin = found_plugins[i];

		for (const KeyValue<String, PluginConfigIOS::PlistItem> &E : plugin.plist) {
			switch (E.value.type) {
				case PluginConfigIOS::PlistItemType::STRING_INPUT: {
					String preset_name = "plugins_plist/" + E.key;
					if (!plist_keys.has(preset_name)) {
						r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, preset_name), E.value.value));
						plist_keys.insert(preset_name);
					}
				} break;
				default:
					continue;
			}
		}
	}

	plugins_changed.clear();
	plugins = found_plugins;

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "entitlements/increased_memory_limit"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "entitlements/game_center"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "entitlements/push_notifications", PROPERTY_HINT_ENUM, "Disabled,Production,Development"), "Disabled"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "entitlements/additional", PROPERTY_HINT_MULTILINE_TEXT), ""));

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "capabilities/access_wifi"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "capabilities/performance_gaming_tier"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "capabilities/performance_a12"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::PACKED_STRING_ARRAY, "capabilities/additional"), PackedStringArray()));

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "user_data/accessible_from_files_app"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "user_data/accessible_from_itunes_sharing"), false));

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "privacy/camera_usage_description", PROPERTY_HINT_PLACEHOLDER_TEXT, "Provide a message if you need to use the camera"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::DICTIONARY, "privacy/camera_usage_description_localized", PROPERTY_HINT_LOCALIZABLE_STRING), Dictionary()));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "privacy/microphone_usage_description", PROPERTY_HINT_PLACEHOLDER_TEXT, "Provide a message if you need to use the microphone"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::DICTIONARY, "privacy/microphone_usage_description_localized", PROPERTY_HINT_LOCALIZABLE_STRING), Dictionary()));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "privacy/photolibrary_usage_description", PROPERTY_HINT_PLACEHOLDER_TEXT, "Provide a message if you need access to the photo library"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::DICTIONARY, "privacy/photolibrary_usage_description_localized", PROPERTY_HINT_LOCALIZABLE_STRING), Dictionary()));

	for (uint64_t i = 0; i < sizeof(api_info) / sizeof(api_info[0]); ++i) {
		String prop_name = vformat("privacy/%s_access_reasons", api_info[i].prop_name);
		String hint;
		for (int j = 0; j < api_info[i].prop_flag_value.size(); j++) {
			if (j != 0) {
				hint += ",";
			}
			hint += vformat("%s - %s:%d", api_info[i].prop_flag_value[j], api_info[i].prop_flag_name[j], (1 << j));
		}
		r_options->push_back(ExportOption(PropertyInfo(Variant::INT, prop_name, PROPERTY_HINT_FLAGS, hint), api_info[i].default_value));
	}

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "privacy/tracking_enabled"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::PACKED_STRING_ARRAY, "privacy/tracking_domains"), Vector<String>()));

	{
		String hint;
		for (uint64_t i = 0; i < sizeof(data_collect_purpose_info) / sizeof(data_collect_purpose_info[0]); ++i) {
			if (i != 0) {
				hint += ",";
			}
			hint += vformat("%s:%d", data_collect_purpose_info[i].prop_name, (1 << i));
		}
		for (uint64_t i = 0; i < sizeof(data_collect_type_info) / sizeof(data_collect_type_info[0]); ++i) {
			r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, vformat("privacy/collected_data/%s/collected", data_collect_type_info[i].prop_name)), false));
			r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, vformat("privacy/collected_data/%s/linked_to_user", data_collect_type_info[i].prop_name)), false));
			r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, vformat("privacy/collected_data/%s/used_for_tracking", data_collect_type_info[i].prop_name)), false));
			r_options->push_back(ExportOption(PropertyInfo(Variant::INT, vformat("privacy/collected_data/%s/collection_purposes", data_collect_type_info[i].prop_name), PROPERTY_HINT_FLAGS, hint), 0));
		}
	}

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "icons/icon_1024x1024", PROPERTY_HINT_FILE, "*.svg,*.png,*.webp,*.jpg,*.jpeg"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "icons/icon_1024x1024_dark", PROPERTY_HINT_FILE, "*.svg,*.png,*.webp,*.jpg,*.jpeg"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "icons/icon_1024x1024_tinted", PROPERTY_HINT_FILE, "*.svg,*.png,*.webp,*.jpg,*.jpeg"), ""));

	HashSet<String> used_names;
	for (uint64_t i = 0; i < sizeof(icon_infos) / sizeof(icon_infos[0]); ++i) {
		if (!used_names.has(icon_infos[i].preset_key)) {
			used_names.insert(icon_infos[i].preset_key);
			r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, String(icon_infos[i].preset_key), PROPERTY_HINT_FILE, "*.png,*.jpg,*.jpeg"), ""));
			r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, String(icon_infos[i].preset_key) + "_dark", PROPERTY_HINT_FILE, "*.png,*.jpg,*.jpeg"), ""));
			r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, String(icon_infos[i].preset_key) + "_tinted", PROPERTY_HINT_FILE, "*.png,*.jpg,*.jpeg"), ""));
		}
	}
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "storyboard/image_scale_mode", PROPERTY_HINT_ENUM, "Same as Logo,Center,Scale to Fit,Scale to Fill,Scale"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "storyboard/custom_image@2x", PROPERTY_HINT_FILE, "*.png,*.jpg,*.jpeg"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "storyboard/custom_image@3x", PROPERTY_HINT_FILE, "*.png,*.jpg,*.jpeg"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "storyboard/use_custom_bg_color"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::COLOR, "storyboard/custom_bg_color"), Color()));
}

void EditorExportPlatformIOS::_fix_config_file(const Ref<EditorExportPreset> &p_preset, Vector<uint8_t> &pfile, const IOSConfigData &p_config, bool p_debug) {
	static const String export_method_string[] = {
		"app-store",
		"development",
		"ad-hoc",
		"enterprise"
	};
	static const String storyboard_image_scale_mode[] = {
		"center",
		"scaleAspectFit",
		"scaleAspectFill",
		"scaleToFill"
	};
	String dbg_sign_id = p_preset->get("application/code_sign_identity_debug").operator String().is_empty() ? "iPhone Developer" : p_preset->get("application/code_sign_identity_debug");
	String rel_sign_id = p_preset->get("application/code_sign_identity_release").operator String().is_empty() ? "iPhone Distribution" : p_preset->get("application/code_sign_identity_release");
	bool dbg_manual = !p_preset->get_or_env("application/provisioning_profile_uuid_debug", ENV_IOS_PROFILE_UUID_DEBUG).operator String().is_empty() || (dbg_sign_id != "iPhone Developer" && dbg_sign_id != "iPhone Distribution");
	bool rel_manual = !p_preset->get_or_env("application/provisioning_profile_uuid_release", ENV_IOS_PROFILE_UUID_RELEASE).operator String().is_empty() || (rel_sign_id != "iPhone Developer" && rel_sign_id != "iPhone Distribution");

	bool valid_dbg_specifier = false;
	bool valid_rel_specifier = false;
	Variant provisioning_profile_specifier_dbg_variant = p_preset->get_or_env("application/provisioning_profile_specifier_debug", ENV_IOS_PROFILE_SPECIFIER_DEBUG, &valid_dbg_specifier);
	dbg_manual |= valid_dbg_specifier;

	Variant provisioning_profile_specifier_rel_variant = p_preset->get_or_env("application/provisioning_profile_specifier_release", ENV_IOS_PROFILE_SPECIFIER_RELEASE, &valid_rel_specifier);
	rel_manual |= valid_rel_specifier;

	String str;
	String strnew;
	str.parse_utf8((const char *)pfile.ptr(), pfile.size());
	Vector<String> lines = str.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		if (lines[i].contains("$binary")) {
			strnew += lines[i].replace("$binary", p_config.binary_name) + "\n";
		} else if (lines[i].contains("$modules_buildfile")) {
			strnew += lines[i].replace("$modules_buildfile", p_config.modules_buildfile) + "\n";
		} else if (lines[i].contains("$modules_fileref")) {
			strnew += lines[i].replace("$modules_fileref", p_config.modules_fileref) + "\n";
		} else if (lines[i].contains("$modules_buildphase")) {
			strnew += lines[i].replace("$modules_buildphase", p_config.modules_buildphase) + "\n";
		} else if (lines[i].contains("$modules_buildgrp")) {
			strnew += lines[i].replace("$modules_buildgrp", p_config.modules_buildgrp) + "\n";
		} else if (lines[i].contains("$name")) {
			strnew += lines[i].replace("$name", p_config.pkg_name) + "\n";
		} else if (lines[i].contains("$bundle_identifier")) {
			strnew += lines[i].replace("$bundle_identifier", p_preset->get("application/bundle_identifier")) + "\n";
		} else if (lines[i].contains("$short_version")) {
			strnew += lines[i].replace("$short_version", p_preset->get_version("application/short_version")) + "\n";
		} else if (lines[i].contains("$version")) {
			strnew += lines[i].replace("$version", p_preset->get_version("application/version")) + "\n";
		} else if (lines[i].contains("$min_version")) {
			strnew += lines[i].replace("$min_version", p_preset->get("application/min_ios_version")) + "\n";
		} else if (lines[i].contains("$signature")) {
			strnew += lines[i].replace("$signature", p_preset->get("application/signature")) + "\n";
		} else if (lines[i].contains("$team_id")) {
			strnew += lines[i].replace("$team_id", p_preset->get("application/app_store_team_id")) + "\n";
		} else if (lines[i].contains("$default_build_config")) {
			strnew += lines[i].replace("$default_build_config", p_debug ? "Debug" : "Release") + "\n";
		} else if (lines[i].contains("$export_method")) {
			int export_method = p_preset->get(p_debug ? "application/export_method_debug" : "application/export_method_release");
			strnew += lines[i].replace("$export_method", export_method_string[export_method]) + "\n";
		} else if (lines[i].contains("$provisioning_profile_specifier_debug")) {
			String specifier = provisioning_profile_specifier_dbg_variant.get_type() != Variant::NIL ? provisioning_profile_specifier_dbg_variant : "";
			strnew += lines[i].replace("$provisioning_profile_specifier_debug", specifier) + "\n";
		} else if (lines[i].contains("$provisioning_profile_specifier_release")) {
			String specifier = provisioning_profile_specifier_rel_variant.get_type() != Variant::NIL ? provisioning_profile_specifier_rel_variant : "";
			strnew += lines[i].replace("$provisioning_profile_specifier_release", specifier) + "\n";
		} else if (lines[i].contains("$provisioning_profile_specifier")) {
			String specifier = p_debug ? provisioning_profile_specifier_dbg_variant : provisioning_profile_specifier_rel_variant;
			strnew += lines[i].replace("$provisioning_profile_specifier", specifier) + "\n";
		} else if (lines[i].contains("$provisioning_profile_uuid_release")) {
			strnew += lines[i].replace("$provisioning_profile_uuid_release", p_preset->get_or_env("application/provisioning_profile_uuid_release", ENV_IOS_PROFILE_UUID_RELEASE)) + "\n";
		} else if (lines[i].contains("$provisioning_profile_uuid_debug")) {
			strnew += lines[i].replace("$provisioning_profile_uuid_debug", p_preset->get_or_env("application/provisioning_profile_uuid_debug", ENV_IOS_PROFILE_UUID_DEBUG)) + "\n";
		} else if (lines[i].contains("$code_sign_style_debug")) {
			if (dbg_manual) {
				strnew += lines[i].replace("$code_sign_style_debug", "Manual") + "\n";
			} else {
				strnew += lines[i].replace("$code_sign_style_debug", "Automatic") + "\n";
			}
		} else if (lines[i].contains("$code_sign_style_release")) {
			if (rel_manual) {
				strnew += lines[i].replace("$code_sign_style_release", "Manual") + "\n";
			} else {
				strnew += lines[i].replace("$code_sign_style_release", "Automatic") + "\n";
			}
		} else if (lines[i].contains("$provisioning_profile_uuid")) {
			bool valid = false;
			String uuid = p_debug ? p_preset->get_or_env("application/provisioning_profile_uuid_debug", ENV_IOS_PROFILE_UUID_DEBUG, &valid) : p_preset->get_or_env("application/provisioning_profile_uuid_release", ENV_IOS_PROFILE_UUID_RELEASE, &valid);
			if (!valid || uuid.is_empty()) {
				Variant variant = p_debug ? provisioning_profile_specifier_dbg_variant : provisioning_profile_specifier_rel_variant;
				valid = p_debug ? valid_dbg_specifier : valid_rel_specifier;
				uuid = valid ? variant : "";
			}
			strnew += lines[i].replace("$provisioning_profile_uuid", uuid) + "\n";
		} else if (lines[i].contains("$code_sign_identity_debug")) {
			strnew += lines[i].replace("$code_sign_identity_debug", dbg_sign_id) + "\n";
		} else if (lines[i].contains("$code_sign_identity_release")) {
			strnew += lines[i].replace("$code_sign_identity_release", rel_sign_id) + "\n";
		} else if (lines[i].contains("$additional_plist_content")) {
			strnew += lines[i].replace("$additional_plist_content", p_config.plist_content) + "\n";
		} else if (lines[i].contains("$godot_archs")) {
			strnew += lines[i].replace("$godot_archs", p_config.architectures) + "\n";
		} else if (lines[i].contains("$linker_flags")) {
			strnew += lines[i].replace("$linker_flags", p_config.linker_flags) + "\n";
		} else if (lines[i].contains("$targeted_device_family")) {
			String xcode_value;
			switch ((int)p_preset->get("application/targeted_device_family")) {
				case 0: // iPhone
					xcode_value = "1";
					break;
				case 1: // iPad
					xcode_value = "2";
					break;
				case 2: // iPhone & iPad
					xcode_value = "1,2";
					break;
			}
			strnew += lines[i].replace("$targeted_device_family", xcode_value) + "\n";
		} else if (lines[i].contains("$cpp_code")) {
			strnew += lines[i].replace("$cpp_code", p_config.cpp_code) + "\n";
		} else if (lines[i].contains("$docs_in_place")) {
			strnew += lines[i].replace("$docs_in_place", ((bool)p_preset->get("user_data/accessible_from_files_app")) ? "<true/>" : "<false/>") + "\n";
		} else if (lines[i].contains("$docs_sharing")) {
			strnew += lines[i].replace("$docs_sharing", ((bool)p_preset->get("user_data/accessible_from_itunes_sharing")) ? "<true/>" : "<false/>") + "\n";
		} else if (lines[i].contains("$entitlements_full")) {
			String entitlements;
			if ((String)p_preset->get("entitlements/push_notifications") != "Disabled") {
				entitlements += "<key>aps-environment</key>\n<string>" + p_preset->get("entitlements/push_notifications").operator String().to_lower() + "</string>" + "\n";
			}
			if ((bool)p_preset->get("entitlements/game_center")) {
				entitlements += "<key>com.apple.developer.game-center</key>\n<true/>\n";
			}
			if ((bool)p_preset->get("entitlements/increased_memory_limit")) {
				entitlements += "<key>com.apple.developer.kernel.increased-memory-limit</key>\n<true/>\n";
			}
			entitlements += p_preset->get("entitlements/additional").operator String() + "\n";

			strnew += lines[i].replace("$entitlements_full", entitlements);
		} else if (lines[i].contains("$required_device_capabilities")) {
			String capabilities;

			// I've removed armv7 as we can run on 64bit only devices
			// Note that capabilities listed here are requirements for the app to be installed.
			// They don't enable anything.
			Vector<String> capabilities_list = p_config.capabilities;

			if ((bool)p_preset->get("capabilities/access_wifi") && !capabilities_list.has("wifi")) {
				capabilities_list.push_back("wifi");
			}
			if ((bool)p_preset->get("capabilities/performance_gaming_tier") && !capabilities_list.has("iphone-performance-gaming-tier")) {
				capabilities_list.push_back("iphone-performance-gaming-tier");
			}
			if ((bool)p_preset->get("capabilities/performance_a12") && !capabilities_list.has("iphone-ipad-minimum-performance-a12")) {
				capabilities_list.push_back("iphone-ipad-minimum-performance-a12");
			}
			for (int idx = 0; idx < capabilities_list.size(); idx++) {
				capabilities += "<string>" + capabilities_list[idx] + "</string>\n";
			}
			for (const String &cap : p_preset->get("capabilities/additional").operator PackedStringArray()) {
				capabilities += "<string>" + cap + "</string>\n";
			}

			strnew += lines[i].replace("$required_device_capabilities", capabilities);
		} else if (lines[i].contains("$interface_orientations")) {
			String orientations;
			const DisplayServer::ScreenOrientation screen_orientation =
					DisplayServer::ScreenOrientation(int(GLOBAL_GET("display/window/handheld/orientation")));

			switch (screen_orientation) {
				case DisplayServer::SCREEN_LANDSCAPE:
					orientations += "<string>UIInterfaceOrientationLandscapeLeft</string>\n";
					break;
				case DisplayServer::SCREEN_PORTRAIT:
					orientations += "<string>UIInterfaceOrientationPortrait</string>\n";
					break;
				case DisplayServer::SCREEN_REVERSE_LANDSCAPE:
					orientations += "<string>UIInterfaceOrientationLandscapeRight</string>\n";
					break;
				case DisplayServer::SCREEN_REVERSE_PORTRAIT:
					orientations += "<string>UIInterfaceOrientationPortraitUpsideDown</string>\n";
					break;
				case DisplayServer::SCREEN_SENSOR_LANDSCAPE:
					// Allow both landscape orientations depending on sensor direction.
					orientations += "<string>UIInterfaceOrientationLandscapeLeft</string>\n";
					orientations += "<string>UIInterfaceOrientationLandscapeRight</string>\n";
					break;
				case DisplayServer::SCREEN_SENSOR_PORTRAIT:
					// Allow both portrait orientations depending on sensor direction.
					orientations += "<string>UIInterfaceOrientationPortrait</string>\n";
					orientations += "<string>UIInterfaceOrientationPortraitUpsideDown</string>\n";
					break;
				case DisplayServer::SCREEN_SENSOR:
					// Allow all screen orientations depending on sensor direction.
					orientations += "<string>UIInterfaceOrientationLandscapeLeft</string>\n";
					orientations += "<string>UIInterfaceOrientationLandscapeRight</string>\n";
					orientations += "<string>UIInterfaceOrientationPortrait</string>\n";
					orientations += "<string>UIInterfaceOrientationPortraitUpsideDown</string>\n";
					break;
			}

			strnew += lines[i].replace("$interface_orientations", orientations);
		} else if (lines[i].contains("$ipad_interface_orientations")) {
			String orientations;
			const DisplayServer::ScreenOrientation screen_orientation =
					DisplayServer::ScreenOrientation(int(GLOBAL_GET("display/window/handheld/orientation")));

			switch (screen_orientation) {
				case DisplayServer::SCREEN_LANDSCAPE:
					orientations += "<string>UIInterfaceOrientationLandscapeRight</string>\n";
					break;
				case DisplayServer::SCREEN_PORTRAIT:
					orientations += "<string>UIInterfaceOrientationPortrait</string>\n";
					break;
				case DisplayServer::SCREEN_REVERSE_LANDSCAPE:
					orientations += "<string>UIInterfaceOrientationLandscapeLeft</string>\n";
					break;
				case DisplayServer::SCREEN_REVERSE_PORTRAIT:
					orientations += "<string>UIInterfaceOrientationPortraitUpsideDown</string>\n";
					break;
				case DisplayServer::SCREEN_SENSOR_LANDSCAPE:
					// Allow both landscape orientations depending on sensor direction.
					orientations += "<string>UIInterfaceOrientationLandscapeLeft</string>\n";
					orientations += "<string>UIInterfaceOrientationLandscapeRight</string>\n";
					break;
				case DisplayServer::SCREEN_SENSOR_PORTRAIT:
					// Allow both portrait orientations depending on sensor direction.
					orientations += "<string>UIInterfaceOrientationPortrait</string>\n";
					orientations += "<string>UIInterfaceOrientationPortraitUpsideDown</string>\n";
					break;
				case DisplayServer::SCREEN_SENSOR:
					// Allow all screen orientations depending on sensor direction.
					orientations += "<string>UIInterfaceOrientationLandscapeLeft</string>\n";
					orientations += "<string>UIInterfaceOrientationLandscapeRight</string>\n";
					orientations += "<string>UIInterfaceOrientationPortrait</string>\n";
					orientations += "<string>UIInterfaceOrientationPortraitUpsideDown</string>\n";
					break;
			}

			strnew += lines[i].replace("$ipad_interface_orientations", orientations);
		} else if (lines[i].contains("$camera_usage_description")) {
			String description = p_preset->get("privacy/camera_usage_description");
			strnew += lines[i].replace("$camera_usage_description", description) + "\n";
		} else if (lines[i].contains("$microphone_usage_description")) {
			String description = p_preset->get("privacy/microphone_usage_description");
			strnew += lines[i].replace("$microphone_usage_description", description) + "\n";
		} else if (lines[i].contains("$photolibrary_usage_description")) {
			String description = p_preset->get("privacy/photolibrary_usage_description");
			strnew += lines[i].replace("$photolibrary_usage_description", description) + "\n";
		} else if (lines[i].contains("$plist_launch_screen_name")) {
			String value = "<key>UILaunchStoryboardName</key>\n<string>Launch Screen</string>";
			strnew += lines[i].replace("$plist_launch_screen_name", value) + "\n";
		} else if (lines[i].contains("$pbx_launch_screen_file_reference")) {
			String value = "90DD2D9D24B36E8000717FE1 = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = file.storyboard; path = \"Launch Screen.storyboard\"; sourceTree = \"<group>\"; };";
			strnew += lines[i].replace("$pbx_launch_screen_file_reference", value) + "\n";
		} else if (lines[i].contains("$pbx_launch_screen_copy_files")) {
			String value = "90DD2D9D24B36E8000717FE1 /* Launch Screen.storyboard */,";
			strnew += lines[i].replace("$pbx_launch_screen_copy_files", value) + "\n";
		} else if (lines[i].contains("$pbx_launch_screen_build_phase")) {
			String value = "90DD2D9E24B36E8000717FE1 /* Launch Screen.storyboard in Resources */,";
			strnew += lines[i].replace("$pbx_launch_screen_build_phase", value) + "\n";
		} else if (lines[i].contains("$pbx_launch_screen_build_reference")) {
			String value = "90DD2D9E24B36E8000717FE1 /* Launch Screen.storyboard in Resources */ = {isa = PBXBuildFile; fileRef = 90DD2D9D24B36E8000717FE1 /* Launch Screen.storyboard */; };";
			strnew += lines[i].replace("$pbx_launch_screen_build_reference", value) + "\n";
#ifndef DISABLE_DEPRECATED
		} else if (lines[i].contains("$pbx_launch_image_usage_setting")) {
			strnew += lines[i].replace("$pbx_launch_image_usage_setting", "") + "\n";
#endif
		} else if (lines[i].contains("$launch_screen_image_mode")) {
			int image_scale_mode = p_preset->get("storyboard/image_scale_mode");
			String value;

			switch (image_scale_mode) {
				case 0: {
					String logo_path = GLOBAL_GET("application/boot_splash/image");
					bool is_on = GLOBAL_GET("application/boot_splash/fullsize");
					// If custom logo is not specified, Godot does not scale default one, so we should do the same.
					value = (is_on && logo_path.length() > 0) ? "scaleAspectFit" : "center";
				} break;
				default: {
					value = storyboard_image_scale_mode[image_scale_mode - 1];
				}
			}

			strnew += lines[i].replace("$launch_screen_image_mode", value) + "\n";
		} else if (lines[i].contains("$launch_screen_background_color")) {
			bool use_custom = p_preset->get("storyboard/use_custom_bg_color");
			Color color = use_custom ? p_preset->get("storyboard/custom_bg_color") : GLOBAL_GET("application/boot_splash/bg_color");
			const String value_format = "red=\"$red\" green=\"$green\" blue=\"$blue\" alpha=\"$alpha\"";

			Dictionary value_dictionary;
			value_dictionary["red"] = color.r;
			value_dictionary["green"] = color.g;
			value_dictionary["blue"] = color.b;
			value_dictionary["alpha"] = color.a;
			String value = value_format.format(value_dictionary, "$_");

			strnew += lines[i].replace("$launch_screen_background_color", value) + "\n";
		} else if (lines[i].contains("$pbx_locale_file_reference")) {
			String locale_files;
			Vector<String> translations = GLOBAL_GET("internationalization/locale/translations");
			if (translations.size() > 0) {
				HashSet<String> languages;
				for (const String &E : translations) {
					Ref<Translation> tr = ResourceLoader::load(E);
					if (tr.is_valid() && tr->get_locale() != "en") {
						languages.insert(tr->get_locale());
					}
				}

				int index = 0;
				for (const String &lang : languages) {
					locale_files += "D0BCFE4518AEBDA2004A" + itos(index).pad_zeros(4) + " /* " + lang + " */ = {isa = PBXFileReference; lastKnownFileType = text.plist.strings; name = " + lang + "; path = " + lang + ".lproj/InfoPlist.strings; sourceTree = \"<group>\"; };\n";
					index++;
				}
			}
			strnew += lines[i].replace("$pbx_locale_file_reference", locale_files);
		} else if (lines[i].contains("$pbx_locale_build_reference")) {
			String locale_files;
			Vector<String> translations = GLOBAL_GET("internationalization/locale/translations");
			if (translations.size() > 0) {
				HashSet<String> languages;
				for (const String &E : translations) {
					Ref<Translation> tr = ResourceLoader::load(E);
					if (tr.is_valid() && tr->get_locale() != "en") {
						languages.insert(tr->get_locale());
					}
				}

				int index = 0;
				for (const String &lang : languages) {
					locale_files += "D0BCFE4518AEBDA2004A" + itos(index).pad_zeros(4) + " /* " + lang + " */,\n";
					index++;
				}
			}
			strnew += lines[i].replace("$pbx_locale_build_reference", locale_files);
		} else if (lines[i].contains("$swift_runtime_migration")) {
			String value = !p_config.use_swift_runtime ? "" : "LastSwiftMigration = 1250;";
			strnew += lines[i].replace("$swift_runtime_migration", value) + "\n";
		} else if (lines[i].contains("$swift_runtime_build_settings")) {
			String value = !p_config.use_swift_runtime ? "" : R"(
                     CLANG_ENABLE_MODULES = YES;
                     SWIFT_OBJC_BRIDGING_HEADER = "$binary/dummy.h";
                     SWIFT_VERSION = 5.0;
                     )";
			value = value.replace("$binary", p_config.binary_name);
			strnew += lines[i].replace("$swift_runtime_build_settings", value) + "\n";
		} else if (lines[i].contains("$swift_runtime_fileref")) {
			String value = !p_config.use_swift_runtime ? "" : R"(
                     90B4C2AA2680BC560039117A /* dummy.h */ = {isa = PBXFileReference; lastKnownFileType = sourcecode.c.h; path = "dummy.h"; sourceTree = "<group>"; };
                     90B4C2B52680C7E90039117A /* dummy.swift */ = {isa = PBXFileReference; lastKnownFileType = sourcecode.swift; path = "dummy.swift"; sourceTree = "<group>"; };
                     )";
			strnew += lines[i].replace("$swift_runtime_fileref", value) + "\n";
		} else if (lines[i].contains("$swift_runtime_binary_files")) {
			String value = !p_config.use_swift_runtime ? "" : R"(
                     90B4C2AA2680BC560039117A /* dummy.h */,
                     90B4C2B52680C7E90039117A /* dummy.swift */,
                     )";
			strnew += lines[i].replace("$swift_runtime_binary_files", value) + "\n";
		} else if (lines[i].contains("$swift_runtime_buildfile")) {
			String value = !p_config.use_swift_runtime ? "" : "90B4C2B62680C7E90039117A /* dummy.swift in Sources */ = {isa = PBXBuildFile; fileRef = 90B4C2B52680C7E90039117A /* dummy.swift */; };";
			strnew += lines[i].replace("$swift_runtime_buildfile", value) + "\n";
		} else if (lines[i].contains("$swift_runtime_build_phase")) {
			String value = !p_config.use_swift_runtime ? "" : "90B4C2B62680C7E90039117A /* dummy.swift */,";
			strnew += lines[i].replace("$swift_runtime_build_phase", value) + "\n";
		} else if (lines[i].contains("$priv_collection")) {
			bool section_opened = false;
			for (uint64_t j = 0; j < sizeof(data_collect_type_info) / sizeof(data_collect_type_info[0]); ++j) {
				bool data_collected = p_preset->get(vformat("privacy/collected_data/%s/collected", data_collect_type_info[j].prop_name));
				bool linked = p_preset->get(vformat("privacy/collected_data/%s/linked_to_user", data_collect_type_info[j].prop_name));
				bool tracking = p_preset->get(vformat("privacy/collected_data/%s/used_for_tracking", data_collect_type_info[j].prop_name));
				int purposes = p_preset->get(vformat("privacy/collected_data/%s/collection_purposes", data_collect_type_info[j].prop_name));
				if (data_collected) {
					if (!section_opened) {
						section_opened = true;
						strnew += "\t<key>NSPrivacyCollectedDataTypes</key>\n";
						strnew += "\t<array>\n";
					}
					strnew += "\t\t<dict>\n";
					strnew += "\t\t\t<key>NSPrivacyCollectedDataType</key>\n";
					strnew += vformat("\t\t\t<string>%s</string>\n", data_collect_type_info[j].type_name);
					strnew += "\t\t\t\t<key>NSPrivacyCollectedDataTypeLinked</key>\n";
					if (linked) {
						strnew += "\t\t\t\t<true/>\n";
					} else {
						strnew += "\t\t\t\t<false/>\n";
					}
					strnew += "\t\t\t\t<key>NSPrivacyCollectedDataTypeTracking</key>\n";
					if (tracking) {
						strnew += "\t\t\t\t<true/>\n";
					} else {
						strnew += "\t\t\t\t<false/>\n";
					}
					if (purposes != 0) {
						strnew += "\t\t\t\t<key>NSPrivacyCollectedDataTypePurposes</key>\n";
						strnew += "\t\t\t\t<array>\n";
						for (uint64_t k = 0; k < sizeof(data_collect_purpose_info) / sizeof(data_collect_purpose_info[0]); ++k) {
							if (purposes & (1 << k)) {
								strnew += vformat("\t\t\t\t\t<string>%s</string>\n", data_collect_purpose_info[k].type_name);
							}
						}
						strnew += "\t\t\t\t</array>\n";
					}
					strnew += "\t\t\t</dict>\n";
				}
			}
			if (section_opened) {
				strnew += "\t</array>\n";
			}
		} else if (lines[i].contains("$priv_tracking")) {
			bool tracking = p_preset->get("privacy/tracking_enabled");
			strnew += "\t<key>NSPrivacyTracking</key>\n";
			if (tracking) {
				strnew += "\t<true/>\n";
			} else {
				strnew += "\t<false/>\n";
			}
			Vector<String> tracking_domains = p_preset->get("privacy/tracking_domains");
			if (!tracking_domains.is_empty()) {
				strnew += "\t<key>NSPrivacyTrackingDomains</key>\n";
				strnew += "\t<array>\n";
				for (const String &E : tracking_domains) {
					strnew += "\t\t<string>" + E + "</string>\n";
				}
				strnew += "\t</array>\n";
			}
		} else if (lines[i].contains("$priv_api_types")) {
			strnew += "\t<array>\n";
			for (uint64_t j = 0; j < sizeof(api_info) / sizeof(api_info[0]); ++j) {
				int api_access = p_preset->get(vformat("privacy/%s_access_reasons", api_info[j].prop_name));
				if (api_access != 0) {
					strnew += "\t\t<dict>\n";
					strnew += "\t\t\t<key>NSPrivacyAccessedAPITypeReasons</key>\n";
					strnew += "\t\t\t<array>\n";
					for (int k = 0; k < api_info[j].prop_flag_value.size(); k++) {
						if (api_access & (1 << k)) {
							strnew += vformat("\t\t\t\t<string>%s</string>\n", api_info[j].prop_flag_value[k]);
						}
					}
					strnew += "\t\t\t</array>\n";
					strnew += "\t\t\t<key>NSPrivacyAccessedAPIType</key>\n";
					strnew += vformat("\t\t\t<string>%s</string>\n", api_info[j].type_name);
					strnew += "\t\t</dict>\n";
				}
			}
			strnew += "\t</array>\n";
		} else {
			strnew += lines[i] + "\n";
		}
	}

	// !BAS! I'm assuming the 9 in the original code was a typo. I've added -1 or else it seems to also be adding our terminating zero...
	// should apply the same fix in our macOS export.
	CharString cs = strnew.utf8();
	pfile.resize(cs.size() - 1);
	for (int i = 0; i < cs.size() - 1; i++) {
		pfile.write[i] = cs[i];
	}
}

String EditorExportPlatformIOS::_get_additional_plist_content() {
	Vector<Ref<EditorExportPlugin>> export_plugins = EditorExport::get_singleton()->get_export_plugins();
	String result;
	for (int i = 0; i < export_plugins.size(); ++i) {
		result += export_plugins[i]->get_ios_plist_content();
	}
	return result;
}

String EditorExportPlatformIOS::_get_linker_flags() {
	Vector<Ref<EditorExportPlugin>> export_plugins = EditorExport::get_singleton()->get_export_plugins();
	String result;
	for (int i = 0; i < export_plugins.size(); ++i) {
		String flags = export_plugins[i]->get_ios_linker_flags();
		if (flags.length() == 0) {
			continue;
		}
		if (result.length() > 0) {
			result += ' ';
		}
		result += flags;
	}
	// the flags will be enclosed in quotes, so need to escape them
	return result.replace("\"", "\\\"");
}

String EditorExportPlatformIOS::_get_cpp_code() {
	Vector<Ref<EditorExportPlugin>> export_plugins = EditorExport::get_singleton()->get_export_plugins();
	String result;
	for (int i = 0; i < export_plugins.size(); ++i) {
		result += export_plugins[i]->get_ios_cpp_code();
	}
	return result;
}

void EditorExportPlatformIOS::_blend_and_rotate(Ref<Image> &p_dst, Ref<Image> &p_src, bool p_rot) {
	ERR_FAIL_COND(p_dst.is_null());
	ERR_FAIL_COND(p_src.is_null());

	int sw = p_rot ? p_src->get_height() : p_src->get_width();
	int sh = p_rot ? p_src->get_width() : p_src->get_height();

	int x_pos = (p_dst->get_width() - sw) / 2;
	int y_pos = (p_dst->get_height() - sh) / 2;

	int xs = (x_pos >= 0) ? 0 : -x_pos;
	int ys = (y_pos >= 0) ? 0 : -y_pos;

	if (sw + x_pos > p_dst->get_width()) {
		sw = p_dst->get_width() - x_pos;
	}
	if (sh + y_pos > p_dst->get_height()) {
		sh = p_dst->get_height() - y_pos;
	}

	for (int y = ys; y < sh; y++) {
		for (int x = xs; x < sw; x++) {
			Color sc = p_rot ? p_src->get_pixel(p_src->get_width() - y - 1, x) : p_src->get_pixel(x, y);
			Color dc = p_dst->get_pixel(x_pos + x, y_pos + y);
			dc.r = (double)(sc.a * sc.r + dc.a * (1.0 - sc.a) * dc.r);
			dc.g = (double)(sc.a * sc.g + dc.a * (1.0 - sc.a) * dc.g);
			dc.b = (double)(sc.a * sc.b + dc.a * (1.0 - sc.a) * dc.b);
			dc.a = (double)(sc.a + dc.a * (1.0 - sc.a));
			p_dst->set_pixel(x_pos + x, y_pos + y, dc);
		}
	}
}

Error EditorExportPlatformIOS::_export_icons(const Ref<EditorExportPreset> &p_preset, const String &p_iconset_dir) {
	String json_description = "{\"images\":[";
	String sizes;

	Ref<DirAccess> da = DirAccess::open(p_iconset_dir);
	if (da.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export Icons"), vformat(TTR("Could not open a directory at path \"%s\"."), p_iconset_dir));
		return ERR_CANT_OPEN;
	}

	Color boot_bg_color = GLOBAL_GET("application/boot_splash/bg_color");

	enum IconColorMode {
		ICON_NORMAL,
		ICON_DARK,
		ICON_TINTED,
		ICON_MAX,
	};

	bool first_icon = true;
	for (uint64_t i = 0; i < (sizeof(icon_infos) / sizeof(icon_infos[0])); ++i) {
		for (int color_mode = ICON_NORMAL; color_mode < ICON_MAX; color_mode++) {
			IconInfo info = icon_infos[i];
			int side_size = String(info.actual_size_side).to_int();
			String key = info.preset_key;
			String exp_name = info.export_name;
			if (color_mode == ICON_DARK) {
				key += "_dark";
				exp_name += "_dark";
			} else if (color_mode == ICON_TINTED) {
				key += "_tinted";
				exp_name += "_tinted";
			}
			exp_name += ".png";
			String icon_path = p_preset->get(key);
			bool resize_waning = true;
			if (icon_path.is_empty()) {
				// Load and resize base icon.
				key = "icons/icon_1024x1024";
				if (color_mode == ICON_DARK) {
					key += "_dark";
				} else if (color_mode == ICON_TINTED) {
					key += "_tinted";
				}
				icon_path = p_preset->get(key);
				resize_waning = false;
			}
			if (icon_path.is_empty()) {
				if (color_mode != ICON_NORMAL) {
					continue;
				}
				// Resize main app icon.
				icon_path = GLOBAL_GET("application/config/icon");
				Ref<Image> img = memnew(Image);
				Error err = ImageLoader::load_image(icon_path, img);
				if (err != OK) {
					add_message(EXPORT_MESSAGE_ERROR, TTR("Export Icons"), vformat("Invalid icon (%s): '%s'.", info.preset_key, icon_path));
					return ERR_UNCONFIGURED;
				} else if (info.force_opaque && img->detect_alpha() != Image::ALPHA_NONE) {
					img->resize(side_size, side_size, (Image::Interpolation)(p_preset->get("application/icon_interpolation").operator int()));
					Ref<Image> new_img = Image::create_empty(side_size, side_size, false, Image::FORMAT_RGBA8);
					new_img->fill(boot_bg_color);
					_blend_and_rotate(new_img, img, false);
					err = new_img->save_png(p_iconset_dir + exp_name);
				} else {
					img->resize(side_size, side_size, (Image::Interpolation)(p_preset->get("application/icon_interpolation").operator int()));
					err = img->save_png(p_iconset_dir + exp_name);
				}
				if (err) {
					add_message(EXPORT_MESSAGE_ERROR, TTR("Export Icons"), vformat("Failed to export icon (%s): '%s'.", info.preset_key, icon_path));
					return err;
				}
			} else {
				// Load custom icon and resize if required.
				Ref<Image> img = memnew(Image);
				Error err = ImageLoader::load_image(icon_path, img);
				if (err != OK) {
					add_message(EXPORT_MESSAGE_ERROR, TTR("Export Icons"), vformat("Invalid icon (%s): '%s'.", info.preset_key, icon_path));
					return ERR_UNCONFIGURED;
				} else if (info.force_opaque && img->detect_alpha() != Image::ALPHA_NONE) {
					if (resize_waning) {
						add_message(EXPORT_MESSAGE_WARNING, TTR("Export Icons"), vformat("Icon (%s) must be opaque.", info.preset_key));
					}
					img->resize(side_size, side_size, (Image::Interpolation)(p_preset->get("application/icon_interpolation").operator int()));
					Ref<Image> new_img = Image::create_empty(side_size, side_size, false, Image::FORMAT_RGBA8);
					new_img->fill(boot_bg_color);
					_blend_and_rotate(new_img, img, false);
					err = new_img->save_png(p_iconset_dir + exp_name);
				} else if (img->get_width() != side_size || img->get_height() != side_size) {
					if (resize_waning) {
						add_message(EXPORT_MESSAGE_WARNING, TTR("Export Icons"), vformat("Icon (%s): '%s' has incorrect size %s and was automatically resized to %s.", info.preset_key, icon_path, img->get_size(), Vector2i(side_size, side_size)));
					}
					img->resize(side_size, side_size, (Image::Interpolation)(p_preset->get("application/icon_interpolation").operator int()));
					err = img->save_png(p_iconset_dir + exp_name);
				} else if (!icon_path.ends_with(".png")) {
					err = img->save_png(p_iconset_dir + exp_name);
				} else {
					err = da->copy(icon_path, p_iconset_dir + exp_name);
				}

				if (err) {
					add_message(EXPORT_MESSAGE_ERROR, TTR("Export Icons"), vformat("Failed to export icon (%s): '%s'.", info.preset_key, icon_path));
					return err;
				}
			}
			sizes += String(info.actual_size_side) + "\n";
			if (first_icon) {
				first_icon = false;
			} else {
				json_description += ",";
			}
			json_description += String("{");
			if (color_mode != ICON_NORMAL) {
				json_description += String("\"appearances\":[{");
				json_description += String("\"appearance\":\"luminosity\",");
				if (color_mode == ICON_DARK) {
					json_description += String("\"value\":\"dark\"");
				} else if (color_mode == ICON_TINTED) {
					json_description += String("\"value\":\"tinted\"");
				}
				json_description += String("}],");
			}
			json_description += String("\"idiom\":") + "\"" + info.idiom + "\",";
			json_description += String("\"platform\":\"ios\",");
			json_description += String("\"size\":") + "\"" + info.unscaled_size + "\",";
			if (String(info.scale) != "1x") {
				json_description += String("\"scale\":") + "\"" + info.scale + "\",";
			}
			json_description += String("\"filename\":") + "\"" + exp_name + "\"";
			json_description += String("}");
		}
	}
	json_description += "],\"info\":{\"author\":\"xcode\",\"version\":1}}";

	Ref<FileAccess> json_file = FileAccess::open(p_iconset_dir + "Contents.json", FileAccess::WRITE);
	if (json_file.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export Icons"), vformat(TTR("Could not write to a file at path \"%s\"."), p_iconset_dir + "Contents.json"));
		return ERR_CANT_CREATE;
	}

	CharString json_utf8 = json_description.utf8();
	json_file->store_buffer((const uint8_t *)json_utf8.get_data(), json_utf8.length());

	Ref<FileAccess> sizes_file = FileAccess::open(p_iconset_dir + "sizes", FileAccess::WRITE);
	if (sizes_file.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export Icons"), vformat(TTR("Could not write to a file at path \"%s\"."), p_iconset_dir + "sizes"));
		return ERR_CANT_CREATE;
	}

	CharString sizes_utf8 = sizes.utf8();
	sizes_file->store_buffer((const uint8_t *)sizes_utf8.get_data(), sizes_utf8.length());

	return OK;
}

Error EditorExportPlatformIOS::_export_loading_screen_file(const Ref<EditorExportPreset> &p_preset, const String &p_dest_dir) {
	const String custom_launch_image_2x = p_preset->get("storyboard/custom_image@2x");
	const String custom_launch_image_3x = p_preset->get("storyboard/custom_image@3x");

	if (custom_launch_image_2x.length() > 0 && custom_launch_image_3x.length() > 0) {
		Ref<Image> image;
		String image_path = p_dest_dir.path_join("splash@2x.png");
		image.instantiate();
		Error err = ImageLoader::load_image(custom_launch_image_2x, image);

		if (err) {
			image.unref();
			return err;
		}

		if (image->save_png(image_path) != OK) {
			return ERR_FILE_CANT_WRITE;
		}

		image.unref();
		image_path = p_dest_dir.path_join("splash@3x.png");
		image.instantiate();
		err = ImageLoader::load_image(custom_launch_image_3x, image);

		if (err) {
			image.unref();
			return err;
		}

		if (image->save_png(image_path) != OK) {
			return ERR_FILE_CANT_WRITE;
		}
	} else {
		Ref<Image> splash;

		const String splash_path = GLOBAL_GET("application/boot_splash/image");

		if (!splash_path.is_empty()) {
			splash.instantiate();
			const Error err = ImageLoader::load_image(splash_path, splash);
			if (err) {
				splash.unref();
			}
		}

		if (splash.is_null()) {
			splash.instantiate(boot_splash_png);
		}

		// Using same image for both @2x and @3x
		// because Godot's own boot logo uses single image for all resolutions.
		// Also not using @1x image, because devices using this image variant
		// are not supported by iOS 9, which is minimal target.
		const String splash_png_path_2x = p_dest_dir.path_join("splash@2x.png");
		const String splash_png_path_3x = p_dest_dir.path_join("splash@3x.png");

		if (splash->save_png(splash_png_path_2x) != OK) {
			return ERR_FILE_CANT_WRITE;
		}

		if (splash->save_png(splash_png_path_3x) != OK) {
			return ERR_FILE_CANT_WRITE;
		}
	}

	return OK;
}

Error EditorExportPlatformIOS::_walk_dir_recursive(Ref<DirAccess> &p_da, FileHandler p_handler, void *p_userdata) {
	Vector<String> dirs;
	String current_dir = p_da->get_current_dir();
	p_da->list_dir_begin();
	String path = p_da->get_next();
	while (!path.is_empty()) {
		if (p_da->current_is_dir()) {
			if (path != "." && path != "..") {
				dirs.push_back(path);
			}
		} else {
			Error err = p_handler(current_dir.path_join(path), p_userdata);
			if (err) {
				p_da->list_dir_end();
				return err;
			}
		}
		path = p_da->get_next();
	}
	p_da->list_dir_end();

	for (int i = 0; i < dirs.size(); ++i) {
		p_da->change_dir(dirs[i]);
		Error err = _walk_dir_recursive(p_da, p_handler, p_userdata);
		p_da->change_dir("..");
		if (err) {
			return err;
		}
	}

	return OK;
}

struct CodesignData {
	const Ref<EditorExportPreset> &preset;
	bool debug = false;

	CodesignData(const Ref<EditorExportPreset> &p_preset, bool p_debug) :
			preset(p_preset),
			debug(p_debug) {
	}
};

Error EditorExportPlatformIOS::_codesign(String p_file, void *p_userdata) {
	if (p_file.ends_with(".dylib")) {
		CodesignData *data = static_cast<CodesignData *>(p_userdata);
		print_line(String("Signing ") + p_file);

		String sign_id;
		if (data->debug) {
			sign_id = data->preset->get("application/code_sign_identity_debug").operator String().is_empty() ? "iPhone Developer" : data->preset->get("application/code_sign_identity_debug");
		} else {
			sign_id = data->preset->get("application/code_sign_identity_release").operator String().is_empty() ? "iPhone Distribution" : data->preset->get("application/code_sign_identity_release");
		}

		List<String> codesign_args;
		codesign_args.push_back("-f");
		codesign_args.push_back("-s");
		codesign_args.push_back(sign_id);
		codesign_args.push_back(p_file);
		String str;
		Error err = OS::get_singleton()->execute("codesign", codesign_args, &str, nullptr, true);
		print_verbose("codesign (" + p_file + "):\n" + str);

		return err;
	}
	return OK;
}

struct PbxId {
private:
	static char _hex_char(uint8_t four_bits) {
		if (four_bits < 10) {
			return ('0' + four_bits);
		}
		return 'A' + (four_bits - 10);
	}

	static String _hex_pad(uint32_t num) {
		Vector<char> ret;
		ret.resize(sizeof(num) * 2);
		for (uint64_t i = 0; i < sizeof(num) * 2; ++i) {
			uint8_t four_bits = (num >> (sizeof(num) * 8 - (i + 1) * 4)) & 0xF;
			ret.write[i] = _hex_char(four_bits);
		}
		return String::utf8(ret.ptr(), ret.size());
	}

public:
	uint32_t high_bits;
	uint32_t mid_bits;
	uint32_t low_bits;

	String str() const {
		return _hex_pad(high_bits) + _hex_pad(mid_bits) + _hex_pad(low_bits);
	}

	PbxId &operator++() {
		low_bits++;
		if (!low_bits) {
			mid_bits++;
			if (!mid_bits) {
				high_bits++;
			}
		}

		return *this;
	}
};

struct ExportLibsData {
	Vector<String> lib_paths;
	String dest_dir;
};

bool EditorExportPlatformIOS::_archive_has_arm64(const String &p_path, uint32_t *r_cputype, uint32_t *r_cpusubtype) const {
	bool has_arm64_image = false;
	if (FileAccess::exists(p_path)) {
		if (LipO::is_lipo(p_path)) {
			// LipO.
			Ref<LipO> lipo;
			lipo.instantiate();
			if (lipo->open_file(p_path)) {
				for (int i = 0; i < lipo->get_arch_count(); i++) {
					if (lipo->get_arch_cputype(i) == 0x100000c && lipo->get_arch_cpusubtype(i) == 0) {
						has_arm64_image = true;
						break;
					}
				}
			}
			lipo->close();
		} else {
			// Single architecture archive.
			Ref<FileAccess> sim_f = FileAccess::open(p_path, FileAccess::READ);
			if (sim_f.is_valid()) {
				char magic[9] = {};
				sim_f->get_buffer((uint8_t *)&magic[0], 8);
				if (String(magic) == String("!<arch>\n")) {
					while (!sim_f->eof_reached()) {
						// Read file metadata.
						char name_short[17] = {};
						char size_short[11] = {};
						sim_f->get_buffer((uint8_t *)&name_short[0], 16);
						sim_f->seek(sim_f->get_position() + 12 + 6 + 6 + 8); // Skip modification time, owner ID, group ID, file mode.
						sim_f->get_buffer((uint8_t *)&size_short[0], 10);
						sim_f->seek(sim_f->get_position() + 2); // Skip end marker.

						int64_t file_size = String(size_short).to_int();
						int64_t next_off = sim_f->get_position() + file_size;

						String name = String(name_short); // Skip extended name.
						if (name.is_empty() || file_size == 0) {
							break;
						}
						if (name.begins_with("#1/")) {
							int64_t name_len = String(name_short).replace("#1/", "").to_int();
							sim_f->seek(sim_f->get_position() + name_len);
						}

						// Read file content.
						uint32_t obj_magic = sim_f->get_32();

						bool swap = (obj_magic == 0xcffaedfe || obj_magic == 0xcefaedfe);
						if (obj_magic == 0xcefaedfe || obj_magic == 0xfeedface || obj_magic == 0xcffaedfe || obj_magic == 0xfeedfacf) {
							uint32_t cputype = sim_f->get_32();
							uint32_t cpusubtype = sim_f->get_32();
							if (swap) {
								cputype = BSWAP32(cputype);
								cpusubtype = BSWAP32(cpusubtype);
							}
							if (r_cputype) {
								*r_cputype = cputype;
							}
							if (r_cpusubtype) {
								*r_cpusubtype = cpusubtype;
							}
							if (cputype == 0x100000c && cpusubtype == 0) {
								has_arm64_image = true;
							}
							break;
						}
						sim_f->seek(next_off);
					}
				}
				sim_f->close();
			}
		}
	}
	return has_arm64_image;
}

int EditorExportPlatformIOS::_archive_convert_to_simulator(const String &p_path) const {
	int commands_patched = 0;
	Ref<FileAccess> sim_f = FileAccess::open(p_path, FileAccess::READ_WRITE);
	if (sim_f.is_valid()) {
		char magic[9] = {};
		sim_f->get_buffer((uint8_t *)&magic[0], 8);
		if (String(magic) == String("!<arch>\n")) {
			while (!sim_f->eof_reached()) {
				// Read file metadata.
				char name_short[17] = {};
				char size_short[11] = {};
				sim_f->get_buffer((uint8_t *)&name_short[0], 16);
				sim_f->seek(sim_f->get_position() + 12 + 6 + 6 + 8); // Skip modification time, owner ID, group ID, file mode.
				sim_f->get_buffer((uint8_t *)&size_short[0], 10);
				sim_f->seek(sim_f->get_position() + 2); // Skip end marker.

				int64_t file_size = String(size_short).to_int();
				int64_t next_off = sim_f->get_position() + file_size;

				String name = String(name_short); // Skip extended name.
				if (name.is_empty() || file_size == 0) {
					break;
				}
				if (name.begins_with("#1/")) {
					int64_t name_len = String(name_short).replace("#1/", "").to_int();
					sim_f->seek(sim_f->get_position() + name_len);
				}

				// Read file content.
				uint32_t obj_magic = sim_f->get_32();

				bool swap = (obj_magic == 0xcffaedfe || obj_magic == 0xcefaedfe);
				if (obj_magic == 0xcefaedfe || obj_magic == 0xfeedface || obj_magic == 0xcffaedfe || obj_magic == 0xfeedfacf) {
					uint32_t cputype = sim_f->get_32();
					uint32_t cpusubtype = sim_f->get_32();
					uint32_t filetype = sim_f->get_32();
					uint32_t ncmds = sim_f->get_32();
					sim_f->get_32(); // Commands total size.
					sim_f->get_32(); // Commands flags.
					if (obj_magic == 0xcffaedfe || obj_magic == 0xfeedfacf) {
						sim_f->get_32(); // Reserved, 64-bit only.
					}
					if (swap) {
						ncmds = BSWAP32(ncmds);
						cputype = BSWAP32(cputype);
						cpusubtype = BSWAP32(cpusubtype);
						filetype = BSWAP32(filetype);
					}
					if (cputype == 0x100000C && cpusubtype == 0 && filetype == 1) {
						// ARM64, object file.
						for (uint32_t i = 0; i < ncmds; i++) {
							int64_t cmdofs = sim_f->get_position();
							uint32_t cmdid = sim_f->get_32();
							uint32_t cmdsize = sim_f->get_32();
							if (swap) {
								cmdid = BSWAP32(cmdid);
								cmdsize = BSWAP32(cmdsize);
							}
							if (cmdid == MachO::LoadCommandID::LC_BUILD_VERSION) {
								int64_t platform = sim_f->get_32();
								if (swap) {
									platform = BSWAP32(platform);
								}
								if (platform == MachO::PlatformID::PLATFORM_IOS) {
									sim_f->seek(cmdofs + 4 + 4);
									uint32_t new_id = MachO::PlatformID::PLATFORM_IOSSIMULATOR;
									if (swap) {
										new_id = BSWAP32(new_id);
									}
									sim_f->store_32(new_id);
									commands_patched++;
								}
							}
							sim_f->seek(cmdofs + cmdsize);
						}
					}
				}
				sim_f->seek(next_off);
			}
		}
		sim_f->close();
	}
	return commands_patched;
}

void EditorExportPlatformIOS::_check_xcframework_content(const String &p_path, int &r_total_libs, int &r_static_libs, int &r_dylibs, int &r_frameworks) const {
	Ref<PList> plist;
	plist.instantiate();
	plist->load_file(p_path.path_join("Info.plist"));
	Ref<PListNode> root_node = plist->get_root();
	if (root_node.is_null()) {
		return;
	}
	Dictionary root = root_node->get_value();
	if (!root.has("AvailableLibraries")) {
		return;
	}
	Ref<PListNode> libs_node = root["AvailableLibraries"];
	if (libs_node.is_null()) {
		return;
	}
	Array libs = libs_node->get_value();
	r_total_libs = libs.size();
	for (int j = 0; j < libs.size(); j++) {
		Ref<PListNode> lib_node = libs[j];
		if (lib_node.is_null()) {
			return;
		}
		Dictionary lib = lib_node->get_value();
		if (lib.has("BinaryPath")) {
			Ref<PListNode> path_node = lib["BinaryPath"];
			if (path_node.is_valid()) {
				String path = path_node->get_value();
				if (path.ends_with(".a")) {
					r_static_libs++;
				}
				if (path.ends_with(".dylib")) {
					r_dylibs++;
				}
				if (path.ends_with(".framework")) {
					r_frameworks++;
				}
			}
		}
	}
}

Error EditorExportPlatformIOS::_convert_to_framework(const String &p_source, const String &p_destination, const String &p_id) const {
	print_line("Converting to .framework", p_source, " -> ", p_destination);

	Ref<DirAccess> da = DirAccess::create_for_path(p_source);
	if (da.is_null()) {
		return ERR_CANT_OPEN;
	}

	Ref<DirAccess> filesystem_da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (filesystem_da.is_null()) {
		return ERR_CANT_OPEN;
	}

	if (!filesystem_da->dir_exists(p_destination)) {
		Error make_dir_err = filesystem_da->make_dir_recursive(p_destination);
		if (make_dir_err) {
			return make_dir_err;
		}
	}

	String asset = p_source.ends_with("/") ? p_source.left(p_source.length() - 1) : p_source;
	if (asset.ends_with(".xcframework")) {
		Ref<PList> plist;
		plist.instantiate();
		plist->load_file(p_source.path_join("Info.plist"));
		Ref<PListNode> root_node = plist->get_root();
		if (root_node.is_null()) {
			return ERR_CANT_OPEN;
		}
		Dictionary root = root_node->get_value();
		if (!root.has("AvailableLibraries")) {
			return ERR_CANT_OPEN;
		}
		Ref<PListNode> libs_node = root["AvailableLibraries"];
		if (libs_node.is_null()) {
			return ERR_CANT_OPEN;
		}
		Array libs = libs_node->get_value();
		for (int j = 0; j < libs.size(); j++) {
			Ref<PListNode> lib_node = libs[j];
			if (lib_node.is_null()) {
				return ERR_CANT_OPEN;
			}
			Dictionary lib = lib_node->get_value();
			if (lib.has("BinaryPath") && lib.has("LibraryPath") && lib.has("LibraryIdentifier")) {
				Ref<PListNode> bpath_node = lib["BinaryPath"];
				Ref<PListNode> lpath_node = lib["LibraryPath"];
				Ref<PListNode> lid_node = lib["LibraryIdentifier"];
				if (bpath_node.is_valid() && lpath_node.is_valid() && lid_node.is_valid()) {
					String binary_path = bpath_node->get_value();
					String library_identifier = lid_node->get_value();

					String file_name = binary_path.get_basename().get_file();
					String framework_name = file_name + ".framework";

					bpath_node->data_string = framework_name.utf8();
					lpath_node->data_string = framework_name.utf8();
					if (!filesystem_da->dir_exists(p_destination.path_join(library_identifier))) {
						filesystem_da->make_dir_recursive(p_destination.path_join(library_identifier));
					}
					_convert_to_framework(p_source.path_join(library_identifier).path_join(binary_path), p_destination.path_join(library_identifier).path_join(framework_name), p_id);
					if (lib.has("DebugSymbolsPath")) {
						Ref<PListNode> dpath_node = lib["DebugSymbolsPath"];
						if (dpath_node.is_valid()) {
							String dpath = dpath_node->get_value();
							if (da->dir_exists(p_source.path_join(library_identifier).path_join(dpath))) {
								da->copy_dir(p_source.path_join(library_identifier).path_join(dpath), p_destination.path_join(library_identifier).path_join("dSYMs"));
							}
						}
					}
				}
			}
		}
		String info_plist = plist->save_text();

		Ref<FileAccess> f = FileAccess::open(p_destination.path_join("Info.plist"), FileAccess::WRITE);
		if (f.is_valid()) {
			f->store_string(info_plist);
		}
	} else {
		String file_name = p_destination.get_basename().get_file();
		String framework_name = file_name + ".framework";

		da->copy(p_source, p_destination.path_join(file_name));

		// Performing `install_name_tool -id @rpath/{name}.framework/{name} ./{name}` on dylib
		{
			List<String> install_name_args;
			install_name_args.push_back("-id");
			install_name_args.push_back(String("@rpath").path_join(framework_name).path_join(file_name));
			install_name_args.push_back(p_destination.path_join(file_name));

			OS::get_singleton()->execute("install_name_tool", install_name_args);
		}

		// Creating Info.plist
		{
			String lib_clean_name = file_name;
			for (int i = 0; i < lib_clean_name.length(); i++) {
				if (!is_ascii_alphanumeric_char(lib_clean_name[i]) && lib_clean_name[i] != '.' && lib_clean_name[i] != '-') {
					lib_clean_name[i] = '-';
				}
			}
			String info_plist_format = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
									   "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
									   "<plist version=\"1.0\">\n"
									   "  <dict>\n"
									   "    <key>CFBundleShortVersionString</key>\n"
									   "    <string>1.0</string>\n"
									   "    <key>CFBundleIdentifier</key>\n"
									   "    <string>$id.framework.$cl_name</string>\n"
									   "    <key>CFBundleName</key>\n"
									   "    <string>$name</string>\n"
									   "    <key>CFBundleExecutable</key>\n"
									   "    <string>$name</string>\n"
									   "    <key>DTPlatformName</key>\n"
									   "    <string>iphoneos</string>\n"
									   "    <key>CFBundleInfoDictionaryVersion</key>\n"
									   "    <string>6.0</string>\n"
									   "    <key>CFBundleVersion</key>\n"
									   "    <string>1</string>\n"
									   "    <key>CFBundlePackageType</key>\n"
									   "    <string>FMWK</string>\n"
									   "    <key>MinimumOSVersion</key>\n"
									   "    <string>12.0</string>\n"
									   "  </dict>\n"
									   "</plist>";

			String info_plist = info_plist_format.replace("$id", p_id).replace("$name", file_name).replace("$cl_name", lib_clean_name);

			Ref<FileAccess> f = FileAccess::open(p_destination.path_join("Info.plist"), FileAccess::WRITE);
			if (f.is_valid()) {
				f->store_string(info_plist);
			}
		}
	}

	return OK;
}

void EditorExportPlatformIOS::_add_assets_to_project(const String &p_out_dir, const Ref<EditorExportPreset> &p_preset, Vector<uint8_t> &p_project_data, const Vector<IOSExportAsset> &p_additional_assets) {
	// that is just a random number, we just need Godot IDs not to clash with
	// existing IDs in the project.
	PbxId current_id = { 0x58938401, 0, 0 };
	String pbx_files;
	String pbx_frameworks_build;
	String pbx_frameworks_refs;
	String pbx_resources_build;
	String pbx_resources_refs;
	String pbx_embeded_frameworks;

	const String file_info_format = String("$build_id = {isa = PBXBuildFile; fileRef = $ref_id; };\n") +
			"$ref_id = {isa = PBXFileReference; lastKnownFileType = $file_type; name = \"$name\"; path = \"$file_path\"; sourceTree = \"<group>\"; };\n";

	for (int i = 0; i < p_additional_assets.size(); ++i) {
		String additional_asset_info_format = file_info_format;

		String build_id = (++current_id).str();
		String ref_id = (++current_id).str();
		String framework_id = "";

		const IOSExportAsset &asset = p_additional_assets[i];

		String type;
		if (asset.exported_path.ends_with(".framework")) {
			if (asset.should_embed) {
				additional_asset_info_format += "$framework_id = {isa = PBXBuildFile; fileRef = $ref_id; settings = {ATTRIBUTES = (CodeSignOnCopy, ); }; };\n";
				framework_id = (++current_id).str();
				pbx_embeded_frameworks += framework_id + ",\n";
			}

			type = "wrapper.framework";
		} else if (asset.exported_path.ends_with(".xcframework")) {
			int total_libs = 0;
			int static_libs = 0;
			int dylibs = 0;
			int frameworks = 0;
			_check_xcframework_content(p_out_dir.path_join(asset.exported_path), total_libs, static_libs, dylibs, frameworks);
			if (asset.should_embed && static_libs != total_libs) {
				additional_asset_info_format += "$framework_id = {isa = PBXBuildFile; fileRef = $ref_id; settings = {ATTRIBUTES = (CodeSignOnCopy, ); }; };\n";
				framework_id = (++current_id).str();
				pbx_embeded_frameworks += framework_id + ",\n";
			}

			type = "wrapper.xcframework";
		} else if (asset.exported_path.ends_with(".dylib")) {
			type = "compiled.mach-o.dylib";
		} else if (asset.exported_path.ends_with(".a")) {
			type = "archive.ar";
		} else {
			type = "file";
		}

		String &pbx_build = asset.is_framework ? pbx_frameworks_build : pbx_resources_build;
		String &pbx_refs = asset.is_framework ? pbx_frameworks_refs : pbx_resources_refs;

		if (pbx_build.length() > 0) {
			pbx_build += ",\n";
			pbx_refs += ",\n";
		}
		pbx_build += build_id;
		pbx_refs += ref_id;

		Dictionary format_dict;
		format_dict["build_id"] = build_id;
		format_dict["ref_id"] = ref_id;
		format_dict["name"] = asset.exported_path.get_file();
		format_dict["file_path"] = asset.exported_path;
		format_dict["file_type"] = type;
		if (framework_id.length() > 0) {
			format_dict["framework_id"] = framework_id;
		}
		pbx_files += additional_asset_info_format.format(format_dict, "$_");
	}

	// Note, frameworks like gamekit are always included in our project.pbxprof file
	// even if turned off in capabilities.

	String str = String::utf8((const char *)p_project_data.ptr(), p_project_data.size());
	str = str.replace("$additional_pbx_files", pbx_files);
	str = str.replace("$additional_pbx_frameworks_build", pbx_frameworks_build);
	str = str.replace("$additional_pbx_frameworks_refs", pbx_frameworks_refs);
	str = str.replace("$additional_pbx_resources_build", pbx_resources_build);
	str = str.replace("$additional_pbx_resources_refs", pbx_resources_refs);
	str = str.replace("$pbx_embeded_frameworks", pbx_embeded_frameworks);

	CharString cs = str.utf8();
	p_project_data.resize(cs.size() - 1);
	for (int i = 0; i < cs.size() - 1; i++) {
		p_project_data.write[i] = cs[i];
	}
}

Error EditorExportPlatformIOS::_copy_asset(const Ref<EditorExportPreset> &p_preset, const String &p_out_dir, const String &p_asset, const String *p_custom_file_name, bool p_is_framework, bool p_should_embed, Vector<IOSExportAsset> &r_exported_assets) {
	String binary_name = p_out_dir.get_file().get_basename();

	Ref<DirAccess> da = DirAccess::create_for_path(p_asset);
	if (da.is_null()) {
		ERR_FAIL_V_MSG(ERR_CANT_CREATE, "Can't open directory: " + p_asset + ".");
	}
	bool file_exists = da->file_exists(p_asset);
	bool dir_exists = da->dir_exists(p_asset);
	if (!file_exists && !dir_exists) {
		return ERR_FILE_NOT_FOUND;
	}

	String base_dir = p_asset.get_base_dir().replace("res://", "").replace(".godot/mono/temp/bin/", "");
	String asset = p_asset.ends_with("/") ? p_asset.left(p_asset.length() - 1) : p_asset;
	String destination_dir;
	String destination;
	String asset_path;

	Ref<DirAccess> filesystem_da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND_V_MSG(filesystem_da.is_null(), ERR_CANT_CREATE, "Cannot create DirAccess for path '" + p_out_dir + "'.");

	if (p_is_framework && asset.ends_with(".dylib")) {
		// For iOS we need to turn .dylib into .framework
		// to be able to send application to AppStore
		asset_path = String("dylibs").path_join(base_dir);

		String file_name;

		if (!p_custom_file_name) {
			file_name = p_asset.get_basename().get_file();
		} else {
			file_name = *p_custom_file_name;
		}

		String framework_name = file_name + ".framework";

		asset_path = asset_path.path_join(framework_name);
		destination_dir = p_out_dir.path_join(asset_path);
		destination = destination_dir;

		// Convert to framework and copy.
		Error err = _convert_to_framework(p_asset, destination, p_preset->get("application/bundle_identifier"));
		if (err) {
			return err;
		}
	} else if (p_is_framework && asset.ends_with(".xcframework")) {
		// For iOS we need to turn .dylib inside .xcframework
		// into .framework to be able to send application to AppStore

		int total_libs = 0;
		int static_libs = 0;
		int dylibs = 0;
		int frameworks = 0;
		_check_xcframework_content(p_asset, total_libs, static_libs, dylibs, frameworks);

		asset_path = String("dylibs").path_join(base_dir);
		String file_name;

		if (!p_custom_file_name) {
			file_name = p_asset.get_file();
		} else {
			file_name = *p_custom_file_name;
		}

		asset_path = asset_path.path_join(file_name);
		destination_dir = p_out_dir.path_join(asset_path);
		destination = destination_dir;

		if (dylibs > 0) {
			// Convert to framework and copy.
			Error err = _convert_to_framework(p_asset, destination, p_preset->get("application/bundle_identifier"));
			if (err) {
				return err;
			}
		} else {
			// Copy as is.
			if (!filesystem_da->dir_exists(destination_dir)) {
				Error make_dir_err = filesystem_da->make_dir_recursive(destination_dir);
				if (make_dir_err) {
					return make_dir_err;
				}
			}
			Error err = dir_exists ? da->copy_dir(p_asset, destination) : da->copy(p_asset, destination);
			if (err) {
				return err;
			}
		}
	} else if (p_is_framework && asset.ends_with(".framework")) {
		// Framework.
		asset_path = String("dylibs").path_join(base_dir);

		String file_name;

		if (!p_custom_file_name) {
			file_name = p_asset.get_file();
		} else {
			file_name = *p_custom_file_name;
		}

		asset_path = asset_path.path_join(file_name);
		destination_dir = p_out_dir.path_join(asset_path);
		destination = destination_dir;

		// Copy as is.
		if (!filesystem_da->dir_exists(destination_dir)) {
			Error make_dir_err = filesystem_da->make_dir_recursive(destination_dir);
			if (make_dir_err) {
				return make_dir_err;
			}
		}
		Error err = dir_exists ? da->copy_dir(p_asset, destination) : da->copy(p_asset, destination);
		if (err) {
			return err;
		}
	} else {
		// Unknown resource.
		asset_path = base_dir;

		String file_name;

		if (!p_custom_file_name) {
			file_name = p_asset.get_file();
		} else {
			file_name = *p_custom_file_name;
		}

		destination_dir = p_out_dir.path_join(asset_path);
		asset_path = asset_path.path_join(file_name);
		destination = p_out_dir.path_join(asset_path);

		// Copy as is.
		if (!filesystem_da->dir_exists(destination_dir)) {
			Error make_dir_err = filesystem_da->make_dir_recursive(destination_dir);
			if (make_dir_err) {
				return make_dir_err;
			}
		}
		Error err = dir_exists ? da->copy_dir(p_asset, destination) : da->copy(p_asset, destination);
		if (err) {
			return err;
		}
	}

	if (asset_path.ends_with("/")) {
		asset_path = asset_path.left(asset_path.length() - 1);
	}
	IOSExportAsset exported_asset = { binary_name.path_join(asset_path), p_is_framework, p_should_embed };
	r_exported_assets.push_back(exported_asset);

	return OK;
}

Error EditorExportPlatformIOS::_export_additional_assets(const Ref<EditorExportPreset> &p_preset, const String &p_out_dir, const Vector<String> &p_assets, bool p_is_framework, bool p_should_embed, Vector<IOSExportAsset> &r_exported_assets) {
	for (int f_idx = 0; f_idx < p_assets.size(); ++f_idx) {
		const String &asset = p_assets[f_idx];
		if (asset.begins_with("res://")) {
			Error err = _copy_asset(p_preset, p_out_dir, asset, nullptr, p_is_framework, p_should_embed, r_exported_assets);
			ERR_FAIL_COND_V(err != OK, err);
		} else if (asset.is_absolute_path() && ProjectSettings::get_singleton()->localize_path(asset).begins_with("res://")) {
			Error err = _copy_asset(p_preset, p_out_dir, ProjectSettings::get_singleton()->localize_path(asset), nullptr, p_is_framework, p_should_embed, r_exported_assets);
			ERR_FAIL_COND_V(err != OK, err);
		} else {
			// either SDK-builtin or already a part of the export template
			IOSExportAsset exported_asset = { asset, p_is_framework, p_should_embed };
			r_exported_assets.push_back(exported_asset);
		}
	}

	return OK;
}

Error EditorExportPlatformIOS::_export_additional_assets(const Ref<EditorExportPreset> &p_preset, const String &p_out_dir, const Vector<SharedObject> &p_libraries, Vector<IOSExportAsset> &r_exported_assets) {
	Vector<Ref<EditorExportPlugin>> export_plugins = EditorExport::get_singleton()->get_export_plugins();
	for (int i = 0; i < export_plugins.size(); i++) {
		Vector<String> linked_frameworks = export_plugins[i]->get_ios_frameworks();
		Error err = _export_additional_assets(p_preset, p_out_dir, linked_frameworks, true, false, r_exported_assets);
		ERR_FAIL_COND_V(err, err);

		Vector<String> embedded_frameworks = export_plugins[i]->get_ios_embedded_frameworks();
		err = _export_additional_assets(p_preset, p_out_dir, embedded_frameworks, true, true, r_exported_assets);
		ERR_FAIL_COND_V(err, err);

		Vector<String> project_static_libs = export_plugins[i]->get_ios_project_static_libs();
		for (int j = 0; j < project_static_libs.size(); j++) {
			project_static_libs.write[j] = project_static_libs[j].get_file(); // Only the file name as it's copied to the project
		}
		err = _export_additional_assets(p_preset, p_out_dir, project_static_libs, true, false, r_exported_assets);
		ERR_FAIL_COND_V(err, err);

		Vector<String> ios_bundle_files = export_plugins[i]->get_ios_bundle_files();
		err = _export_additional_assets(p_preset, p_out_dir, ios_bundle_files, false, false, r_exported_assets);
		ERR_FAIL_COND_V(err, err);
	}

	Vector<String> library_paths;
	for (int i = 0; i < p_libraries.size(); ++i) {
		library_paths.push_back(p_libraries[i].path);
	}
	Error err = _export_additional_assets(p_preset, p_out_dir, library_paths, true, true, r_exported_assets);
	ERR_FAIL_COND_V(err, err);

	return OK;
}

Vector<String> EditorExportPlatformIOS::_get_preset_architectures(const Ref<EditorExportPreset> &p_preset) const {
	Vector<ExportArchitecture> all_archs = _get_supported_architectures();
	Vector<String> enabled_archs;
	for (int i = 0; i < all_archs.size(); ++i) {
		bool is_enabled = p_preset->get("architectures/" + all_archs[i].name);
		if (is_enabled) {
			enabled_archs.push_back(all_archs[i].name);
		}
	}
	return enabled_archs;
}

Error EditorExportPlatformIOS::_export_ios_plugins(const Ref<EditorExportPreset> &p_preset, IOSConfigData &p_config_data, const String &dest_dir, Vector<IOSExportAsset> &r_exported_assets, bool p_debug) {
	String plugin_definition_cpp_code;
	String plugin_initialization_cpp_code;
	String plugin_deinitialization_cpp_code;

	Vector<String> plugin_linked_dependencies;
	Vector<String> plugin_embedded_dependencies;
	Vector<String> plugin_files;

	Vector<PluginConfigIOS> enabled_plugins = get_enabled_plugins(p_preset);

	Vector<String> added_linked_dependenciy_names;
	Vector<String> added_embedded_dependenciy_names;
	HashMap<String, String> plist_values;

	HashSet<String> plugin_linker_flags;

	Error err;

	for (int i = 0; i < enabled_plugins.size(); i++) {
		PluginConfigIOS plugin = enabled_plugins[i];

		// Export plugin binary.
		String plugin_main_binary = PluginConfigIOS::get_plugin_main_binary(plugin, p_debug);
		String plugin_binary_result_file = plugin.binary.get_file();
		// We shouldn't embed .xcframework that contains static libraries.
		// Static libraries are not embedded anyway.
		err = _copy_asset(p_preset, dest_dir, plugin_main_binary, &plugin_binary_result_file, true, false, r_exported_assets);
		ERR_FAIL_COND_V(err != OK, err);

		// Adding dependencies.
		// Use separate container for names to check for duplicates.
		for (int j = 0; j < plugin.linked_dependencies.size(); j++) {
			String dependency = plugin.linked_dependencies[j];
			String name = dependency.get_file();

			if (added_linked_dependenciy_names.has(name)) {
				continue;
			}

			added_linked_dependenciy_names.push_back(name);
			plugin_linked_dependencies.push_back(dependency);
		}

		for (int j = 0; j < plugin.system_dependencies.size(); j++) {
			String dependency = plugin.system_dependencies[j];
			String name = dependency.get_file();

			if (added_linked_dependenciy_names.has(name)) {
				continue;
			}

			added_linked_dependenciy_names.push_back(name);
			plugin_linked_dependencies.push_back(dependency);
		}

		for (int j = 0; j < plugin.embedded_dependencies.size(); j++) {
			String dependency = plugin.embedded_dependencies[j];
			String name = dependency.get_file();

			if (added_embedded_dependenciy_names.has(name)) {
				continue;
			}

			added_embedded_dependenciy_names.push_back(name);
			plugin_embedded_dependencies.push_back(dependency);
		}

		plugin_files.append_array(plugin.files_to_copy);

		// Capabilities
		// Also checking for duplicates.
		for (int j = 0; j < plugin.capabilities.size(); j++) {
			String capability = plugin.capabilities[j];

			if (p_config_data.capabilities.has(capability)) {
				continue;
			}

			p_config_data.capabilities.push_back(capability);
		}

		// Linker flags
		// Checking duplicates
		for (int j = 0; j < plugin.linker_flags.size(); j++) {
			String linker_flag = plugin.linker_flags[j];
			plugin_linker_flags.insert(linker_flag);
		}

		// Plist
		// Using hash map container to remove duplicates

		for (const KeyValue<String, PluginConfigIOS::PlistItem> &E : plugin.plist) {
			String key = E.key;
			const PluginConfigIOS::PlistItem &item = E.value;

			String value;

			switch (item.type) {
				case PluginConfigIOS::PlistItemType::STRING_INPUT: {
					String preset_name = "plugins_plist/" + key;
					String input_value = p_preset->get(preset_name);
					value = "<string>" + input_value + "</string>";
				} break;
				default:
					value = item.value;
					break;
			}

			if (key.is_empty() || value.is_empty()) {
				continue;
			}

			String plist_key = "<key>" + key + "</key>";

			plist_values[plist_key] = value;
		}

		// CPP Code
		String definition_comment = "// Plugin: " + plugin.name + "\n";
		String initialization_method = plugin.initialization_method + "();\n";
		String deinitialization_method = plugin.deinitialization_method + "();\n";

		plugin_definition_cpp_code += definition_comment +
				"extern void " + initialization_method +
				"extern void " + deinitialization_method + "\n";

		plugin_initialization_cpp_code += "\t" + initialization_method;
		plugin_deinitialization_cpp_code += "\t" + deinitialization_method;

		if (plugin.use_swift_runtime) {
			p_config_data.use_swift_runtime = true;
		}
	}

	// Updating `Info.plist`
	{
		for (const KeyValue<String, String> &E : plist_values) {
			String key = E.key;
			String value = E.value;

			if (key.is_empty() || value.is_empty()) {
				continue;
			}

			p_config_data.plist_content += key + value + "\n";
		}
	}

	// Export files
	{
		// Export linked plugin dependency
		err = _export_additional_assets(p_preset, dest_dir, plugin_linked_dependencies, true, false, r_exported_assets);
		ERR_FAIL_COND_V(err != OK, err);

		// Export embedded plugin dependency
		err = _export_additional_assets(p_preset, dest_dir, plugin_embedded_dependencies, true, true, r_exported_assets);
		ERR_FAIL_COND_V(err != OK, err);

		// Export plugin files
		err = _export_additional_assets(p_preset, dest_dir, plugin_files, false, false, r_exported_assets);
		ERR_FAIL_COND_V(err != OK, err);
	}

	// Update CPP
	{
		Dictionary plugin_format;
		plugin_format["definition"] = plugin_definition_cpp_code;
		plugin_format["initialization"] = plugin_initialization_cpp_code;
		plugin_format["deinitialization"] = plugin_deinitialization_cpp_code;

		String plugin_cpp_code = "\n// Godot Plugins\n"
								 "void godot_ios_plugins_initialize();\n"
								 "void godot_ios_plugins_deinitialize();\n"
								 "// Exported Plugins\n\n"
								 "$definition"
								 "// Use Plugins\n"
								 "void godot_ios_plugins_initialize() {\n"
								 "$initialization"
								 "}\n\n"
								 "void godot_ios_plugins_deinitialize() {\n"
								 "$deinitialization"
								 "}\n";

		p_config_data.cpp_code += plugin_cpp_code.format(plugin_format, "$_");
	}

	// Update Linker Flag Values
	{
		String result_linker_flags = " ";
		for (const String &E : plugin_linker_flags) {
			const String &flag = E;

			if (flag.length() == 0) {
				continue;
			}

			if (result_linker_flags.length() > 0) {
				result_linker_flags += ' ';
			}

			result_linker_flags += flag;
		}
		result_linker_flags = result_linker_flags.replace("\"", "\\\"");
		p_config_data.linker_flags += result_linker_flags;
	}

	return OK;
}

Error EditorExportPlatformIOS::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	return _export_project_helper(p_preset, p_debug, p_path, p_flags, false, false);
}

Error EditorExportPlatformIOS::_export_project_helper(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags, bool p_simulator, bool p_oneclick) {
	ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);

	const String dest_dir = p_path.get_base_dir() + "/";
	const String binary_name = p_path.get_file().get_basename();
	const String binary_dir = dest_dir + binary_name;

	if (!DirAccess::exists(dest_dir)) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Target folder does not exist or is inaccessible: \"%s\""), dest_dir));
		return ERR_FILE_BAD_PATH;
	}

	bool export_project_only = p_preset->get("application/export_project_only");
	if (p_oneclick) {
		export_project_only = false; // Skip for one-click deploy.
	}

	EditorProgress ep("export", export_project_only ? TTR("Exporting for iOS (Project Files Only)") : TTR("Exporting for iOS"), export_project_only ? 2 : 5, true);

	String team_id = p_preset->get("application/app_store_team_id");
	ERR_FAIL_COND_V_MSG(team_id.length() == 0, ERR_CANT_OPEN, "App Store Team ID not specified - cannot configure the project.");

	String src_pkg_name;
	if (p_debug) {
		src_pkg_name = p_preset->get("custom_template/debug");
	} else {
		src_pkg_name = p_preset->get("custom_template/release");
	}

	if (src_pkg_name.is_empty()) {
		String err;
		src_pkg_name = find_export_template("ios.zip", &err);
		if (src_pkg_name.is_empty()) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Templates"), TTR("Export template not found."));
			return ERR_FILE_NOT_FOUND;
		}
	}

	{
		bool delete_old = p_preset->get("application/delete_old_export_files_unconditionally");
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		if (da.is_valid()) {
			String current_dir = da->get_current_dir();

			// Remove leftovers from last export so they don't interfere in case some files are no longer needed.
			if (da->change_dir(binary_dir + ".xcodeproj") == OK) {
				// Check directory content before deleting.
				int expected_files = 0;
				int total_files = 0;
				if (!delete_old) {
					da->list_dir_begin();
					for (String n = da->get_next(); !n.is_empty(); n = da->get_next()) {
						if (!n.begins_with(".")) { // Ignore ".", ".." and hidden files.
							if (da->current_is_dir()) {
								if (n == "xcshareddata" || n == "project.xcworkspace") {
									expected_files++;
								}
							} else {
								if (n == "project.pbxproj") {
									expected_files++;
								}
							}
							total_files++;
						}
					}
					da->list_dir_end();
				}
				if ((total_files == 0) || (expected_files >= Math::floor(total_files * 0.8))) {
					da->erase_contents_recursive();
				} else {
					add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Unexpected files found in the export destination directory \"%s.xcodeproj\", delete it manually or select another destination."), binary_dir));
					return ERR_CANT_CREATE;
				}
			}
			da->change_dir(current_dir);

			if (da->change_dir(binary_dir) == OK) {
				// Check directory content before deleting.
				int expected_files = 0;
				int total_files = 0;
				if (!delete_old) {
					da->list_dir_begin();
					for (String n = da->get_next(); !n.is_empty(); n = da->get_next()) {
						if (!n.begins_with(".")) { // Ignore ".", ".." and hidden files.
							if (da->current_is_dir()) {
								if (n == "dylibs" || n == "Images.xcassets" || n.ends_with(".lproj") || n == "godot-publish-dotnet" || n.ends_with(".xcframework") || n.ends_with(".framework")) {
									expected_files++;
								}
							} else {
								if (n == binary_name + "-Info.plist" || n == binary_name + ".entitlements" || n == "Launch Screen.storyboard" || n == "export_options.plist" || n.begins_with("dummy.") || n.ends_with(".gdip")) {
									expected_files++;
								}
							}
							total_files++;
						}
					}
					da->list_dir_end();
				}
				if ((total_files == 0) || (expected_files >= Math::floor(total_files * 0.8))) {
					da->erase_contents_recursive();
				} else {
					add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Unexpected files found in the export destination directory \"%s\", delete it manually or select another destination."), binary_dir));
					return ERR_CANT_CREATE;
				}
			}
			da->change_dir(current_dir);

			if (!da->dir_exists(binary_dir)) {
				Error err = da->make_dir(binary_dir);
				if (err != OK) {
					add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Failed to create the directory: \"%s\""), binary_dir));
					return err;
				}
			}
		}
	}

	if (ep.step("Making .pck", 0)) {
		return ERR_SKIP;
	}
	String pack_path = binary_dir + ".pck";
	Vector<SharedObject> libraries;
	Error err = save_pack(p_preset, p_debug, pack_path, &libraries);
	if (err) {
		// Message is supplied by the subroutine method.
		return err;
	}

	if (ep.step("Extracting and configuring Xcode project", 1)) {
		return ERR_SKIP;
	}

	String library_to_use = "libgodot.ios." + String(p_debug ? "debug" : "release") + ".xcframework";

	print_line("Static framework: " + library_to_use);
	String pkg_name;
	if (String(GLOBAL_GET("application/config/name")) != "") {
		pkg_name = String(GLOBAL_GET("application/config/name"));
	} else {
		pkg_name = "Unnamed";
	}

	bool found_library = false;

	const String project_file = "godot_ios.xcodeproj/project.pbxproj";
	HashSet<String> files_to_parse;
	files_to_parse.insert("godot_ios/godot_ios-Info.plist");
	files_to_parse.insert(project_file);
	files_to_parse.insert("godot_ios/export_options.plist");
	files_to_parse.insert("godot_ios/dummy.cpp");
	files_to_parse.insert("godot_ios.xcodeproj/project.xcworkspace/contents.xcworkspacedata");
	files_to_parse.insert("godot_ios.xcodeproj/xcshareddata/xcschemes/godot_ios.xcscheme");
	files_to_parse.insert("godot_ios/godot_ios.entitlements");
	files_to_parse.insert("godot_ios/Launch Screen.storyboard");
	files_to_parse.insert("PrivacyInfo.xcprivacy");

	IOSConfigData config_data = {
		pkg_name,
		binary_name,
		_get_additional_plist_content(),
		String(" ").join(_get_preset_architectures(p_preset)),
		_get_linker_flags(),
		_get_cpp_code(),
		"",
		"",
		"",
		"",
		Vector<String>(),
		false
	};

	config_data.plist_content += p_preset->get("application/additional_plist_content").operator String() + "\n";

	Vector<IOSExportAsset> assets;

	Ref<DirAccess> tmp_app_path = DirAccess::create_for_path(dest_dir);
	if (tmp_app_path.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Templates"), vformat(TTR("Could not create and open the directory: \"%s\""), dest_dir));
		return ERR_CANT_CREATE;
	}

	print_line("Unzipping...");
	Ref<FileAccess> io_fa;
	zlib_filefunc_def io = zipio_create_io(&io_fa);
	unzFile src_pkg_zip = unzOpen2(src_pkg_name.utf8().get_data(), &io);
	if (!src_pkg_zip) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Templates"), TTR("Could not open export template (not a zip file?): \"%s\".", src_pkg_name));
		return ERR_CANT_OPEN;
	}

	err = _export_ios_plugins(p_preset, config_data, binary_dir, assets, p_debug);
	if (err != OK) {
		// TODO: Improve error reporting by using `add_message` throughout all methods called via `_export_ios_plugins`.
		// For now a generic top level message would be fine, but we're ought to use proper reporting here instead of
		// just fail macros and non-descriptive error return values.
		add_message(EXPORT_MESSAGE_ERROR, TTR("iOS Plugins"), vformat(TTR("Failed to export iOS plugins with code %d. Please check the output log."), err));
		return err;
	}

	//export rest of the files
	int ret = unzGoToFirstFile(src_pkg_zip);
	Vector<uint8_t> project_file_data;
	while (ret == UNZ_OK) {
#if defined(MACOS_ENABLED) || defined(LINUXBSD_ENABLED)
		bool is_execute = false;
#endif

		//get filename
		unz_file_info info;
		char fname[16384];
		ret = unzGetCurrentFileInfo(src_pkg_zip, &info, fname, 16384, nullptr, 0, nullptr, 0);
		if (ret != UNZ_OK) {
			break;
		}

		String file = String::utf8(fname);

		print_line("READ: " + file);
		Vector<uint8_t> data;
		data.resize(info.uncompressed_size);

		//read
		unzOpenCurrentFile(src_pkg_zip);
		unzReadCurrentFile(src_pkg_zip, data.ptrw(), data.size());
		unzCloseCurrentFile(src_pkg_zip);

		//write

		if (files_to_parse.has(file)) {
			_fix_config_file(p_preset, data, config_data, p_debug);
		} else if (file.begins_with("libgodot.ios")) {
			if (!file.begins_with(library_to_use) || file.ends_with(String("/empty"))) {
				ret = unzGoToNextFile(src_pkg_zip);
				continue; //ignore!
			}
			found_library = true;
#if defined(MACOS_ENABLED) || defined(LINUXBSD_ENABLED)
			is_execute = true;
#endif
			file = file.replace(library_to_use, binary_name + ".xcframework");
		}

		if (file == project_file) {
			project_file_data = data;
		}

		///@TODO need to parse logo files

		if (data.size() > 0) {
			file = file.replace("godot_ios", binary_name);

			print_line("ADDING: " + file + " size: " + itos(data.size()));

			/* write it into our folder structure */
			file = dest_dir + file;

			/* make sure this folder exists */
			String dir_name = file.get_base_dir();
			if (!tmp_app_path->dir_exists(dir_name)) {
				print_line("Creating " + dir_name);
				Error dir_err = tmp_app_path->make_dir_recursive(dir_name);
				if (dir_err) {
					unzClose(src_pkg_zip);
					add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not create a directory at path \"%s\"."), dir_name));
					return ERR_CANT_CREATE;
				}
			}

			/* write the file */
			{
				Ref<FileAccess> f = FileAccess::open(file, FileAccess::WRITE);
				if (f.is_null()) {
					unzClose(src_pkg_zip);
					add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not write to a file at path \"%s\"."), file));
					return ERR_CANT_CREATE;
				};
				f->store_buffer(data.ptr(), data.size());
			}

#if defined(MACOS_ENABLED) || defined(LINUXBSD_ENABLED)
			if (is_execute) {
				// we need execute rights on this file
				chmod(file.utf8().get_data(), 0755);
			}
#endif
		}

		ret = unzGoToNextFile(src_pkg_zip);
	}

	// We're done with our source zip.
	unzClose(src_pkg_zip);

	if (!found_library) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Requested template library '%s' not found. It might be missing from your template archive."), library_to_use));
		return ERR_FILE_NOT_FOUND;
	}

	// Check and generate missing ARM64 simulator library.
	if (p_preset->get("application/generate_simulator_library_if_missing").operator bool()) {
		String sim_lib_path = dest_dir + String(binary_name + ".xcframework").path_join("ios-arm64_x86_64-simulator").path_join("libgodot.a");
		String dev_lib_path = dest_dir + String(binary_name + ".xcframework").path_join("ios-arm64").path_join("libgodot.a");
		String tmp_lib_path = EditorPaths::get_singleton()->get_temp_dir().path_join(binary_name + "_lipo_");
		uint32_t cputype = 0;
		uint32_t cpusubtype = 0;
		if (!_archive_has_arm64(sim_lib_path, &cputype, &cpusubtype) && _archive_has_arm64(dev_lib_path) && FileAccess::exists(dev_lib_path)) {
			add_message(EXPORT_MESSAGE_INFO, TTR("Export"), TTR("ARM64 simulator library, generating from device library."));

			Vector<String> tmp_lib_files;
			Vector<Vector2i> tmp_lib_cputypes;
			// Extract/copy simulator lib.
			if (FileAccess::exists(sim_lib_path)) {
				if (LipO::is_lipo(sim_lib_path)) {
					Ref<LipO> lipo;
					lipo.instantiate();
					if (lipo->open_file(sim_lib_path)) {
						for (int i = 0; i < lipo->get_arch_count(); i++) {
							const String &f_name = tmp_lib_path + itos(tmp_lib_files.size());
							lipo->extract_arch(i, f_name);
							tmp_lib_files.push_back(f_name);
							tmp_lib_cputypes.push_back(Vector2i(lipo->get_arch_cputype(i), lipo->get_arch_cpusubtype(i)));
						}
					}
				} else {
					const String &f_name = tmp_lib_path + itos(tmp_lib_files.size());
					tmp_app_path->copy(sim_lib_path, f_name);
					tmp_lib_files.push_back(f_name);
					tmp_lib_cputypes.push_back(Vector2i(cputype, cpusubtype));
				}
			}
			// Copy device lib.
			if (LipO::is_lipo(dev_lib_path)) {
				Ref<LipO> lipo;
				lipo.instantiate();
				if (lipo->open_file(dev_lib_path)) {
					for (int i = 0; i < lipo->get_arch_count(); i++) {
						if (lipo->get_arch_cputype(i) == 0x100000c && lipo->get_arch_cpusubtype(i) == 0) {
							const String &f_name = tmp_lib_path + itos(tmp_lib_files.size());
							lipo->extract_arch(i, f_name);
							tmp_lib_files.push_back(f_name);
							tmp_lib_cputypes.push_back(Vector2i(0x100000c, 0)); // ARM64.
							break;
						}
					}
				}
			} else {
				const String &f_name = tmp_lib_path + itos(tmp_lib_files.size());
				tmp_app_path->copy(dev_lib_path, f_name);
				tmp_lib_files.push_back(f_name);
				tmp_lib_cputypes.push_back(Vector2i(0x100000c, 0)); // ARM64.
			}

			// Patch device lib.
			int patch_count = _archive_convert_to_simulator(tmp_lib_path + itos(tmp_lib_files.size() - 1));
			if (patch_count == 0) {
				add_message(EXPORT_MESSAGE_WARNING, TTR("Export"), TTR("Unable to generate ARM64 simulator library."));
			} else {
				// Repack.
				Ref<LipO> lipo;
				lipo.instantiate();
				lipo->create_file(sim_lib_path, tmp_lib_files, tmp_lib_cputypes);
			}

			// Cleanup.
			for (const String &E : tmp_lib_files) {
				tmp_app_path->remove(E);
			}
		}
	}

	// Generate translations files.

	Dictionary appnames = GLOBAL_GET("application/config/name_localized");
	Dictionary camera_usage_descriptions = p_preset->get("privacy/camera_usage_description_localized");
	Dictionary microphone_usage_descriptions = p_preset->get("privacy/microphone_usage_description_localized");
	Dictionary photolibrary_usage_descriptions = p_preset->get("privacy/photolibrary_usage_description_localized");

	Vector<String> translations = GLOBAL_GET("internationalization/locale/translations");
	if (translations.size() > 0) {
		{
			String fname = binary_dir + "/en.lproj";
			tmp_app_path->make_dir_recursive(fname);
			Ref<FileAccess> f = FileAccess::open(fname + "/InfoPlist.strings", FileAccess::WRITE);
			f->store_line("/* Localized versions of Info.plist keys */");
			f->store_line("");
			f->store_line("CFBundleDisplayName = \"" + GLOBAL_GET("application/config/name").operator String() + "\";");
			f->store_line("NSCameraUsageDescription = \"" + p_preset->get("privacy/camera_usage_description").operator String() + "\";");
			f->store_line("NSMicrophoneUsageDescription = \"" + p_preset->get("privacy/microphone_usage_description").operator String() + "\";");
			f->store_line("NSPhotoLibraryUsageDescription = \"" + p_preset->get("privacy/photolibrary_usage_description").operator String() + "\";");
		}

		HashSet<String> languages;
		for (const String &E : translations) {
			Ref<Translation> tr = ResourceLoader::load(E);
			if (tr.is_valid() && tr->get_locale() != "en") {
				languages.insert(tr->get_locale());
			}
		}

		for (const String &lang : languages) {
			String fname = binary_dir + "/" + lang + ".lproj";
			tmp_app_path->make_dir_recursive(fname);
			Ref<FileAccess> f = FileAccess::open(fname + "/InfoPlist.strings", FileAccess::WRITE);
			f->store_line("/* Localized versions of Info.plist keys */");
			f->store_line("");
			if (appnames.has(lang)) {
				f->store_line("CFBundleDisplayName = \"" + appnames[lang].operator String() + "\";");
			}
			if (camera_usage_descriptions.has(lang)) {
				f->store_line("NSCameraUsageDescription = \"" + camera_usage_descriptions[lang].operator String() + "\";");
			}
			if (microphone_usage_descriptions.has(lang)) {
				f->store_line("NSMicrophoneUsageDescription = \"" + microphone_usage_descriptions[lang].operator String() + "\";");
			}
			if (photolibrary_usage_descriptions.has(lang)) {
				f->store_line("NSPhotoLibraryUsageDescription = \"" + photolibrary_usage_descriptions[lang].operator String() + "\";");
			}
		}
	}

	// Copy project static libs to the project
	Vector<Ref<EditorExportPlugin>> export_plugins = EditorExport::get_singleton()->get_export_plugins();
	for (int i = 0; i < export_plugins.size(); i++) {
		Vector<String> project_static_libs = export_plugins[i]->get_ios_project_static_libs();
		for (int j = 0; j < project_static_libs.size(); j++) {
			const String &static_lib_path = project_static_libs[j];
			String dest_lib_file_path = dest_dir + static_lib_path.get_file();
			Error lib_copy_err = tmp_app_path->copy(static_lib_path, dest_lib_file_path);
			if (lib_copy_err != OK) {
				add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not copy a file at path \"%s\" to \"%s\"."), static_lib_path, dest_lib_file_path));
				return lib_copy_err;
			}
		}
	}

	String iconset_dir = binary_dir + "/Images.xcassets/AppIcon.appiconset/";
	err = OK;
	if (!tmp_app_path->dir_exists(iconset_dir)) {
		err = tmp_app_path->make_dir_recursive(iconset_dir);
	}
	if (err != OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not create a directory at path \"%s\"."), iconset_dir));
		return err;
	}

	err = _export_icons(p_preset, iconset_dir);
	if (err != OK) {
		// Message is supplied by the subroutine method.
		return err;
	}

	{
		String splash_image_path = binary_dir + "/Images.xcassets/SplashImage.imageset/";

		Ref<DirAccess> launch_screen_da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		if (launch_screen_da.is_null()) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Could not access the filesystem."));
			return ERR_CANT_CREATE;
		}

		print_line("Exporting launch screen storyboard");

		err = _export_loading_screen_file(p_preset, splash_image_path);
		if (err != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Failed to create a file at path \"%s\" with code %d."), splash_image_path, err));
		}
	}

	if (err != OK) {
		return err;
	}

	print_line("Exporting additional assets");
	_export_additional_assets(p_preset, binary_dir, libraries, assets);
	_add_assets_to_project(dest_dir, p_preset, project_file_data, assets);
	String project_file_name = binary_dir + ".xcodeproj/project.pbxproj";
	{
		Ref<FileAccess> f = FileAccess::open(project_file_name, FileAccess::WRITE);
		if (f.is_null()) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not write to a file at path \"%s\"."), project_file_name));
			return ERR_CANT_CREATE;
		};
		f->store_buffer(project_file_data.ptr(), project_file_data.size());
	}

#ifdef MACOS_ENABLED
	{
		if (ep.step("Code-signing dylibs", 2)) {
			return ERR_SKIP;
		}
		Ref<DirAccess> dylibs_dir = DirAccess::open(binary_dir + "/dylibs");
		ERR_FAIL_COND_V(dylibs_dir.is_null(), ERR_CANT_OPEN);
		CodesignData codesign_data(p_preset, p_debug);
		err = _walk_dir_recursive(dylibs_dir, _codesign, &codesign_data);
		if (err != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Code Signing"), TTR("Code signing failed, see editor log for details."));
			return err;
		}
	}

	if (export_project_only) {
		return OK;
	}

	if (ep.step("Making .xcarchive", 3)) {
		return ERR_SKIP;
	}

	String archive_path = p_path.get_basename() + ".xcarchive";
	List<String> archive_args;
	archive_args.push_back("-project");
	archive_args.push_back(binary_dir + ".xcodeproj");
	archive_args.push_back("-scheme");
	archive_args.push_back(binary_name);
	archive_args.push_back("-sdk");
	if (p_simulator) {
		archive_args.push_back("iphonesimulator");
	} else {
		archive_args.push_back("iphoneos");
	}
	archive_args.push_back("-configuration");
	archive_args.push_back(p_debug ? "Debug" : "Release");
	archive_args.push_back("-destination");
	if (p_simulator) {
		archive_args.push_back("generic/platform=iOS Simulator");
	} else {
		archive_args.push_back("generic/platform=iOS");
	}
	archive_args.push_back("archive");
	archive_args.push_back("-allowProvisioningUpdates");
	archive_args.push_back("-archivePath");
	archive_args.push_back(archive_path);

	String archive_str;
	err = OS::get_singleton()->execute("xcodebuild", archive_args, &archive_str, nullptr, true);
	if (err != OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Xcode Build"), vformat(TTR("Failed to run xcodebuild with code %d"), err));
		return err;
	}

	print_line("xcodebuild (.xcarchive):\n" + archive_str);
	if (!archive_str.contains("** ARCHIVE SUCCEEDED **")) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Xcode Build"), TTR("Xcode project build failed, see editor log for details."));
		return FAILED;
	}

	if (!p_oneclick) {
		if (ep.step("Making .ipa", 4)) {
			return ERR_SKIP;
		}

		List<String> export_args;
		export_args.push_back("-exportArchive");
		export_args.push_back("-archivePath");
		export_args.push_back(archive_path);
		export_args.push_back("-exportOptionsPlist");
		export_args.push_back(binary_dir + "/export_options.plist");
		export_args.push_back("-allowProvisioningUpdates");
		export_args.push_back("-exportPath");
		export_args.push_back(dest_dir);

		String export_str;
		err = OS::get_singleton()->execute("xcodebuild", export_args, &export_str, nullptr, true);
		if (err != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Xcode Build"), vformat(TTR("Failed to run xcodebuild with code %d"), err));
			return err;
		}

		print_line("xcodebuild (.ipa):\n" + export_str);
		if (!export_str.contains("** EXPORT SUCCEEDED **")) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Xcode Build"), TTR(".ipa export failed, see editor log for details."));
			return FAILED;
		}
	}
#else
	add_message(EXPORT_MESSAGE_WARNING, TTR("Xcode Build"), TTR(".ipa can only be built on macOS. Leaving Xcode project without building the package."));
#endif

	return OK;
}

bool EditorExportPlatformIOS::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
#if defined(MODULE_MONO_ENABLED) && !defined(MACOS_ENABLED)
	// TODO: Remove this restriction when we don't rely on macOS tools to package up the native libraries anymore.
	r_error += TTR("Exporting to iOS when using C#/.NET is experimental and requires macOS.") + "\n";
	return false;
#else

	String err;
	bool valid = false;

#if defined(MODULE_MONO_ENABLED)
	// iOS export is still a work in progress, keep a message as a warning.
	err += TTR("Exporting to iOS when using C#/.NET is experimental.") + "\n";
#endif
	// Look for export templates (first official, and if defined custom templates).

	bool dvalid = exists_export_template("ios.zip", &err);
	bool rvalid = dvalid; // Both in the same ZIP.

	if (p_preset->get("custom_template/debug") != "") {
		dvalid = FileAccess::exists(p_preset->get("custom_template/debug"));
		if (!dvalid) {
			err += TTR("Custom debug template not found.") + "\n";
		}
	}
	if (p_preset->get("custom_template/release") != "") {
		rvalid = FileAccess::exists(p_preset->get("custom_template/release"));
		if (!rvalid) {
			err += TTR("Custom release template not found.") + "\n";
		}
	}

	valid = dvalid || rvalid;
	r_missing_templates = !valid;

	const String &additional_plist_content = p_preset->get("application/additional_plist_content");
	if (!additional_plist_content.is_empty()) {
		const String &plist = vformat("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
									  "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
									  "<plist version=\"1.0\">"
									  "<dict>\n"
									  "%s\n"
									  "</dict>\n"
									  "</plist>\n",
				additional_plist_content);

		String plist_err;
		Ref<PList> plist_parser;
		plist_parser.instantiate();
		if (!plist_parser->load_string(plist, plist_err)) {
			err += TTR("Invalid additional PList content: ") + plist_err + "\n";
			valid = false;
		}
	}

	if (GLOBAL_GET("rendering/rendering_device/driver.ios") == "metal") {
		float version = p_preset->get("application/min_ios_version").operator String().to_float();
		if (version < 14.0) {
			err += TTR("Metal renderer require iOS 14+.") + "\n";
		}
	}

	if (!err.is_empty()) {
		r_error = err;
	}

	return valid;
#endif // !(MODULE_MONO_ENABLED && !MACOS_ENABLED)
}

bool EditorExportPlatformIOS::has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const {
	String err;
	bool valid = true;

	// Validate the project configuration.

	List<ExportOption> options;
	get_export_options(&options);
	for (const EditorExportPlatform::ExportOption &E : options) {
		if (get_export_option_visibility(p_preset.ptr(), E.option.name)) {
			String warn = get_export_option_warning(p_preset.ptr(), E.option.name);
			if (!warn.is_empty()) {
				err += warn + "\n";
				if (E.required) {
					valid = false;
				}
			}
		}
	}

	if (!ResourceImporterTextureSettings::should_import_etc2_astc()) {
		valid = false;
	}

	if (!err.is_empty()) {
		r_error = err;
	}

	return valid;
}

int EditorExportPlatformIOS::get_options_count() const {
	MutexLock lock(device_lock);
	return devices.size();
}

String EditorExportPlatformIOS::get_options_tooltip() const {
	return TTR("Select device from the list");
}

Ref<ImageTexture> EditorExportPlatformIOS::get_option_icon(int p_index) const {
	MutexLock lock(device_lock);

	Ref<ImageTexture> icon;
	if (p_index >= 0 || p_index < devices.size()) {
		Ref<Theme> theme = EditorNode::get_singleton()->get_editor_theme();
		if (theme.is_valid()) {
			if (devices[p_index].simulator) {
				icon = theme->get_icon("IOSSimulator", EditorStringName(EditorIcons));
			} else if (devices[p_index].wifi) {
				icon = theme->get_icon("IOSDeviceWireless", EditorStringName(EditorIcons));
			} else {
				icon = theme->get_icon("IOSDeviceWired", EditorStringName(EditorIcons));
			}
		}
	}
	return icon;
}

String EditorExportPlatformIOS::get_option_label(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, devices.size(), "");
	MutexLock lock(device_lock);
	return devices[p_index].name;
}

String EditorExportPlatformIOS::get_option_tooltip(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, devices.size(), "");
	MutexLock lock(device_lock);
	return "UUID: " + devices[p_index].id;
}

bool EditorExportPlatformIOS::is_package_name_valid(const String &p_package, String *r_error) const {
	String pname = p_package;

	if (pname.length() == 0) {
		if (r_error) {
			*r_error = TTR("Identifier is missing.");
		}
		return false;
	}

	for (int i = 0; i < pname.length(); i++) {
		char32_t c = pname[i];
		if (!(is_ascii_alphanumeric_char(c) || c == '-' || c == '.')) {
			if (r_error) {
				*r_error = vformat(TTR("The character '%s' is not allowed in Identifier."), String::chr(c));
			}
			return false;
		}
	}

	return true;
}

#ifdef MACOS_ENABLED
bool EditorExportPlatformIOS::_check_xcode_install() {
	static bool xcode_found = false;
	if (!xcode_found) {
		Vector<String> mdfind_paths;
		List<String> mdfind_args;
		mdfind_args.push_back("kMDItemCFBundleIdentifier=com.apple.dt.Xcode");

		String output;
		Error err = OS::get_singleton()->execute("mdfind", mdfind_args, &output);
		if (err == OK) {
			mdfind_paths = output.split("\n");
		}
		for (const String &found_path : mdfind_paths) {
			xcode_found = !found_path.is_empty() && DirAccess::dir_exists_absolute(found_path.strip_edges());
			if (xcode_found) {
				break;
			}
		}
	}
	return xcode_found;
}

void EditorExportPlatformIOS::_check_for_changes_poll_thread(void *ud) {
	EditorExportPlatformIOS *ea = static_cast<EditorExportPlatformIOS *>(ud);

	while (!ea->quit_request.is_set()) {
		// Nothing to do if we already know the plugins have changed.
		if (!ea->plugins_changed.is_set()) {
			MutexLock lock(ea->plugins_lock);

			Vector<PluginConfigIOS> loaded_plugins = get_plugins();

			if (ea->plugins.size() != loaded_plugins.size()) {
				ea->plugins_changed.set();
			} else {
				for (int i = 0; i < ea->plugins.size(); i++) {
					if (ea->plugins[i].name != loaded_plugins[i].name || ea->plugins[i].last_updated != loaded_plugins[i].last_updated) {
						ea->plugins_changed.set();
						break;
					}
				}
			}
		}

		// Check for devices updates.
		Vector<Device> ldevices;

		// Enum real devices (via ios_deploy, pre Xcode 15).
		String idepl = EDITOR_GET("export/ios/ios_deploy");
		if (ea->has_runnable_preset.is_set() && !idepl.is_empty()) {
			String devices;
			List<String> args;
			args.push_back("-c");
			args.push_back("-timeout");
			args.push_back("1");
			args.push_back("-j");
			args.push_back("-u");
			args.push_back("-I");

			int ec = 0;
			Error err = OS::get_singleton()->execute(idepl, args, &devices, &ec, true);
			if (err == OK && ec == 0) {
				Ref<JSON> json;
				json.instantiate();
				devices = "{ \"devices\":[" + devices.replace("}{", "},{") + "]}";
				err = json->parse(devices);
				if (err == OK) {
					Dictionary data = json->get_data();
					Array devices = data["devices"];
					for (int i = 0; i < devices.size(); i++) {
						Dictionary device_event = devices[i];
						if (device_event["Event"] == "DeviceDetected") {
							Dictionary device_info = device_event["Device"];
							Device nd;
							nd.id = device_info["DeviceIdentifier"];
							nd.name = device_info["DeviceName"].operator String() + " (ios_deploy, " + ((device_event["Interface"] == "WIFI") ? "network" : "wired") + ")";
							nd.wifi = device_event["Interface"] == "WIFI";
							nd.use_ios_deploy = true;
							nd.simulator = false;
							ldevices.push_back(nd);
						}
					}
				}
			}
		}

		// Enum simulators.
		if (ea->has_runnable_preset.is_set() && _check_xcode_install() && (FileAccess::exists("/usr/bin/xcrun") || FileAccess::exists("/bin/xcrun"))) {
			{
				String devices;
				List<String> args;
				args.push_back("devicectl");
				args.push_back("list");
				args.push_back("devices");
				args.push_back("-j");
				args.push_back("-");
				args.push_back("-q");
				int ec = 0;
				Error err = OS::get_singleton()->execute("xcrun", args, &devices, &ec, true);
				if (err == OK && ec == 0) {
					Ref<JSON> json;
					json.instantiate();
					err = json->parse(devices);
					if (err == OK) {
						const Dictionary &data = json->get_data();
						const Dictionary &result = data["result"];
						const Array &devices = result["devices"];
						for (int i = 0; i < devices.size(); i++) {
							const Dictionary &device_info = devices[i];
							const Dictionary &conn_props = device_info["connectionProperties"];
							const Dictionary &dev_props = device_info["deviceProperties"];
							if (conn_props["pairingState"] == "paired" && dev_props["developerModeStatus"] == "enabled") {
								Device nd;
								nd.id = device_info["identifier"];
								nd.name = dev_props["name"].operator String() + " (devicectl, " + ((conn_props["transportType"] == "localNetwork") ? "network" : "wired") + ")";
								nd.wifi = conn_props["transportType"] == "localNetwork";
								nd.simulator = false;
								ldevices.push_back(nd);
							}
						}
					}
				}
			}

			// Enum simulators.
			if (ea->has_runnable_preset.is_set()) {
				String devices;
				List<String> args;
				args.push_back("simctl");
				args.push_back("list");
				args.push_back("devices");
				args.push_back("-j");

				int ec = 0;
				Error err = OS::get_singleton()->execute("xcrun", args, &devices, &ec, true);
				if (err == OK && ec == 0) {
					Ref<JSON> json;
					json.instantiate();
					err = json->parse(devices);
					if (err == OK) {
						const Dictionary &data = json->get_data();
						const Dictionary &devices = data["devices"];
						for (const Variant *key = devices.next(nullptr); key; key = devices.next(key)) {
							const Array &os_devices = devices[*key];
							for (int i = 0; i < os_devices.size(); i++) {
								const Dictionary &device_info = os_devices[i];
								if (device_info["isAvailable"].operator bool() && device_info["state"] == "Booted") {
									Device nd;
									nd.id = device_info["udid"];
									nd.name = device_info["name"].operator String() + " (simulator)";
									nd.simulator = true;
									ldevices.push_back(nd);
								}
							}
						}
					}
				}
			}
		}

		// Update device list.
		{
			MutexLock lock(ea->device_lock);

			bool different = false;

			if (ea->devices.size() != ldevices.size()) {
				different = true;
			} else {
				for (int i = 0; i < ea->devices.size(); i++) {
					if (ea->devices[i].id != ldevices[i].id) {
						different = true;
						break;
					}
				}
			}

			if (different) {
				ea->devices = ldevices;
				ea->devices_changed.set();
			}
		}

		uint64_t sleep = 200;
		uint64_t wait = 3000000;
		uint64_t time = OS::get_singleton()->get_ticks_usec();
		while (OS::get_singleton()->get_ticks_usec() - time < wait) {
			OS::get_singleton()->delay_usec(1000 * sleep);
			if (ea->quit_request.is_set()) {
				break;
			}
		}
	}
}

void EditorExportPlatformIOS::_update_preset_status() {
	const int preset_count = EditorExport::get_singleton()->get_export_preset_count();
	bool has_runnable = false;

	for (int i = 0; i < preset_count; i++) {
		const Ref<EditorExportPreset> &preset = EditorExport::get_singleton()->get_export_preset(i);
		if (preset->get_platform() == this && preset->is_runnable()) {
			has_runnable = true;
			break;
		}
	}

	if (has_runnable) {
		has_runnable_preset.set();
	} else {
		has_runnable_preset.clear();
	}
	devices_changed.set();
}
#endif

Error EditorExportPlatformIOS::run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) {
#ifdef MACOS_ENABLED
	ERR_FAIL_INDEX_V(p_device, devices.size(), ERR_INVALID_PARAMETER);

	String can_export_error;
	bool can_export_missing_templates;
	if (!can_export(p_preset, can_export_error, can_export_missing_templates)) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Run"), can_export_error);
		return ERR_UNCONFIGURED;
	}

	MutexLock lock(device_lock);

	EditorProgress ep("run", vformat(TTR("Running on %s"), devices[p_device].name), 3);

	String id = "tmpexport." + uitos(OS::get_singleton()->get_unix_time());

	Ref<DirAccess> filesystem_da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND_V_MSG(filesystem_da.is_null(), ERR_CANT_CREATE, "Cannot create DirAccess for path '" + EditorPaths::get_singleton()->get_temp_dir() + "'.");
	filesystem_da->make_dir_recursive(EditorPaths::get_singleton()->get_temp_dir().path_join(id));
	String tmp_export_path = EditorPaths::get_singleton()->get_temp_dir().path_join(id).path_join("export.ipa");

#define CLEANUP_AND_RETURN(m_err)                                                                          \
	{                                                                                                      \
		if (filesystem_da->change_dir(EditorPaths::get_singleton()->get_temp_dir().path_join(id)) == OK) { \
			filesystem_da->erase_contents_recursive();                                                     \
			filesystem_da->change_dir("..");                                                               \
			filesystem_da->remove(id);                                                                     \
		}                                                                                                  \
		return m_err;                                                                                      \
	}                                                                                                      \
	((void)0)

	Device dev = devices[p_device];

	// Export before sending to device.
	Error err = _export_project_helper(p_preset, true, tmp_export_path, p_debug_flags, dev.simulator, true);

	if (err != OK) {
		CLEANUP_AND_RETURN(err);
	}

	Vector<String> cmd_args_list;
	String host = EDITOR_GET("network/debug/remote_host");
	int remote_port = (int)EDITOR_GET("network/debug/remote_port");

	if (p_debug_flags.has_flag(DEBUG_FLAG_REMOTE_DEBUG_LOCALHOST)) {
		host = "localhost";
	}

	if (p_debug_flags.has_flag(DEBUG_FLAG_DUMB_CLIENT)) {
		int port = EDITOR_GET("filesystem/file_server/port");
		String passwd = EDITOR_GET("filesystem/file_server/password");
		cmd_args_list.push_back("--remote-fs");
		cmd_args_list.push_back(host + ":" + itos(port));
		if (!passwd.is_empty()) {
			cmd_args_list.push_back("--remote-fs-password");
			cmd_args_list.push_back(passwd);
		}
	}

	if (p_debug_flags.has_flag(DEBUG_FLAG_REMOTE_DEBUG)) {
		cmd_args_list.push_back("--remote-debug");

		cmd_args_list.push_back(get_debug_protocol() + host + ":" + String::num(remote_port));

		List<String> breakpoints;
		ScriptEditor::get_singleton()->get_breakpoints(&breakpoints);

		if (breakpoints.size()) {
			cmd_args_list.push_back("--breakpoints");
			String bpoints;
			for (const List<String>::Element *E = breakpoints.front(); E; E = E->next()) {
				bpoints += E->get().replace(" ", "%20");
				if (E->next()) {
					bpoints += ",";
				}
			}

			cmd_args_list.push_back(bpoints);
		}
	}

	if (p_debug_flags.has_flag(DEBUG_FLAG_VIEW_COLLISIONS)) {
		cmd_args_list.push_back("--debug-collisions");
	}

	if (p_debug_flags.has_flag(DEBUG_FLAG_VIEW_NAVIGATION)) {
		cmd_args_list.push_back("--debug-navigation");
	}

	if (dev.simulator) {
		// Deploy and run on simulator.
		if (ep.step("Installing to simulator...", 3)) {
			CLEANUP_AND_RETURN(ERR_SKIP);
		} else {
			List<String> args;
			args.push_back("simctl");
			args.push_back("install");
			args.push_back(dev.id);
			args.push_back(EditorPaths::get_singleton()->get_temp_dir().path_join(id).path_join("export.xcarchive/Products/Applications/export.app"));

			String log;
			int ec;
			err = OS::get_singleton()->execute("xcrun", args, &log, &ec, true);
			if (err != OK) {
				add_message(EXPORT_MESSAGE_WARNING, TTR("Run"), TTR("Could not start simctl executable."));
				CLEANUP_AND_RETURN(err);
			}
			if (ec != 0) {
				print_line("simctl install:\n" + log);
				add_message(EXPORT_MESSAGE_ERROR, TTR("Run"), TTR("Installation failed, see editor log for details."));
				CLEANUP_AND_RETURN(ERR_UNCONFIGURED);
			}
		}

		if (ep.step("Running on simulator...", 4)) {
			CLEANUP_AND_RETURN(ERR_SKIP);
		} else {
			List<String> args;
			args.push_back("simctl");
			args.push_back("launch");
			args.push_back("--terminate-running-process");
			args.push_back(dev.id);
			args.push_back(p_preset->get("application/bundle_identifier"));
			for (const String &E : cmd_args_list) {
				args.push_back(E);
			}

			String log;
			int ec;
			err = OS::get_singleton()->execute("xcrun", args, &log, &ec, true);
			if (err != OK) {
				add_message(EXPORT_MESSAGE_WARNING, TTR("Run"), TTR("Could not start simctl executable."));
				CLEANUP_AND_RETURN(err);
			}
			if (ec != 0) {
				print_line("simctl launch:\n" + log);
				add_message(EXPORT_MESSAGE_ERROR, TTR("Run"), TTR("Running failed, see editor log for details."));
			}
		}
	} else if (dev.use_ios_deploy) {
		// Deploy and run on real device (via ios-deploy).
		if (ep.step("Installing and running on device...", 4)) {
			CLEANUP_AND_RETURN(ERR_SKIP);
		} else {
			List<String> args;
			args.push_back("-u");
			args.push_back("-I");
			args.push_back("--id");
			args.push_back(dev.id);
			args.push_back("--justlaunch");
			args.push_back("--bundle");
			args.push_back(EditorPaths::get_singleton()->get_temp_dir().path_join(id).path_join("export.xcarchive/Products/Applications/export.app"));
			String app_args;
			for (const String &E : cmd_args_list) {
				app_args += E + " ";
			}
			if (!app_args.is_empty()) {
				args.push_back("--args");
				args.push_back(app_args);
			}

			String idepl = EDITOR_GET("export/ios/ios_deploy");
			if (idepl.is_empty()) {
				idepl = "ios-deploy";
			}
			String log;
			int ec;
			err = OS::get_singleton()->execute(idepl, args, &log, &ec, true);
			if (err != OK) {
				add_message(EXPORT_MESSAGE_WARNING, TTR("Run"), TTR("Could not start ios-deploy executable."));
				CLEANUP_AND_RETURN(err);
			}
			if (ec != 0) {
				print_line("ios-deploy:\n" + log);
				add_message(EXPORT_MESSAGE_ERROR, TTR("Run"), TTR("Installation/running failed, see editor log for details."));
				CLEANUP_AND_RETURN(ERR_UNCONFIGURED);
			}
		}
	} else {
		// Deploy and run on real device.
		if (ep.step("Installing to device...", 3)) {
			CLEANUP_AND_RETURN(ERR_SKIP);
		} else {
			List<String> args;
			args.push_back("devicectl");
			args.push_back("device");
			args.push_back("install");
			args.push_back("app");
			args.push_back("-d");
			args.push_back(dev.id);
			args.push_back(EditorPaths::get_singleton()->get_temp_dir().path_join(id).path_join("export.xcarchive/Products/Applications/export.app"));

			String log;
			int ec;
			err = OS::get_singleton()->execute("xcrun", args, &log, &ec, true);
			if (err != OK) {
				add_message(EXPORT_MESSAGE_WARNING, TTR("Run"), TTR("Could not start device executable."));
				CLEANUP_AND_RETURN(err);
			}
			if (ec != 0) {
				print_line("device install:\n" + log);
				add_message(EXPORT_MESSAGE_ERROR, TTR("Run"), TTR("Installation failed, see editor log for details."));
				CLEANUP_AND_RETURN(ERR_UNCONFIGURED);
			}
		}

		if (ep.step("Running on device...", 4)) {
			CLEANUP_AND_RETURN(ERR_SKIP);
		} else {
			List<String> args;
			args.push_back("devicectl");
			args.push_back("device");
			args.push_back("process");
			args.push_back("launch");
			args.push_back("--terminate-existing");
			args.push_back("-d");
			args.push_back(dev.id);
			args.push_back(p_preset->get("application/bundle_identifier"));
			for (const String &E : cmd_args_list) {
				args.push_back(E);
			}

			String log;
			int ec;
			err = OS::get_singleton()->execute("xcrun", args, &log, &ec, true);
			if (err != OK) {
				add_message(EXPORT_MESSAGE_WARNING, TTR("Run"), TTR("Could not start devicectl executable."));
				CLEANUP_AND_RETURN(err);
			}
			if (ec != 0) {
				print_line("devicectl launch:\n" + log);
				add_message(EXPORT_MESSAGE_ERROR, TTR("Run"), TTR("Running failed, see editor log for details."));
			}
		}
	}

	CLEANUP_AND_RETURN(OK);

#undef CLEANUP_AND_RETURN
#else
	return ERR_UNCONFIGURED;
#endif
}

EditorExportPlatformIOS::EditorExportPlatformIOS() {
	if (EditorNode::get_singleton()) {
#ifdef MODULE_SVG_ENABLED
		Ref<Image> img = memnew(Image);
		const bool upsample = !Math::is_equal_approx(Math::round(EDSCALE), EDSCALE);

		ImageLoaderSVG::create_image_from_string(img, _ios_logo_svg, EDSCALE, upsample, false);
		logo = ImageTexture::create_from_image(img);

		ImageLoaderSVG::create_image_from_string(img, _ios_run_icon_svg, EDSCALE, upsample, false);
		run_icon = ImageTexture::create_from_image(img);
#endif

		plugins_changed.set();
		devices_changed.set();
#ifdef MACOS_ENABLED
		_update_preset_status();
		check_for_changes_thread.start(_check_for_changes_poll_thread, this);
#endif
	}
}

EditorExportPlatformIOS::~EditorExportPlatformIOS() {
#ifdef MACOS_ENABLED
	quit_request.set();
	if (check_for_changes_thread.is_started()) {
		check_for_changes_thread.wait_to_finish();
	}
#endif
}
