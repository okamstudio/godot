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

#include "core/io/image_loader.h"
#include "core/io/plist.h"
#include "core/string/translation.h"
#include "drivers/png/png_driver_common.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/editor_string_names.h"
#include "editor/export/codesign.h"
#include "editor/export/lipo.h"
#include "editor/export/macho.h"
#include "editor/import/resource_importer_texture_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/resources/image_texture.h"

#include "modules/modules_enabled.gen.h" // For svg and regex.
#ifdef MODULE_SVG_ENABLED
#include "modules/svg/image_loader_svg.h"
#endif

void EditorExportPlatformMacOS::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const {
	r_features->push_back(p_preset->get("binary_format/architecture"));
	String architecture = p_preset->get("binary_format/architecture");

	if (architecture == "universal" || architecture == "x86_64") {
		r_features->push_back("s3tc");
		r_features->push_back("bptc");
	} else if (architecture == "arm64") {
		r_features->push_back("etc2");
		r_features->push_back("astc");
	} else {
		ERR_PRINT("Invalid architecture");
	}
}

String EditorExportPlatformMacOS::get_export_option_warning(const EditorExportPreset *p_preset, const StringName &p_name) const {
	if (p_preset) {
		int dist_type = p_preset->get("export/distribution_type");
		bool ad_hoc = false;
		int codesign_tool = p_preset->get("codesign/codesign");
		int notary_tool = p_preset->get("notarization/notarization");
		switch (codesign_tool) {
			case 1: { // built-in ad-hoc
				ad_hoc = true;
			} break;
			case 2: { // "rcodesign"
				ad_hoc = p_preset->get_or_env("codesign/certificate_file", ENV_MAC_CODESIGN_CERT_FILE).operator String().is_empty() || p_preset->get_or_env("codesign/certificate_password", ENV_MAC_CODESIGN_CERT_FILE).operator String().is_empty();
			} break;
#ifdef MACOS_ENABLED
			case 3: { // "codesign"
				ad_hoc = (p_preset->get("codesign/identity") == "" || p_preset->get("codesign/identity") == "-");
			} break;
#endif
			default: {
			};
		}

		if (p_name == "application/bundle_identifier") {
			String identifier = p_preset->get("application/bundle_identifier");
			String pn_err;
			if (!is_package_name_valid(identifier, &pn_err)) {
				return TTR("Invalid bundle identifier:") + " " + pn_err;
			}
		}

		if (p_name == "codesign/certificate_file" || p_name == "codesign/certificate_password" || p_name == "codesign/identity") {
			if (dist_type == 2) {
				if (ad_hoc) {
					return TTR("App Store distribution with ad-hoc code signing is not supported.");
				}
			} else if (notary_tool > 0 && ad_hoc) {
				return TTR("Notarization with an ad-hoc signature is not supported.");
			}
		}

		if (p_name == "codesign/apple_team_id") {
			String team_id = p_preset->get("codesign/apple_team_id");
			if (team_id.is_empty()) {
				if (dist_type == 2) {
					return TTR("Apple Team ID is required for App Store distribution.");
				} else if (notary_tool > 0) {
					return TTR("Apple Team ID is required for notarization.");
				}
			}
		}

		if (p_name == "codesign/provisioning_profile" && dist_type == 2) {
			String pprof = p_preset->get_or_env("codesign/provisioning_profile", ENV_MAC_CODESIGN_PROFILE);
			if (pprof.is_empty()) {
				return TTR("Provisioning profile is required for App Store distribution.");
			}
		}

		if (p_name == "codesign/installer_identity" && dist_type == 2) {
			String ident = p_preset->get("codesign/installer_identity");
			if (ident.is_empty()) {
				return TTR("Installer signing identity is required for App Store distribution.");
			}
		}

		if (p_name == "codesign/entitlements/app_sandbox/enabled" && dist_type == 2) {
			bool sandbox = p_preset->get("codesign/entitlements/app_sandbox/enabled");
			if (!sandbox) {
				return TTR("App sandbox is required for App Store distribution.");
			}
		}

		if (p_name == "codesign/codesign") {
			if (dist_type == 2) {
				if (codesign_tool == 2 && ClassDB::class_exists("CSharpScript")) {
					return TTR("'rcodesign' doesn't support signing applications with embedded dynamic libraries (GDExtension or .NET).");
				}
				if (codesign_tool == 0) {
					return TTR("Code signing is required for App Store distribution.");
				}
				if (codesign_tool == 1) {
					return TTR("App Store distribution with ad-hoc code signing is not supported.");
				}
			} else if (notary_tool > 0) {
				if (codesign_tool == 0) {
					return TTR("Code signing is required for notarization.");
				}
				if (codesign_tool == 1) {
					return TTR("Notarization with an ad-hoc signature is not supported.");
				}
			}
		}

		if (notary_tool == 2 || notary_tool == 3) {
			if (p_name == "notarization/apple_id_name" || p_name == "notarization/api_uuid") {
				String apple_id = p_preset->get_or_env("notarization/apple_id_name", ENV_MAC_NOTARIZATION_APPLE_ID);
				String api_uuid = p_preset->get_or_env("notarization/api_uuid", ENV_MAC_NOTARIZATION_UUID);
				if (apple_id.is_empty() && api_uuid.is_empty()) {
					return TTR("Neither Apple ID name nor App Store Connect issuer ID name not specified.");
				}
				if (!apple_id.is_empty() && !api_uuid.is_empty()) {
					return TTR("Both Apple ID name and App Store Connect issuer ID name are specified, only one should be set at the same time.");
				}
			}
			if (p_name == "notarization/apple_id_password") {
				String apple_id = p_preset->get_or_env("notarization/apple_id_name", ENV_MAC_NOTARIZATION_APPLE_ID);
				String apple_pass = p_preset->get_or_env("notarization/apple_id_password", ENV_MAC_NOTARIZATION_APPLE_PASS);
				if (!apple_id.is_empty() && apple_pass.is_empty()) {
					return TTR("Apple ID password not specified.");
				}
			}
			if (p_name == "notarization/api_key_id") {
				String api_uuid = p_preset->get_or_env("notarization/api_uuid", ENV_MAC_NOTARIZATION_UUID);
				String api_key = p_preset->get_or_env("notarization/api_key_id", ENV_MAC_NOTARIZATION_KEY_ID);
				if (!api_uuid.is_empty() && api_key.is_empty()) {
					return TTR("App Store Connect API key ID not specified.");
				}
			}
		} else if (notary_tool == 1) {
			if (p_name == "notarization/api_uuid") {
				String api_uuid = p_preset->get_or_env("notarization/api_uuid", ENV_MAC_NOTARIZATION_UUID);
				if (api_uuid.is_empty()) {
					return TTR("App Store Connect issuer ID name not specified.");
				}
			}
			if (p_name == "notarization/api_key_id") {
				String api_key = p_preset->get_or_env("notarization/api_key_id", ENV_MAC_NOTARIZATION_KEY_ID);
				if (api_key.is_empty()) {
					return TTR("App Store Connect API key ID not specified.");
				}
			}
		}

		if (codesign_tool > 0) {
			if (p_name == "privacy/microphone_usage_description") {
				String discr = p_preset->get("privacy/microphone_usage_description");
				bool enabled = p_preset->get("codesign/entitlements/audio_input");
				if (enabled && discr.is_empty()) {
					return TTR("Microphone access is enabled, but usage description is not specified.");
				}
			}
			if (p_name == "privacy/camera_usage_description") {
				String discr = p_preset->get("privacy/camera_usage_description");
				bool enabled = p_preset->get("codesign/entitlements/camera");
				if (enabled && discr.is_empty()) {
					return TTR("Camera access is enabled, but usage description is not specified.");
				}
			}
			if (p_name == "privacy/location_usage_description") {
				String discr = p_preset->get("privacy/location_usage_description");
				bool enabled = p_preset->get("codesign/entitlements/location");
				if (enabled && discr.is_empty()) {
					return TTR("Location information access is enabled, but usage description is not specified.");
				}
			}
			if (p_name == "privacy/address_book_usage_description") {
				String discr = p_preset->get("privacy/address_book_usage_description");
				bool enabled = p_preset->get("codesign/entitlements/address_book");
				if (enabled && discr.is_empty()) {
					return TTR("Address book access is enabled, but usage description is not specified.");
				}
			}
			if (p_name == "privacy/calendar_usage_description") {
				String discr = p_preset->get("privacy/calendar_usage_description");
				bool enabled = p_preset->get("codesign/entitlements/calendars");
				if (enabled && discr.is_empty()) {
					return TTR("Calendar access is enabled, but usage description is not specified.");
				}
			}
			if (p_name == "privacy/photos_library_usage_description") {
				String discr = p_preset->get("privacy/photos_library_usage_description");
				bool enabled = p_preset->get("codesign/entitlements/photos_library");
				if (enabled && discr.is_empty()) {
					return TTR("Photo library access is enabled, but usage description is not specified.");
				}
			}
		}
	}
	return String();
}

bool EditorExportPlatformMacOS::get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const {
	// Hide irrelevant code signing options.
	if (p_preset) {
		int codesign_tool = p_preset->get("codesign/codesign");
		switch (codesign_tool) {
			case 1: { // built-in ad-hoc
				if (p_option == "codesign/identity" || p_option == "codesign/certificate_file" || p_option == "codesign/certificate_password" || p_option == "codesign/custom_options" || p_option == "codesign/team_id") {
					return false;
				}
			} break;
			case 2: { // "rcodesign"
				if (p_option == "codesign/identity") {
					return false;
				}
			} break;
#ifdef MACOS_ENABLED
			case 3: { // "codesign"
				if (p_option == "codesign/certificate_file" || p_option == "codesign/certificate_password") {
					return false;
				}
			} break;
#endif
			default: { // disabled
				if (p_option == "codesign/identity" || p_option == "codesign/certificate_file" || p_option == "codesign/certificate_password" || p_option == "codesign/custom_options" || p_option.begins_with("codesign/entitlements") || p_option == "codesign/team_id") {
					return false;
				}
			} break;
		}

		// Distribution type.
		int dist_type = p_preset->get("export/distribution_type");
		if (dist_type != 2 && p_option == "codesign/installer_identity") {
			return false;
		}

		if (dist_type == 2 && p_option.begins_with("notarization/")) {
			return false;
		}

		if (dist_type != 2 && p_option == "codesign/provisioning_profile") {
			return false;
		}

		String custom_prof = p_preset->get("codesign/entitlements/custom_file");
		if (!custom_prof.is_empty() && p_option != "codesign/entitlements/custom_file" && p_option.begins_with("codesign/entitlements/")) {
			return false;
		}

		// Hide sandbox entitlements.
		bool sandbox = p_preset->get("codesign/entitlements/app_sandbox/enabled");
		if (!sandbox && p_option != "codesign/entitlements/app_sandbox/enabled" && p_option.begins_with("codesign/entitlements/app_sandbox/")) {
			return false;
		}

		// Hide SSH options.
		bool ssh = p_preset->get("ssh_remote_deploy/enabled");
		if (!ssh && p_option != "ssh_remote_deploy/enabled" && p_option.begins_with("ssh_remote_deploy/")) {
			return false;
		}

		// Hide irrelevant notarization options.
		int notary_tool = p_preset->get("notarization/notarization");
		switch (notary_tool) {
			case 1: { // "rcodesign"
				if (p_option == "notarization/apple_id_name" || p_option == "notarization/apple_id_password") {
					return false;
				}
			} break;
			case 2: { // "notarytool"
				// All options are visible.
			} break;
			default: { // disabled
				if (p_option == "notarization/apple_id_name" || p_option == "notarization/apple_id_password" || p_option == "notarization/api_uuid" || p_option == "notarization/api_key" || p_option == "notarization/api_key_id") {
					return false;
				}
			} break;
		}

		bool advanced_options_enabled = p_preset->are_advanced_options_enabled();
		if (p_option.begins_with("privacy") || p_option == "codesign/entitlements/additional") {
			return advanced_options_enabled;
		}
	}

	// These entitlements are required to run managed code, and are always enabled in Mono builds.
	if (ClassDB::class_exists("CSharpScript")) {
		if (p_option == "codesign/entitlements/allow_jit_code_execution" || p_option == "codesign/entitlements/allow_unsigned_executable_memory" || p_option == "codesign/entitlements/allow_dyld_environment_variables") {
			return false;
		}
	}

	// Hide unsupported .NET embedding option.
	if (p_option == "dotnet/embed_build_outputs") {
		return false;
	}

	return true;
}

List<String> EditorExportPlatformMacOS::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> list;

	if (p_preset.is_valid()) {
		int dist_type = p_preset->get("export/distribution_type");
		if (dist_type == 0) {
#ifdef MACOS_ENABLED
			list.push_back("dmg");
#endif
			list.push_back("zip");
			list.push_back("app");
		} else if (dist_type == 1) {
#ifdef MACOS_ENABLED
			list.push_back("dmg");
#endif
			list.push_back("zip");
			list.push_back("app");
		} else if (dist_type == 2) {
#ifdef MACOS_ENABLED
			list.push_back("pkg");
#endif
		}
	}

	return list;
}

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

void EditorExportPlatformMacOS::get_export_options(List<ExportOption> *r_options) const {
#ifdef MACOS_ENABLED
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "export/distribution_type", PROPERTY_HINT_ENUM, "Testing,Distribution,App Store"), 1, true));
#else
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "export/distribution_type", PROPERTY_HINT_ENUM, "Testing,Distribution"), 1, true));
#endif

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "binary_format/architecture", PROPERTY_HINT_ENUM, "universal,x86_64,arm64", PROPERTY_USAGE_STORAGE), "universal"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_template/debug", PROPERTY_HINT_GLOBAL_FILE, "*.zip"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_template/release", PROPERTY_HINT_GLOBAL_FILE, "*.zip"), ""));

	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "debug/export_console_wrapper", PROPERTY_HINT_ENUM, "No,Debug Only,Debug and Release"), 1));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/icon", PROPERTY_HINT_FILE, "*.icns,*.png,*.webp,*.svg"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "application/icon_interpolation", PROPERTY_HINT_ENUM, "Nearest neighbor,Bilinear,Cubic,Trilinear,Lanczos"), 4));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/bundle_identifier", PROPERTY_HINT_PLACEHOLDER_TEXT, "com.example.game"), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/signature"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/app_category", PROPERTY_HINT_ENUM, "Business,Developer-tools,Education,Entertainment,Finance,Games,Action-games,Adventure-games,Arcade-games,Board-games,Card-games,Casino-games,Dice-games,Educational-games,Family-games,Kids-games,Music-games,Puzzle-games,Racing-games,Role-playing-games,Simulation-games,Sports-games,Strategy-games,Trivia-games,Word-games,Graphics-design,Healthcare-fitness,Lifestyle,Medical,Music,News,Photography,Productivity,Reference,Social-networking,Sports,Travel,Utilities,Video,Weather"), "Games"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/short_version", PROPERTY_HINT_PLACEHOLDER_TEXT, "Leave empty to use project version"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/version", PROPERTY_HINT_PLACEHOLDER_TEXT, "Leave empty to use project version"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/copyright"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::DICTIONARY, "application/copyright_localized", PROPERTY_HINT_LOCALIZABLE_STRING), Dictionary()));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/min_macos_version_x86_64"), "10.12"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/min_macos_version_arm64"), "11.00"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "application/export_angle", PROPERTY_HINT_ENUM, "Auto,Yes,No"), 0, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "display/high_res"), true));

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/additional_plist_content", PROPERTY_HINT_MULTILINE_TEXT), ""));

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "xcode/platform_build"), "14C18"));
	// TODO(sgc): Need to set appropriate version when using Metal
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "xcode/sdk_version"), "13.1"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "xcode/sdk_build"), "22C55"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "xcode/sdk_name"), "macosx13.1"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "xcode/xcode_version"), "1420"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "xcode/xcode_build"), "14C18"));

#ifdef MACOS_ENABLED
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "codesign/codesign", PROPERTY_HINT_ENUM, "Disabled,Built-in (ad-hoc only),rcodesign,Xcode codesign"), 3, true));
#else
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "codesign/codesign", PROPERTY_HINT_ENUM, "Disabled,Built-in (ad-hoc only),rcodesign"), 1, true, true));
#endif
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "codesign/installer_identity", PROPERTY_HINT_PLACEHOLDER_TEXT, "3rd Party Mac Developer Installer: (ID)"), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "codesign/apple_team_id", PROPERTY_HINT_PLACEHOLDER_TEXT, "ID"), "", false, true));
	// "codesign" only options:
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "codesign/identity", PROPERTY_HINT_PLACEHOLDER_TEXT, "Type: Name (ID)"), ""));
	// "rcodesign" only options:
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "codesign/certificate_file", PROPERTY_HINT_GLOBAL_FILE, "*.pfx,*.p12", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "codesign/certificate_password", PROPERTY_HINT_PASSWORD, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), ""));
	// "codesign" and "rcodesign" only options:
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "codesign/provisioning_profile", PROPERTY_HINT_GLOBAL_FILE, "*.provisionprofile", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), "", false, true));

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "codesign/entitlements/custom_file", PROPERTY_HINT_GLOBAL_FILE, "*.plist"), "", true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/entitlements/allow_jit_code_execution"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/entitlements/allow_unsigned_executable_memory"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/entitlements/allow_dyld_environment_variables"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/entitlements/disable_library_validation"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/entitlements/audio_input"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/entitlements/camera"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/entitlements/location"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/entitlements/address_book"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/entitlements/calendars"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/entitlements/photos_library"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/entitlements/apple_events"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/entitlements/debugging"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/entitlements/app_sandbox/enabled"), false, true, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/entitlements/app_sandbox/network_server"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/entitlements/app_sandbox/network_client"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/entitlements/app_sandbox/device_usb"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/entitlements/app_sandbox/device_bluetooth"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "codesign/entitlements/app_sandbox/files_downloads", PROPERTY_HINT_ENUM, "No,Read-only,Read-write"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "codesign/entitlements/app_sandbox/files_pictures", PROPERTY_HINT_ENUM, "No,Read-only,Read-write"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "codesign/entitlements/app_sandbox/files_music", PROPERTY_HINT_ENUM, "No,Read-only,Read-write"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "codesign/entitlements/app_sandbox/files_movies", PROPERTY_HINT_ENUM, "No,Read-only,Read-write"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "codesign/entitlements/app_sandbox/files_user_selected", PROPERTY_HINT_ENUM, "No,Read-only,Read-write"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::ARRAY, "codesign/entitlements/app_sandbox/helper_executables", PROPERTY_HINT_ARRAY_TYPE, itos(Variant::STRING) + "/" + itos(PROPERTY_HINT_GLOBAL_FILE) + ":"), Array()));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "codesign/entitlements/additional", PROPERTY_HINT_MULTILINE_TEXT), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::PACKED_STRING_ARRAY, "codesign/custom_options"), PackedStringArray()));

#ifdef MACOS_ENABLED
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "notarization/notarization", PROPERTY_HINT_ENUM, "Disabled,rcodesign,Xcode notarytool"), 0, true));
#else
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "notarization/notarization", PROPERTY_HINT_ENUM, "Disabled,rcodesign"), 0, true));
#endif
	// "notarytool" only options:
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "notarization/apple_id_name", PROPERTY_HINT_PLACEHOLDER_TEXT, "Apple ID email", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "notarization/apple_id_password", PROPERTY_HINT_PASSWORD, "Enable two-factor authentication and provide app-specific password", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), "", false, true));
	// "notarytool" and "rcodesign" only options:
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "notarization/api_uuid", PROPERTY_HINT_PLACEHOLDER_TEXT, "App Store Connect issuer ID UUID", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "notarization/api_key", PROPERTY_HINT_GLOBAL_FILE, "*.p8", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "notarization/api_key_id", PROPERTY_HINT_PLACEHOLDER_TEXT, "App Store Connect API key ID", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), "", false, true));

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "privacy/microphone_usage_description", PROPERTY_HINT_PLACEHOLDER_TEXT, "Provide a message if you need to use the microphone"), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::DICTIONARY, "privacy/microphone_usage_description_localized", PROPERTY_HINT_LOCALIZABLE_STRING), Dictionary()));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "privacy/camera_usage_description", PROPERTY_HINT_PLACEHOLDER_TEXT, "Provide a message if you need to use the camera"), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::DICTIONARY, "privacy/camera_usage_description_localized", PROPERTY_HINT_LOCALIZABLE_STRING), Dictionary()));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "privacy/location_usage_description", PROPERTY_HINT_PLACEHOLDER_TEXT, "Provide a message if you need to use the location information"), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::DICTIONARY, "privacy/location_usage_description_localized", PROPERTY_HINT_LOCALIZABLE_STRING), Dictionary()));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "privacy/address_book_usage_description", PROPERTY_HINT_PLACEHOLDER_TEXT, "Provide a message if you need to use the address book"), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::DICTIONARY, "privacy/address_book_usage_description_localized", PROPERTY_HINT_LOCALIZABLE_STRING), Dictionary()));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "privacy/calendar_usage_description", PROPERTY_HINT_PLACEHOLDER_TEXT, "Provide a message if you need to use the calendar"), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::DICTIONARY, "privacy/calendar_usage_description_localized", PROPERTY_HINT_LOCALIZABLE_STRING), Dictionary()));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "privacy/photos_library_usage_description", PROPERTY_HINT_PLACEHOLDER_TEXT, "Provide a message if you need to use the photo library"), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::DICTIONARY, "privacy/photos_library_usage_description_localized", PROPERTY_HINT_LOCALIZABLE_STRING), Dictionary()));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "privacy/desktop_folder_usage_description", PROPERTY_HINT_PLACEHOLDER_TEXT, "Provide a message if you need to use Desktop folder"), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::DICTIONARY, "privacy/desktop_folder_usage_description_localized", PROPERTY_HINT_LOCALIZABLE_STRING), Dictionary()));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "privacy/documents_folder_usage_description", PROPERTY_HINT_PLACEHOLDER_TEXT, "Provide a message if you need to use Documents folder"), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::DICTIONARY, "privacy/documents_folder_usage_description_localized", PROPERTY_HINT_LOCALIZABLE_STRING), Dictionary()));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "privacy/downloads_folder_usage_description", PROPERTY_HINT_PLACEHOLDER_TEXT, "Provide a message if you need to use Downloads folder"), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::DICTIONARY, "privacy/downloads_folder_usage_description_localized", PROPERTY_HINT_LOCALIZABLE_STRING), Dictionary()));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "privacy/network_volumes_usage_description", PROPERTY_HINT_PLACEHOLDER_TEXT, "Provide a message if you need to use network volumes"), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::DICTIONARY, "privacy/network_volumes_usage_description_localized", PROPERTY_HINT_LOCALIZABLE_STRING), Dictionary()));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "privacy/removable_volumes_usage_description", PROPERTY_HINT_PLACEHOLDER_TEXT, "Provide a message if you need to use removable volumes"), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::DICTIONARY, "privacy/removable_volumes_usage_description_localized", PROPERTY_HINT_LOCALIZABLE_STRING), Dictionary()));

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

	String run_script = "#!/usr/bin/env bash\n"
						"unzip -o -q \"{temp_dir}/{archive_name}\" -d \"{temp_dir}\"\n"
						"open \"{temp_dir}/{exe_name}.app\" --args {cmd_args}";

	String cleanup_script = "#!/usr/bin/env bash\n"
							"kill $(pgrep -x -f \"{temp_dir}/{exe_name}.app/Contents/MacOS/{exe_name} {cmd_args}\")\n"
							"rm -rf \"{temp_dir}\"";

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "ssh_remote_deploy/enabled"), false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ssh_remote_deploy/host"), "user@host_ip"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ssh_remote_deploy/port"), "22"));

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ssh_remote_deploy/extra_args_ssh", PROPERTY_HINT_MULTILINE_TEXT), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ssh_remote_deploy/extra_args_scp", PROPERTY_HINT_MULTILINE_TEXT), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ssh_remote_deploy/run_script", PROPERTY_HINT_MULTILINE_TEXT), run_script));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ssh_remote_deploy/cleanup_script", PROPERTY_HINT_MULTILINE_TEXT), cleanup_script));
}

void _rgba8_to_packbits_encode(int p_ch, int p_size, Vector<uint8_t> &p_source, Vector<uint8_t> &p_dest) {
	int src_len = p_size * p_size;

	Vector<uint8_t> result;

	int i = 0;
	const uint8_t *src = p_source.ptr();
	while (i < src_len) {
		Vector<uint8_t> seq;

		uint8_t count = 0;
		while (count <= 0x7f && i < src_len) {
			if (i + 2 < src_len && src[i * 4 + p_ch] == src[(i + 1) * 4 + p_ch] && src[i] == src[(i + 2) * 4 + p_ch]) {
				break;
			}
			seq.push_back(src[i * 4 + p_ch]);
			i++;
			count++;
		}
		if (!seq.is_empty()) {
			result.push_back(count - 1);
			result.append_array(seq);
		}
		if (i >= src_len) {
			break;
		}

		uint8_t rep = src[i * 4 + p_ch];
		count = 0;
		while (count <= 0x7f && i < src_len && src[i * 4 + p_ch] == rep) {
			i++;
			count++;
		}
		if (count >= 3) {
			result.push_back(0x80 + count - 3);
			result.push_back(rep);
		} else {
			result.push_back(count - 1);
			for (int j = 0; j < count; j++) {
				result.push_back(rep);
			}
		}
	}

	int ofs = p_dest.size();
	p_dest.resize(p_dest.size() + result.size());
	memcpy(&p_dest.write[ofs], result.ptr(), result.size());
}

void EditorExportPlatformMacOS::_make_icon(const Ref<EditorExportPreset> &p_preset, const Ref<Image> &p_icon, Vector<uint8_t> &p_data) {
	Vector<uint8_t> data;

	data.resize(8);
	data.write[0] = 'i';
	data.write[1] = 'c';
	data.write[2] = 'n';
	data.write[3] = 's';

	struct MacOSIconInfo {
		const char *name;
		const char *mask_name;
		bool is_png;
		int size;
	};

	static const MacOSIconInfo icon_infos[] = {
		{ "ic10", "", true, 1024 }, //1024×1024 32-bit PNG and 512×512@2x 32-bit "retina" PNG
		{ "ic09", "", true, 512 }, //512×512 32-bit PNG
		{ "ic14", "", true, 512 }, //256×256@2x 32-bit "retina" PNG
		{ "ic08", "", true, 256 }, //256×256 32-bit PNG
		{ "ic13", "", true, 256 }, //128×128@2x 32-bit "retina" PNG
		{ "ic07", "", true, 128 }, //128×128 32-bit PNG
		{ "ic12", "", true, 64 }, //32×32@2× 32-bit "retina" PNG
		{ "ic11", "", true, 32 }, //16×16@2× 32-bit "retina" PNG
		{ "il32", "l8mk", false, 32 }, //32×32 24-bit RLE + 8-bit uncompressed mask
		{ "is32", "s8mk", false, 16 } //16×16 24-bit RLE + 8-bit uncompressed mask
	};

	for (uint64_t i = 0; i < (sizeof(icon_infos) / sizeof(icon_infos[0])); ++i) {
		Ref<Image> copy = p_icon->duplicate();
		copy->convert(Image::FORMAT_RGBA8);
		copy->resize(icon_infos[i].size, icon_infos[i].size, (Image::Interpolation)(p_preset->get("application/icon_interpolation").operator int()));

		if (icon_infos[i].is_png) {
			// Encode PNG icon.
			Vector<uint8_t> png_buffer;
			Error err = PNGDriverCommon::image_to_png(copy, png_buffer);
			if (err == OK) {
				int ofs = data.size();
				uint64_t len = png_buffer.size();
				data.resize(data.size() + len + 8);
				memcpy(&data.write[ofs + 8], png_buffer.ptr(), len);
				len += 8;
				len = BSWAP32(len);
				memcpy(&data.write[ofs], icon_infos[i].name, 4);
				encode_uint32(len, &data.write[ofs + 4]);
			}
		} else {
			Vector<uint8_t> src_data = copy->get_data();

			// Encode 24-bit RGB RLE icon.
			{
				int ofs = data.size();
				data.resize(data.size() + 8);

				_rgba8_to_packbits_encode(0, icon_infos[i].size, src_data, data); // Encode R.
				_rgba8_to_packbits_encode(1, icon_infos[i].size, src_data, data); // Encode G.
				_rgba8_to_packbits_encode(2, icon_infos[i].size, src_data, data); // Encode B.

				// Note: workaround for macOS icon decoder bug corrupting last RLE encoded value.
				data.push_back(0x00);

				int len = data.size() - ofs;
				len = BSWAP32(len);
				memcpy(&data.write[ofs], icon_infos[i].name, 4);
				encode_uint32(len, &data.write[ofs + 4]);
			}

			// Encode 8-bit mask uncompressed icon.
			{
				int ofs = data.size();
				int len = copy->get_width() * copy->get_height();
				data.resize(data.size() + len + 8);

				for (int j = 0; j < len; j++) {
					data.write[ofs + 8 + j] = src_data.ptr()[j * 4 + 3];
				}
				len += 8;
				len = BSWAP32(len);
				memcpy(&data.write[ofs], icon_infos[i].mask_name, 4);
				encode_uint32(len, &data.write[ofs + 4]);
			}
		}
	}

	uint32_t total_len = data.size();
	total_len = BSWAP32(total_len);
	encode_uint32(total_len, &data.write[4]);

	p_data = data;
}

void EditorExportPlatformMacOS::_fix_privacy_manifest(const Ref<EditorExportPreset> &p_preset, Vector<uint8_t> &plist) {
	String str;
	String strnew;
	str.parse_utf8((const char *)plist.ptr(), plist.size());
	Vector<String> lines = str.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		if (lines[i].find("$priv_collection") != -1) {
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
		} else if (lines[i].find("$priv_tracking") != -1) {
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
		} else {
			strnew += lines[i] + "\n";
		}
	}

	CharString cs = strnew.utf8();
	plist.resize(cs.size() - 1);
	for (int i = 0; i < cs.size() - 1; i++) {
		plist.write[i] = cs[i];
	}
}

void EditorExportPlatformMacOS::_fix_plist(const Ref<EditorExportPreset> &p_preset, Vector<uint8_t> &plist, const String &p_binary) {
	String str;
	String strnew;
	str.parse_utf8((const char *)plist.ptr(), plist.size());
	Vector<String> lines = str.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		if (lines[i].contains("$binary")) {
			strnew += lines[i].replace("$binary", p_binary) + "\n";
		} else if (lines[i].contains("$name")) {
			strnew += lines[i].replace("$name", GLOBAL_GET("application/config/name")) + "\n";
		} else if (lines[i].contains("$bundle_identifier")) {
			strnew += lines[i].replace("$bundle_identifier", p_preset->get("application/bundle_identifier")) + "\n";
		} else if (lines[i].contains("$short_version")) {
			strnew += lines[i].replace("$short_version", p_preset->get_version("application/short_version")) + "\n";
		} else if (lines[i].contains("$version")) {
			strnew += lines[i].replace("$version", p_preset->get_version("application/version")) + "\n";
		} else if (lines[i].contains("$signature")) {
			strnew += lines[i].replace("$signature", p_preset->get("application/signature")) + "\n";
		} else if (lines[i].contains("$app_category")) {
			String cat = p_preset->get("application/app_category");
			strnew += lines[i].replace("$app_category", cat.to_lower()) + "\n";
		} else if (lines[i].contains("$copyright")) {
			strnew += lines[i].replace("$copyright", p_preset->get("application/copyright")) + "\n";
		} else if (lines[i].contains("$min_version_arm64")) {
			strnew += lines[i].replace("$min_version_arm64", p_preset->get("application/min_macos_version_arm64")) + "\n";
		} else if (lines[i].contains("$min_version_x86_64")) {
			strnew += lines[i].replace("$min_version_x86_64", p_preset->get("application/min_macos_version_x86_64")) + "\n";
		} else if (lines[i].contains("$min_version")) {
			strnew += lines[i].replace("$min_version", p_preset->get("application/min_macos_version_x86_64")) + "\n"; // Old template, use x86-64 version for both.
		} else if (lines[i].contains("$highres")) {
			strnew += lines[i].replace("$highres", p_preset->get("display/high_res") ? "\t<true/>" : "\t<false/>") + "\n";
		} else if (lines[i].contains("$additional_plist_content")) {
			strnew += lines[i].replace("$additional_plist_content", p_preset->get("application/additional_plist_content")) + "\n";
		} else if (lines[i].contains("$platfbuild")) {
			strnew += lines[i].replace("$platfbuild", p_preset->get("xcode/platform_build")) + "\n";
		} else if (lines[i].contains("$sdkver")) {
			strnew += lines[i].replace("$sdkver", p_preset->get("xcode/sdk_version")) + "\n";
		} else if (lines[i].contains("$sdkname")) {
			strnew += lines[i].replace("$sdkname", p_preset->get("xcode/sdk_name")) + "\n";
		} else if (lines[i].contains("$sdkbuild")) {
			strnew += lines[i].replace("$sdkbuild", p_preset->get("xcode/sdk_build")) + "\n";
		} else if (lines[i].contains("$xcodever")) {
			strnew += lines[i].replace("$xcodever", p_preset->get("xcode/xcode_version")) + "\n";
		} else if (lines[i].contains("$xcodebuild")) {
			strnew += lines[i].replace("$xcodebuild", p_preset->get("xcode/xcode_build")) + "\n";
		} else if (lines[i].contains("$usage_descriptions")) {
			String descriptions;
			if (!((String)p_preset->get("privacy/microphone_usage_description")).is_empty()) {
				descriptions += "\t<key>NSMicrophoneUsageDescription</key>\n";
				descriptions += "\t<string>" + (String)p_preset->get("privacy/microphone_usage_description") + "</string>\n";
			}
			if (!((String)p_preset->get("privacy/camera_usage_description")).is_empty()) {
				descriptions += "\t<key>NSCameraUsageDescription</key>\n";
				descriptions += "\t<string>" + (String)p_preset->get("privacy/camera_usage_description") + "</string>\n";
			}
			if (!((String)p_preset->get("privacy/location_usage_description")).is_empty()) {
				descriptions += "\t<key>NSLocationUsageDescription</key>\n";
				descriptions += "\t<string>" + (String)p_preset->get("privacy/location_usage_description") + "</string>\n";
			}
			if (!((String)p_preset->get("privacy/address_book_usage_description")).is_empty()) {
				descriptions += "\t<key>NSContactsUsageDescription</key>\n";
				descriptions += "\t<string>" + (String)p_preset->get("privacy/address_book_usage_description") + "</string>\n";
			}
			if (!((String)p_preset->get("privacy/calendar_usage_description")).is_empty()) {
				descriptions += "\t<key>NSCalendarsUsageDescription</key>\n";
				descriptions += "\t<string>" + (String)p_preset->get("privacy/calendar_usage_description") + "</string>\n";
			}
			if (!((String)p_preset->get("privacy/photos_library_usage_description")).is_empty()) {
				descriptions += "\t<key>NSPhotoLibraryUsageDescription</key>\n";
				descriptions += "\t<string>" + (String)p_preset->get("privacy/photos_library_usage_description") + "</string>\n";
			}
			if (!((String)p_preset->get("privacy/desktop_folder_usage_description")).is_empty()) {
				descriptions += "\t<key>NSDesktopFolderUsageDescription</key>\n";
				descriptions += "\t<string>" + (String)p_preset->get("privacy/desktop_folder_usage_description") + "</string>\n";
			}
			if (!((String)p_preset->get("privacy/documents_folder_usage_description")).is_empty()) {
				descriptions += "\t<key>NSDocumentsFolderUsageDescription</key>\n";
				descriptions += "\t<string>" + (String)p_preset->get("privacy/documents_folder_usage_description") + "</string>\n";
			}
			if (!((String)p_preset->get("privacy/downloads_folder_usage_description")).is_empty()) {
				descriptions += "\t<key>NSDownloadsFolderUsageDescription</key>\n";
				descriptions += "\t<string>" + (String)p_preset->get("privacy/downloads_folder_usage_description") + "</string>\n";
			}
			if (!((String)p_preset->get("privacy/network_volumes_usage_description")).is_empty()) {
				descriptions += "\t<key>NSNetworkVolumesUsageDescription</key>\n";
				descriptions += "\t<string>" + (String)p_preset->get("privacy/network_volumes_usage_description") + "</string>\n";
			}
			if (!((String)p_preset->get("privacy/removable_volumes_usage_description")).is_empty()) {
				descriptions += "\t<key>NSRemovableVolumesUsageDescription</key>\n";
				descriptions += "\t<string>" + (String)p_preset->get("privacy/removable_volumes_usage_description") + "</string>\n";
			}
			if (!descriptions.is_empty()) {
				strnew += lines[i].replace("$usage_descriptions", descriptions);
			}
		} else {
			strnew += lines[i] + "\n";
		}
	}

	CharString cs = strnew.utf8();
	plist.resize(cs.size() - 1);
	for (int i = 0; i < cs.size() - 1; i++) {
		plist.write[i] = cs[i];
	}
}

/**
 * If we're running the macOS version of the Godot editor we'll:
 * - export our application bundle to a temporary folder
 * - attempt to code sign it
 * - and then wrap it up in a DMG
 */

Error EditorExportPlatformMacOS::_notarize(const Ref<EditorExportPreset> &p_preset, const String &p_path) {
	int notary_tool = p_preset->get("notarization/notarization");
	switch (notary_tool) {
		case 1: { // "rcodesign"
			print_verbose("using rcodesign notarization...");

			String rcodesign = EDITOR_GET("export/macos/rcodesign").operator String();
			if (rcodesign.is_empty()) {
				add_message(EXPORT_MESSAGE_ERROR, TTR("Notarization"), TTR("rcodesign path is not set. Configure rcodesign path in the Editor Settings (Export > macOS > rcodesign)."));
				return Error::FAILED;
			}

			List<String> args;

			args.push_back("notary-submit");

			if (p_preset->get_or_env("notarization/api_uuid", ENV_MAC_NOTARIZATION_UUID) == "") {
				add_message(EXPORT_MESSAGE_ERROR, TTR("Notarization"), TTR("App Store Connect issuer ID name not specified."));
				return Error::FAILED;
			}
			if (p_preset->get_or_env("notarization/api_key", ENV_MAC_NOTARIZATION_KEY) == "") {
				add_message(EXPORT_MESSAGE_ERROR, TTR("Notarization"), TTR("App Store Connect API key ID not specified."));
				return Error::FAILED;
			}

			args.push_back("--api-issuer");
			args.push_back(p_preset->get_or_env("notarization/api_uuid", ENV_MAC_NOTARIZATION_UUID));

			args.push_back("--api-key");
			args.push_back(p_preset->get_or_env("notarization/api_key_id", ENV_MAC_NOTARIZATION_KEY_ID));

			if (!p_preset->get_or_env("notarization/api_key", ENV_MAC_NOTARIZATION_KEY).operator String().is_empty()) {
				args.push_back("--api-key-path");
				args.push_back(p_preset->get_or_env("notarization/api_key", ENV_MAC_NOTARIZATION_KEY));
			}

			args.push_back(p_path);

			String str;
			int exitcode = 0;

			Error err = OS::get_singleton()->execute(rcodesign, args, &str, &exitcode, true);
			if (err != OK) {
				add_message(EXPORT_MESSAGE_WARNING, TTR("Notarization"), TTR("Could not start rcodesign executable."));
				return err;
			}

			int rq_offset = str.find("created submission ID:");
			if (exitcode != 0 || rq_offset == -1) {
				print_line("rcodesign (" + p_path + "):\n" + str);
				add_message(EXPORT_MESSAGE_WARNING, TTR("Notarization"), TTR("Notarization failed, see editor log for details."));
				return Error::FAILED;
			} else {
				print_verbose("rcodesign (" + p_path + "):\n" + str);
				int next_nl = str.find("\n", rq_offset);
				String request_uuid = (next_nl == -1) ? str.substr(rq_offset + 23, -1) : str.substr(rq_offset + 23, next_nl - rq_offset - 23);
				add_message(EXPORT_MESSAGE_INFO, TTR("Notarization"), vformat(TTR("Notarization request UUID: \"%s\""), request_uuid));
				add_message(EXPORT_MESSAGE_INFO, TTR("Notarization"), TTR("The notarization process generally takes less than an hour."));
				add_message(EXPORT_MESSAGE_INFO, TTR("Notarization"), "\t" + TTR("You can check progress manually by opening a Terminal and running the following command:"));
				add_message(EXPORT_MESSAGE_INFO, TTR("Notarization"), "\t\t\"rcodesign notary-log --api-issuer <api uuid> --api-key <api key> <request uuid>\"");
				add_message(EXPORT_MESSAGE_INFO, TTR("Notarization"), "\t" + TTR("Run the following command to staple the notarization ticket to the exported application (optional):"));
				add_message(EXPORT_MESSAGE_INFO, TTR("Notarization"), "\t\t\"rcodesign staple <app path>\"");
			}
		} break;
#ifdef MACOS_ENABLED
		case 2: { // "notarytool"
			print_verbose("using notarytool notarization...");

			if (!FileAccess::exists("/usr/bin/xcrun") && !FileAccess::exists("/bin/xcrun")) {
				add_message(EXPORT_MESSAGE_ERROR, TTR("Notarization"), TTR("Xcode command line tools are not installed."));
				return Error::FAILED;
			}

			List<String> args;

			args.push_back("notarytool");
			args.push_back("submit");

			args.push_back(p_path);

			if (p_preset->get_or_env("notarization/apple_id_name", ENV_MAC_NOTARIZATION_APPLE_ID) == "" && p_preset->get_or_env("notarization/api_uuid", ENV_MAC_NOTARIZATION_UUID) == "") {
				add_message(EXPORT_MESSAGE_ERROR, TTR("Notarization"), TTR("Neither Apple ID name nor App Store Connect issuer ID name not specified."));
				return Error::FAILED;
			}
			if (p_preset->get_or_env("notarization/apple_id_name", ENV_MAC_NOTARIZATION_APPLE_ID) != "" && p_preset->get_or_env("notarization/api_uuid", ENV_MAC_NOTARIZATION_UUID) != "") {
				add_message(EXPORT_MESSAGE_ERROR, TTR("Notarization"), TTR("Both Apple ID name and App Store Connect issuer ID name are specified, only one should be set at the same time."));
				return Error::FAILED;
			}

			if (p_preset->get_or_env("notarization/apple_id_name", ENV_MAC_NOTARIZATION_APPLE_ID) != "") {
				if (p_preset->get_or_env("notarization/apple_id_password", ENV_MAC_NOTARIZATION_APPLE_PASS) == "") {
					add_message(EXPORT_MESSAGE_ERROR, TTR("Notarization"), TTR("Apple ID password not specified."));
					return Error::FAILED;
				}
				args.push_back("--apple-id");
				args.push_back(p_preset->get_or_env("notarization/apple_id_name", ENV_MAC_NOTARIZATION_APPLE_ID));

				args.push_back("--password");
				args.push_back(p_preset->get_or_env("notarization/apple_id_password", ENV_MAC_NOTARIZATION_APPLE_PASS));
			} else {
				if (p_preset->get_or_env("notarization/api_key_id", ENV_MAC_NOTARIZATION_KEY_ID) == "") {
					add_message(EXPORT_MESSAGE_ERROR, TTR("Notarization"), TTR("App Store Connect API key ID not specified."));
					return Error::FAILED;
				}
				args.push_back("--issuer");
				args.push_back(p_preset->get_or_env("notarization/api_uuid", ENV_MAC_NOTARIZATION_UUID));

				if (!p_preset->get_or_env("notarization/api_key", ENV_MAC_NOTARIZATION_KEY).operator String().is_empty()) {
					args.push_back("--key");
					args.push_back(p_preset->get_or_env("notarization/api_key", ENV_MAC_NOTARIZATION_KEY));
				}

				args.push_back("--key-id");
				args.push_back(p_preset->get_or_env("notarization/api_key_id", ENV_MAC_NOTARIZATION_KEY_ID));
			}

			args.push_back("--no-progress");

			if (p_preset->get("codesign/apple_team_id")) {
				args.push_back("--team-id");
				args.push_back(p_preset->get("codesign/apple_team_id"));
			}

			String str;
			int exitcode = 0;
			Error err = OS::get_singleton()->execute("xcrun", args, &str, &exitcode, true);
			if (err != OK) {
				add_message(EXPORT_MESSAGE_WARNING, TTR("Notarization"), TTR("Could not start xcrun executable."));
				return err;
			}

			int rq_offset = str.find("id:");
			if (exitcode != 0 || rq_offset == -1) {
				print_line("notarytool (" + p_path + "):\n" + str);
				add_message(EXPORT_MESSAGE_WARNING, TTR("Notarization"), TTR("Notarization failed, see editor log for details."));
				return Error::FAILED;
			} else {
				print_verbose("notarytool (" + p_path + "):\n" + str);
				int next_nl = str.find("\n", rq_offset);
				String request_uuid = (next_nl == -1) ? str.substr(rq_offset + 4, -1) : str.substr(rq_offset + 4, next_nl - rq_offset - 4);
				add_message(EXPORT_MESSAGE_INFO, TTR("Notarization"), vformat(TTR("Notarization request UUID: \"%s\""), request_uuid));
				add_message(EXPORT_MESSAGE_INFO, TTR("Notarization"), TTR("The notarization process generally takes less than an hour."));
				add_message(EXPORT_MESSAGE_INFO, TTR("Notarization"), "\t" + TTR("You can check progress manually by opening a Terminal and running the following command:"));
				add_message(EXPORT_MESSAGE_INFO, TTR("Notarization"), "\t\t\"xcrun notarytool log <request uuid> --issuer <api uuid> --key-id <api key id> --key <api key path>\" or");
				add_message(EXPORT_MESSAGE_INFO, TTR("Notarization"), "\t\t\"xcrun notarytool log <request uuid> --apple-id <your email> --password <app-specific pwd>>\"");
				add_message(EXPORT_MESSAGE_INFO, TTR("Notarization"), "\t" + TTR("Run the following command to staple the notarization ticket to the exported application (optional):"));
				add_message(EXPORT_MESSAGE_INFO, TTR("Notarization"), "\t\t\"xcrun stapler staple <app path>\"");
			}
		} break;
#endif
		default: {
		};
	}
	return OK;
}

void EditorExportPlatformMacOS::_code_sign(const Ref<EditorExportPreset> &p_preset, const String &p_path, const String &p_ent_path, bool p_warn, bool p_set_id) {
	int codesign_tool = p_preset->get("codesign/codesign");
	switch (codesign_tool) {
		case 1: { // built-in ad-hoc
			print_verbose("using built-in codesign...");
#ifdef MODULE_REGEX_ENABLED
			String error_msg;
			Error err = CodeSign::codesign(false, true, p_path, p_ent_path, error_msg);
			if (err != OK) {
				add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), vformat(TTR("Built-in CodeSign failed with error \"%s\"."), error_msg));
				return;
			}
#else
			add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("Built-in CodeSign require regex module."));
#endif
		} break;
		case 2: { // "rcodesign"
			print_verbose("using rcodesign codesign...");

			String rcodesign = EDITOR_GET("export/macos/rcodesign").operator String();
			if (rcodesign.is_empty()) {
				add_message(EXPORT_MESSAGE_ERROR, TTR("Code Signing"), TTR("Xrcodesign path is not set. Configure rcodesign path in the Editor Settings (Export > macOS > rcodesign)."));
				return;
			}

			List<String> args;
			args.push_back("sign");

			if (!p_ent_path.is_empty()) {
				args.push_back("--entitlements-xml-path");
				args.push_back(p_ent_path);
			}

			String certificate_file = p_preset->get_or_env("codesign/certificate_file", ENV_MAC_CODESIGN_CERT_FILE);
			String certificate_pass = p_preset->get_or_env("codesign/certificate_password", ENV_MAC_CODESIGN_CERT_PASS);
			if (!certificate_file.is_empty() && !certificate_pass.is_empty()) {
				args.push_back("--p12-file");
				args.push_back(certificate_file);
				args.push_back("--p12-password");
				args.push_back(certificate_pass);
			}
			args.push_back("--code-signature-flags");
			args.push_back("runtime");

			if (p_set_id) {
				String app_id = p_preset->get("application/bundle_identifier");
				args.push_back("--binary-identifier");
				args.push_back(app_id);
			}

			args.push_back("-v"); /* provide some more feedback */

			args.push_back(p_path);

			String str;
			int exitcode = 0;

			Error err = OS::get_singleton()->execute(rcodesign, args, &str, &exitcode, true);
			if (err != OK) {
				add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("Could not start rcodesign executable."));
				return;
			}

			if (exitcode != 0) {
				print_line("rcodesign (" + p_path + "):\n" + str);
				add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("Code signing failed, see editor log for details."));
				return;
			} else {
				print_verbose("rcodesign (" + p_path + "):\n" + str);
			}
		} break;
#ifdef MACOS_ENABLED
		case 3: { // "codesign"
			print_verbose("using xcode codesign...");

			if (!FileAccess::exists("/usr/bin/codesign") && !FileAccess::exists("/bin/codesign")) {
				add_message(EXPORT_MESSAGE_ERROR, TTR("Code Signing"), TTR("Xcode command line tools are not installed."));
				return;
			}

			bool ad_hoc = (p_preset->get("codesign/identity") == "" || p_preset->get("codesign/identity") == "-");

			List<String> args;
			if (!ad_hoc) {
				args.push_back("--timestamp");
				args.push_back("--options");
				args.push_back("runtime");
			}

			if (!p_ent_path.is_empty()) {
				args.push_back("--entitlements");
				args.push_back(p_ent_path);
			}

			PackedStringArray user_args = p_preset->get("codesign/custom_options");
			for (int i = 0; i < user_args.size(); i++) {
				String user_arg = user_args[i].strip_edges();
				if (!user_arg.is_empty()) {
					args.push_back(user_arg);
				}
			}

			args.push_back("-s");
			if (ad_hoc) {
				args.push_back("-");
			} else {
				args.push_back(p_preset->get("codesign/identity"));
			}

			if (p_set_id) {
				String app_id = p_preset->get("application/bundle_identifier");
				args.push_back("-i");
				args.push_back(app_id);
			}

			args.push_back("-v"); /* provide some more feedback */
			args.push_back("-f");

			args.push_back(p_path);

			String str;
			int exitcode = 0;

			Error err = OS::get_singleton()->execute("codesign", args, &str, &exitcode, true);
			if (err != OK) {
				add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("Could not start codesign executable, make sure Xcode command line tools are installed."));
				return;
			}

			if (exitcode != 0) {
				print_line("codesign (" + p_path + "):\n" + str);
				add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("Code signing failed, see editor log for details."));
				return;
			} else {
				print_verbose("codesign (" + p_path + "):\n" + str);
			}
		} break;
#endif
		default: {
		};
	}
}

void EditorExportPlatformMacOS::_code_sign_directory(const Ref<EditorExportPreset> &p_preset, const String &p_path,
		const String &p_ent_path, const String &p_helper_ent_path, bool p_should_error_on_non_code) {
	static Vector<String> extensions_to_sign;

	bool sandbox = p_preset->get("codesign/entitlements/app_sandbox/enabled");
	if (extensions_to_sign.is_empty()) {
		extensions_to_sign.push_back("dylib");
		extensions_to_sign.push_back("framework");
		extensions_to_sign.push_back("");
	}

	Error dir_access_error;
	Ref<DirAccess> dir_access{ DirAccess::open(p_path, &dir_access_error) };

	if (dir_access_error != OK) {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), vformat(TTR("Cannot sign directory %s."), p_path));
		return;
	}

	dir_access->list_dir_begin();
	String current_file{ dir_access->get_next() };
	while (!current_file.is_empty()) {
		String current_file_path{ p_path.path_join(current_file) };

		if (current_file == ".." || current_file == ".") {
			current_file = dir_access->get_next();
			continue;
		}

		if (extensions_to_sign.has(current_file.get_extension())) {
			String ent_path;
			bool set_bundle_id = false;
			if (sandbox && FileAccess::exists(current_file_path)) {
				int ftype = MachO::get_filetype(current_file_path);
				if (ftype == 2 || ftype == 5) {
					ent_path = p_helper_ent_path;
					set_bundle_id = true;
				}
			}
			_code_sign(p_preset, current_file_path, ent_path, false, set_bundle_id);
			if (is_executable(current_file_path)) {
				// chmod with 0755 if the file is executable.
				FileAccess::set_unix_permissions(current_file_path, 0755);
			}
		} else if (dir_access->current_is_dir()) {
			_code_sign_directory(p_preset, current_file_path, p_ent_path, p_helper_ent_path, p_should_error_on_non_code);
		} else if (p_should_error_on_non_code) {
			add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), vformat(TTR("Cannot sign file %s."), current_file));
		}

		current_file = dir_access->get_next();
	}
}

Error EditorExportPlatformMacOS::_copy_and_sign_files(Ref<DirAccess> &dir_access, const String &p_src_path,
		const String &p_in_app_path, bool p_sign_enabled,
		const Ref<EditorExportPreset> &p_preset, const String &p_ent_path,
		const String &p_helper_ent_path,
		bool p_should_error_on_non_code_sign, bool p_sandbox) {
	static Vector<String> extensions_to_sign;

	if (extensions_to_sign.is_empty()) {
		extensions_to_sign.push_back("dylib");
		extensions_to_sign.push_back("framework");
		extensions_to_sign.push_back("");
	}

	Error err{ OK };
	if (dir_access->dir_exists(p_src_path)) {
#ifndef UNIX_ENABLED
		add_message(EXPORT_MESSAGE_INFO, TTR("Export"), vformat(TTR("Relative symlinks are not supported, exported \"%s\" might be broken!"), p_src_path.get_file()));
#endif
		print_verbose("export framework: " + p_src_path + " -> " + p_in_app_path);

		bool plist_misssing = false;
		Ref<PList> plist;
		plist.instantiate();
		plist->load_file(p_src_path.path_join("Resources").path_join("Info.plist"));

		Ref<PListNode> root_node = plist->get_root();
		if (root_node.is_null()) {
			plist_misssing = true;
		} else {
			Dictionary root = root_node->get_value();
			if (!root.has("CFBundleExecutable") || !root.has("CFBundleIdentifier") || !root.has("CFBundlePackageType") || !root.has("CFBundleInfoDictionaryVersion") || !root.has("CFBundleName") || !root.has("CFBundleSupportedPlatforms")) {
				plist_misssing = true;
			}
		}

		err = dir_access->make_dir_recursive(p_in_app_path);
		if (err == OK) {
			err = dir_access->copy_dir(p_src_path, p_in_app_path, -1, true);
		}
		if (err == OK && plist_misssing) {
			add_message(EXPORT_MESSAGE_WARNING, TTR("Export"), vformat(TTR("\"%s\": Info.plist missing or invalid, new Info.plist generated."), p_src_path.get_file()));
			// Generate Info.plist
			String lib_name = p_src_path.get_basename().get_file();
			String lib_id = p_preset->get("application/bundle_identifier");
			String lib_clean_name = lib_name;
			for (int i = 0; i < lib_clean_name.length(); i++) {
				if (!is_ascii_alphanumeric_char(lib_clean_name[i]) && lib_clean_name[i] != '.' && lib_clean_name[i] != '-') {
					lib_clean_name[i] = '-';
				}
			}

			String info_plist_format = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
									   "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
									   "<plist version=\"1.0\">\n"
									   "  <dict>\n"
									   "    <key>CFBundleExecutable</key>\n"
									   "    <string>$name</string>\n"
									   "    <key>CFBundleIdentifier</key>\n"
									   "    <string>$id.framework.$cl_name</string>\n"
									   "    <key>CFBundleInfoDictionaryVersion</key>\n"
									   "    <string>6.0</string>\n"
									   "    <key>CFBundleName</key>\n"
									   "    <string>$name</string>\n"
									   "    <key>CFBundlePackageType</key>\n"
									   "    <string>FMWK</string>\n"
									   "    <key>CFBundleShortVersionString</key>\n"
									   "    <string>1.0.0</string>\n"
									   "    <key>CFBundleSupportedPlatforms</key>\n"
									   "    <array>\n"
									   "      <string>MacOSX</string>\n"
									   "    </array>\n"
									   "    <key>CFBundleVersion</key>\n"
									   "    <string>1.0.0</string>\n"
									   "    <key>LSMinimumSystemVersion</key>\n"
									   "    <string>10.12</string>\n"
									   "  </dict>\n"
									   "</plist>";

			String info_plist = info_plist_format.replace("$id", lib_id).replace("$name", lib_name).replace("$cl_name", lib_clean_name);

			err = dir_access->make_dir_recursive(p_in_app_path.path_join("Resources"));
			Ref<FileAccess> f = FileAccess::open(p_in_app_path.path_join("Resources").path_join("Info.plist"), FileAccess::WRITE);
			if (f.is_valid()) {
				f->store_string(info_plist);
			}
		}
	} else {
		print_verbose("export dylib: " + p_src_path + " -> " + p_in_app_path);
		err = dir_access->copy(p_src_path, p_in_app_path);
	}
	if (err == OK && p_sign_enabled) {
		if (dir_access->dir_exists(p_src_path) && p_src_path.get_extension().is_empty()) {
			// If it is a directory, find and sign all dynamic libraries.
			_code_sign_directory(p_preset, p_in_app_path, p_ent_path, p_helper_ent_path, p_should_error_on_non_code_sign);
		} else {
			if (extensions_to_sign.has(p_in_app_path.get_extension())) {
				String ent_path;
				bool set_bundle_id = false;
				if (p_sandbox && FileAccess::exists(p_in_app_path)) {
					int ftype = MachO::get_filetype(p_in_app_path);
					if (ftype == 2 || ftype == 5) {
						ent_path = p_helper_ent_path;
						set_bundle_id = true;
					}
				}
				_code_sign(p_preset, p_in_app_path, ent_path, false, set_bundle_id);
			}
			if (dir_access->file_exists(p_in_app_path) && is_executable(p_in_app_path)) {
				// chmod with 0755 if the file is executable.
				FileAccess::set_unix_permissions(p_in_app_path, 0755);
			}
		}
	}
	return err;
}

Error EditorExportPlatformMacOS::_export_macos_plugins_for(Ref<EditorExportPlugin> p_editor_export_plugin,
		const String &p_app_path_name, Ref<DirAccess> &dir_access,
		bool p_sign_enabled, const Ref<EditorExportPreset> &p_preset,
		const String &p_ent_path, const String &p_helper_ent_path, bool p_sandbox) {
	Error error{ OK };
	const Vector<String> &macos_plugins{ p_editor_export_plugin->get_macos_plugin_files() };
	for (int i = 0; i < macos_plugins.size(); ++i) {
		String src_path{ ProjectSettings::get_singleton()->globalize_path(macos_plugins[i]) };
		String path_in_app{ p_app_path_name + "/Contents/PlugIns/" + src_path.get_file() };
		error = _copy_and_sign_files(dir_access, src_path, path_in_app, p_sign_enabled, p_preset, p_ent_path, p_helper_ent_path, false, p_sandbox);
		if (error != OK) {
			break;
		}
	}
	return error;
}

Error EditorExportPlatformMacOS::_create_pkg(const Ref<EditorExportPreset> &p_preset, const String &p_pkg_path, const String &p_app_path_name) {
	List<String> args;

	if (FileAccess::exists(p_pkg_path)) {
		OS::get_singleton()->move_to_trash(p_pkg_path);
	}

	args.push_back("productbuild");
	args.push_back("--component");
	args.push_back(p_app_path_name);
	args.push_back("/Applications");
	String ident = p_preset->get("codesign/installer_identity");
	if (!ident.is_empty()) {
		args.push_back("--timestamp");
		args.push_back("--sign");
		args.push_back(ident);
	}
	args.push_back("--quiet");
	args.push_back(p_pkg_path);

	String str;
	Error err = OS::get_singleton()->execute("xcrun", args, &str, nullptr, true);
	if (err != OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("PKG Creation"), TTR("Could not start productbuild executable."));
		return err;
	}

	print_verbose("productbuild returned: " + str);
	if (str.contains("productbuild: error:")) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("PKG Creation"), TTR("`productbuild` failed."));
		return FAILED;
	}

	return OK;
}

Error EditorExportPlatformMacOS::_create_dmg(const String &p_dmg_path, const String &p_pkg_name, const String &p_app_path_name) {
	List<String> args;

	if (FileAccess::exists(p_dmg_path)) {
		OS::get_singleton()->move_to_trash(p_dmg_path);
	}

	args.push_back("create");
	args.push_back(p_dmg_path);
	args.push_back("-volname");
	args.push_back(p_pkg_name);
	args.push_back("-fs");
	args.push_back("HFS+");
	args.push_back("-srcfolder");
	args.push_back(p_app_path_name);

	String str;
	Error err = OS::get_singleton()->execute("hdiutil", args, &str, nullptr, true);
	if (err != OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("DMG Creation"), TTR("Could not start hdiutil executable."));
		return err;
	}

	print_verbose("hdiutil returned: " + str);
	if (str.contains("create failed")) {
		if (str.contains("File exists")) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("DMG Creation"), TTR("`hdiutil create` failed - file exists."));
		} else {
			add_message(EXPORT_MESSAGE_ERROR, TTR("DMG Creation"), TTR("`hdiutil create` failed."));
		}
		return FAILED;
	}

	return OK;
}

bool EditorExportPlatformMacOS::is_shebang(const String &p_path) const {
	Ref<FileAccess> fb = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(fb.is_null(), false, vformat("Can't open file: \"%s\".", p_path));
	uint16_t magic = fb->get_16();
	return (magic == 0x2123);
}

bool EditorExportPlatformMacOS::is_executable(const String &p_path) const {
	return MachO::is_macho(p_path) || LipO::is_lipo(p_path) || is_shebang(p_path);
}

Error EditorExportPlatformMacOS::_export_debug_script(const Ref<EditorExportPreset> &p_preset, const String &p_app_name, const String &p_pkg_name, const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Debug Script Export"), vformat(TTR("Could not open file \"%s\"."), p_path));
		return ERR_CANT_CREATE;
	}

	f->store_line("#!/bin/sh");
	f->store_line("echo -ne '\\033c\\033]0;" + p_app_name + "\\a'");
	f->store_line("");
	f->store_line("function app_realpath() {");
	f->store_line("    SOURCE=$1");
	f->store_line("    while [ -h \"$SOURCE\" ]; do");
	f->store_line("        DIR=$(dirname \"$SOURCE\")");
	f->store_line("        SOURCE=$(readlink \"$SOURCE\")");
	f->store_line("        [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE");
	f->store_line("    done");
	f->store_line("    echo \"$( cd -P \"$( dirname \"$SOURCE\" )\" >/dev/null 2>&1 && pwd )\"");
	f->store_line("}");
	f->store_line("");
	f->store_line("BASE_PATH=\"$(app_realpath \"${BASH_SOURCE[0]}\")\"");
	f->store_line("\"$BASE_PATH/" + p_pkg_name + "\" \"$@\"");
	f->store_line("");

	return OK;
}

Error EditorExportPlatformMacOS::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);

	const String base_dir = p_path.get_base_dir();

	if (!DirAccess::exists(base_dir)) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Target folder does not exist or is inaccessible: \"%s\""), base_dir));
		return ERR_FILE_BAD_PATH;
	}

	EditorProgress ep("export", TTR("Exporting for macOS"), 3, true);

	String src_pkg_name;
	if (p_debug) {
		src_pkg_name = p_preset->get("custom_template/debug");
	} else {
		src_pkg_name = p_preset->get("custom_template/release");
	}

	if (src_pkg_name.is_empty()) {
		String err;
		src_pkg_name = find_export_template("macos.zip", &err);
		if (src_pkg_name.is_empty()) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Templates"), TTR("Export template not found.") + "\n" + err);
			return ERR_FILE_NOT_FOUND;
		}
	}

	Ref<FileAccess> io_fa;
	zlib_filefunc_def io = zipio_create_io(&io_fa);

	if (ep.step(TTR("Creating app bundle"), 0)) {
		return ERR_SKIP;
	}

	unzFile src_pkg_zip = unzOpen2(src_pkg_name.utf8().get_data(), &io);
	if (!src_pkg_zip) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Templates"), vformat(TTR("Could not find template app to export: \"%s\"."), src_pkg_name));
		return ERR_FILE_NOT_FOUND;
	}

	int ret = unzGoToFirstFile(src_pkg_zip);

	String architecture = p_preset->get("binary_format/architecture");
	String binary_to_use = "godot_macos_" + String(p_debug ? "debug" : "release") + "." + architecture;

	String pkg_name;
	if (String(GLOBAL_GET("application/config/name")) != "") {
		pkg_name = String(GLOBAL_GET("application/config/name"));
	} else {
		pkg_name = "Unnamed";
	}
	pkg_name = OS::get_singleton()->get_safe_dir_name(pkg_name);

	String export_format;
	if (p_path.ends_with("zip")) {
		export_format = "zip";
	} else if (p_path.ends_with("app")) {
		export_format = "app";
#ifdef MACOS_ENABLED
	} else if (p_path.ends_with("dmg")) {
		export_format = "dmg";
	} else if (p_path.ends_with("pkg")) {
		export_format = "pkg";
#endif
	} else {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Invalid export format."));
		return ERR_CANT_CREATE;
	}

	// Create our application bundle.
	String tmp_app_dir_name = pkg_name + ".app";
	String tmp_base_path_name;
	String tmp_app_path_name;
	String scr_path;
	if (export_format == "app") {
		tmp_base_path_name = p_path.get_base_dir();
		tmp_app_path_name = p_path;
		scr_path = p_path.get_basename() + ".command";
	} else {
		tmp_base_path_name = EditorPaths::get_singleton()->get_cache_dir().path_join(pkg_name);
		tmp_app_path_name = tmp_base_path_name.path_join(tmp_app_dir_name);
		scr_path = tmp_base_path_name.path_join(pkg_name + ".command");
	}

	print_verbose("Exporting to " + tmp_app_path_name);

	Error err = OK;

	Ref<DirAccess> tmp_app_dir = DirAccess::create_for_path(tmp_base_path_name);
	if (tmp_app_dir.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not create directory: \"%s\"."), tmp_base_path_name));
		err = ERR_CANT_CREATE;
	}

	if (FileAccess::exists(scr_path)) {
		DirAccess::remove_file_or_error(scr_path);
	}
	if (DirAccess::exists(tmp_app_path_name)) {
		String old_dir = tmp_app_dir->get_current_dir();
		if (tmp_app_dir->change_dir(tmp_app_path_name) == OK) {
			tmp_app_dir->erase_contents_recursive();
			tmp_app_dir->change_dir(old_dir);
		}
	}

	Array helpers = p_preset->get("codesign/entitlements/app_sandbox/helper_executables");

	// Create our folder structure.
	if (err == OK) {
		print_verbose("Creating " + tmp_app_path_name + "/Contents/MacOS");
		err = tmp_app_dir->make_dir_recursive(tmp_app_path_name + "/Contents/MacOS");
		if (err != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not create directory \"%s\"."), tmp_app_path_name + "/Contents/MacOS"));
		}
	}

	if (err == OK) {
		print_verbose("Creating " + tmp_app_path_name + "/Contents/Frameworks");
		err = tmp_app_dir->make_dir_recursive(tmp_app_path_name + "/Contents/Frameworks");
		if (err != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not create directory \"%s\"."), tmp_app_path_name + "/Contents/Frameworks"));
		}
	}

	if ((err == OK) && helpers.size() > 0) {
		print_line("Creating " + tmp_app_path_name + "/Contents/Helpers");
		err = tmp_app_dir->make_dir_recursive(tmp_app_path_name + "/Contents/Helpers");
		if (err != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not create directory \"%s\"."), tmp_app_path_name + "/Contents/Helpers"));
		}
	}

	if (err == OK) {
		print_verbose("Creating " + tmp_app_path_name + "/Contents/Resources");
		err = tmp_app_dir->make_dir_recursive(tmp_app_path_name + "/Contents/Resources");
		if (err != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not create directory \"%s\"."), tmp_app_path_name + "/Contents/Resources"));
		}
	}

	Dictionary appnames = GLOBAL_GET("application/config/name_localized");
	Dictionary microphone_usage_descriptions = p_preset->get("privacy/microphone_usage_description_localized");
	Dictionary camera_usage_descriptions = p_preset->get("privacy/camera_usage_description_localized");
	Dictionary location_usage_descriptions = p_preset->get("privacy/location_usage_description_localized");
	Dictionary address_book_usage_descriptions = p_preset->get("privacy/address_book_usage_description_localized");
	Dictionary calendar_usage_descriptions = p_preset->get("privacy/calendar_usage_description_localized");
	Dictionary photos_library_usage_descriptions = p_preset->get("privacy/photos_library_usage_description_localized");
	Dictionary desktop_folder_usage_descriptions = p_preset->get("privacy/desktop_folder_usage_description_localized");
	Dictionary documents_folder_usage_descriptions = p_preset->get("privacy/documents_folder_usage_description_localized");
	Dictionary downloads_folder_usage_descriptions = p_preset->get("privacy/downloads_folder_usage_description_localized");
	Dictionary network_volumes_usage_descriptions = p_preset->get("privacy/network_volumes_usage_description_localized");
	Dictionary removable_volumes_usage_descriptions = p_preset->get("privacy/removable_volumes_usage_description_localized");
	Dictionary copyrights = p_preset->get("application/copyright_localized");

	Vector<String> translations = GLOBAL_GET("internationalization/locale/translations");
	if (translations.size() > 0) {
		{
			String fname = tmp_app_path_name + "/Contents/Resources/en.lproj";
			tmp_app_dir->make_dir_recursive(fname);
			Ref<FileAccess> f = FileAccess::open(fname + "/InfoPlist.strings", FileAccess::WRITE);
			f->store_line("/* Localized versions of Info.plist keys */");
			f->store_line("");
			f->store_line("CFBundleDisplayName = \"" + GLOBAL_GET("application/config/name").operator String() + "\";");
			if (!((String)p_preset->get("privacy/microphone_usage_description")).is_empty()) {
				f->store_line("NSMicrophoneUsageDescription = \"" + p_preset->get("privacy/microphone_usage_description").operator String() + "\";");
			}
			if (!((String)p_preset->get("privacy/camera_usage_description")).is_empty()) {
				f->store_line("NSCameraUsageDescription = \"" + p_preset->get("privacy/camera_usage_description").operator String() + "\";");
			}
			if (!((String)p_preset->get("privacy/location_usage_description")).is_empty()) {
				f->store_line("NSLocationUsageDescription = \"" + p_preset->get("privacy/location_usage_description").operator String() + "\";");
			}
			if (!((String)p_preset->get("privacy/address_book_usage_description")).is_empty()) {
				f->store_line("NSContactsUsageDescription = \"" + p_preset->get("privacy/address_book_usage_description").operator String() + "\";");
			}
			if (!((String)p_preset->get("privacy/calendar_usage_description")).is_empty()) {
				f->store_line("NSCalendarsUsageDescription = \"" + p_preset->get("privacy/calendar_usage_description").operator String() + "\";");
			}
			if (!((String)p_preset->get("privacy/photos_library_usage_description")).is_empty()) {
				f->store_line("NSPhotoLibraryUsageDescription = \"" + p_preset->get("privacy/photos_library_usage_description").operator String() + "\";");
			}
			if (!((String)p_preset->get("privacy/desktop_folder_usage_description")).is_empty()) {
				f->store_line("NSDesktopFolderUsageDescription = \"" + p_preset->get("privacy/desktop_folder_usage_description").operator String() + "\";");
			}
			if (!((String)p_preset->get("privacy/documents_folder_usage_description")).is_empty()) {
				f->store_line("NSDocumentsFolderUsageDescription = \"" + p_preset->get("privacy/documents_folder_usage_description").operator String() + "\";");
			}
			if (!((String)p_preset->get("privacy/downloads_folder_usage_description")).is_empty()) {
				f->store_line("NSDownloadsFolderUsageDescription = \"" + p_preset->get("privacy/downloads_folder_usage_description").operator String() + "\";");
			}
			if (!((String)p_preset->get("privacy/network_volumes_usage_description")).is_empty()) {
				f->store_line("NSNetworkVolumesUsageDescription = \"" + p_preset->get("privacy/network_volumes_usage_description").operator String() + "\";");
			}
			if (!((String)p_preset->get("privacy/removable_volumes_usage_description")).is_empty()) {
				f->store_line("NSRemovableVolumesUsageDescription = \"" + p_preset->get("privacy/removable_volumes_usage_description").operator String() + "\";");
			}
			f->store_line("NSHumanReadableCopyright = \"" + p_preset->get("application/copyright").operator String() + "\";");
		}

		HashSet<String> languages;
		for (const String &E : translations) {
			Ref<Translation> tr = ResourceLoader::load(E);
			if (tr.is_valid() && tr->get_locale() != "en") {
				languages.insert(tr->get_locale());
			}
		}

		for (const String &lang : languages) {
			String fname = tmp_app_path_name + "/Contents/Resources/" + lang + ".lproj";
			tmp_app_dir->make_dir_recursive(fname);
			Ref<FileAccess> f = FileAccess::open(fname + "/InfoPlist.strings", FileAccess::WRITE);
			f->store_line("/* Localized versions of Info.plist keys */");
			f->store_line("");
			if (appnames.has(lang)) {
				f->store_line("CFBundleDisplayName = \"" + appnames[lang].operator String() + "\";");
			}
			if (microphone_usage_descriptions.has(lang)) {
				f->store_line("NSMicrophoneUsageDescription = \"" + microphone_usage_descriptions[lang].operator String() + "\";");
			}
			if (camera_usage_descriptions.has(lang)) {
				f->store_line("NSCameraUsageDescription = \"" + camera_usage_descriptions[lang].operator String() + "\";");
			}
			if (location_usage_descriptions.has(lang)) {
				f->store_line("NSLocationUsageDescription = \"" + location_usage_descriptions[lang].operator String() + "\";");
			}
			if (address_book_usage_descriptions.has(lang)) {
				f->store_line("NSContactsUsageDescription = \"" + address_book_usage_descriptions[lang].operator String() + "\";");
			}
			if (calendar_usage_descriptions.has(lang)) {
				f->store_line("NSCalendarsUsageDescription = \"" + calendar_usage_descriptions[lang].operator String() + "\";");
			}
			if (photos_library_usage_descriptions.has(lang)) {
				f->store_line("NSPhotoLibraryUsageDescription = \"" + photos_library_usage_descriptions[lang].operator String() + "\";");
			}
			if (desktop_folder_usage_descriptions.has(lang)) {
				f->store_line("NSDesktopFolderUsageDescription = \"" + desktop_folder_usage_descriptions[lang].operator String() + "\";");
			}
			if (documents_folder_usage_descriptions.has(lang)) {
				f->store_line("NSDocumentsFolderUsageDescription = \"" + documents_folder_usage_descriptions[lang].operator String() + "\";");
			}
			if (downloads_folder_usage_descriptions.has(lang)) {
				f->store_line("NSDownloadsFolderUsageDescription = \"" + downloads_folder_usage_descriptions[lang].operator String() + "\";");
			}
			if (network_volumes_usage_descriptions.has(lang)) {
				f->store_line("NSNetworkVolumesUsageDescription = \"" + network_volumes_usage_descriptions[lang].operator String() + "\";");
			}
			if (removable_volumes_usage_descriptions.has(lang)) {
				f->store_line("NSRemovableVolumesUsageDescription = \"" + removable_volumes_usage_descriptions[lang].operator String() + "\";");
			}
			if (copyrights.has(lang)) {
				f->store_line("NSHumanReadableCopyright = \"" + copyrights[lang].operator String() + "\";");
			}
		}
	}

	// Now process our template.
	bool found_binary = false;

	int export_angle = p_preset->get("application/export_angle");
	bool include_angle_libs = false;
	if (export_angle == 0) {
		include_angle_libs = String(GLOBAL_GET("rendering/gl_compatibility/driver.macos")) == "opengl3_angle";
	} else if (export_angle == 1) {
		include_angle_libs = true;
	}

	while (ret == UNZ_OK && err == OK) {
		// Get filename.
		unz_file_info info;
		char fname[16384];
		ret = unzGetCurrentFileInfo(src_pkg_zip, &info, fname, 16384, nullptr, 0, nullptr, 0);
		if (ret != UNZ_OK) {
			break;
		}

		String file = String::utf8(fname);

		Vector<uint8_t> data;
		data.resize(info.uncompressed_size);

		// Read.
		unzOpenCurrentFile(src_pkg_zip);
		unzReadCurrentFile(src_pkg_zip, data.ptrw(), data.size());
		unzCloseCurrentFile(src_pkg_zip);

		// Write.
		file = file.replace_first("macos_template.app/", "");

		if (((info.external_fa >> 16L) & 0120000) == 0120000) {
#ifndef UNIX_ENABLED
			add_message(EXPORT_MESSAGE_INFO, TTR("Export"), TTR("Relative symlinks are not supported on this OS, the exported project might be broken!"));
#endif
			// Handle symlinks in the archive.
			file = tmp_app_path_name.path_join(file);
			if (err == OK) {
				err = tmp_app_dir->make_dir_recursive(file.get_base_dir());
				if (err != OK) {
					add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not create directory \"%s\"."), file.get_base_dir()));
				}
			}
			if (err == OK) {
				String lnk_data = String::utf8((const char *)data.ptr(), data.size());
				err = tmp_app_dir->create_link(lnk_data, file);
				if (err != OK) {
					add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not created symlink \"%s\" -> \"%s\"."), lnk_data, file));
				}
				print_verbose(vformat("ADDING SYMLINK %s => %s\n", file, lnk_data));
			}

			ret = unzGoToNextFile(src_pkg_zip);
			continue; // next
		}

		if (file == "Contents/Frameworks/libEGL.dylib") {
			if (!include_angle_libs) {
				ret = unzGoToNextFile(src_pkg_zip);
				continue; // skip
			}
		}

		if (file == "Contents/Frameworks/libGLESv2.dylib") {
			if (!include_angle_libs) {
				ret = unzGoToNextFile(src_pkg_zip);
				continue; // skip
			}
		}

		if (file == "Contents/Info.plist") {
			_fix_plist(p_preset, data, pkg_name);
		}

		if (file == "Contents/Resources/PrivacyInfo.xcprivacy") {
			_fix_privacy_manifest(p_preset, data);
		}

		if (file.begins_with("Contents/MacOS/godot_")) {
			if (file != "Contents/MacOS/" + binary_to_use) {
				ret = unzGoToNextFile(src_pkg_zip);
				continue; // skip
			}
			found_binary = true;
			file = "Contents/MacOS/" + pkg_name;
		}

		if (file == "Contents/Resources/icon.icns") {
			// See if there is an icon.
			String icon_path;
			if (p_preset->get("application/icon") != "") {
				icon_path = p_preset->get("application/icon");
			} else if (GLOBAL_GET("application/config/macos_native_icon") != "") {
				icon_path = GLOBAL_GET("application/config/macos_native_icon");
			} else {
				icon_path = GLOBAL_GET("application/config/icon");
			}

			if (!icon_path.is_empty()) {
				if (icon_path.get_extension() == "icns") {
					Ref<FileAccess> icon = FileAccess::open(icon_path, FileAccess::READ);
					if (icon.is_valid()) {
						data.resize(icon->get_length());
						icon->get_buffer(&data.write[0], icon->get_length());
					}
				} else {
					Ref<Image> icon;
					icon.instantiate();
					err = ImageLoader::load_image(icon_path, icon);
					if (err == OK && !icon->is_empty()) {
						_make_icon(p_preset, icon, data);
					}
				}
			}
		}

		if (data.size() > 0) {
			print_verbose("ADDING: " + file + " size: " + itos(data.size()));

			// Write it into our application bundle.
			file = tmp_app_path_name.path_join(file);
			if (err == OK) {
				err = tmp_app_dir->make_dir_recursive(file.get_base_dir());
				if (err != OK) {
					add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not create directory \"%s\"."), file.get_base_dir()));
				}
			}
			if (err == OK) {
				Ref<FileAccess> f = FileAccess::open(file, FileAccess::WRITE);
				if (f.is_valid()) {
					f->store_buffer(data.ptr(), data.size());
					f.unref();
					if (is_executable(file)) {
						// chmod with 0755 if the file is executable.
						FileAccess::set_unix_permissions(file, 0755);
#ifndef UNIX_ENABLED
						if (export_format == "app") {
							add_message(EXPORT_MESSAGE_INFO, TTR("Export"), vformat(TTR("Unable to set Unix permissions for executable \"%s\". Use \"chmod +x\" to set it after transferring the exported .app to macOS or Linux."), "Contents/MacOS/" + file.get_file()));
						}
#endif
					}
				} else {
					add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not open \"%s\"."), file));
					err = ERR_CANT_CREATE;
				}
			}
		}

		ret = unzGoToNextFile(src_pkg_zip);
	}

	// We're done with our source zip.
	unzClose(src_pkg_zip);

	if (!found_binary) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Requested template binary \"%s\" not found. It might be missing from your template archive."), binary_to_use));
		err = ERR_FILE_NOT_FOUND;
	}

	// Save console wrapper.
	if (err == OK) {
		int con_scr = p_preset->get("debug/export_console_wrapper");
		if ((con_scr == 1 && p_debug) || (con_scr == 2)) {
			err = _export_debug_script(p_preset, pkg_name, tmp_app_path_name.get_file() + "/Contents/MacOS/" + pkg_name, scr_path);
			FileAccess::set_unix_permissions(scr_path, 0755);
#ifndef UNIX_ENABLED
			if (export_format == "app") {
				add_message(EXPORT_MESSAGE_INFO, TTR("Export"), vformat(TTR("Unable to set Unix permissions for executable \"%s\". Use \"chmod +x\" to set it after transferring the exported .app to macOS or Linux."), scr_path.get_file()));
			}
#endif
			if (err != OK) {
				add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Could not create console wrapper."));
			}
		}
	}

	if (err == OK) {
		if (ep.step(TTR("Making PKG"), 1)) {
			return ERR_SKIP;
		}

		// See if we can code sign our new package.
		bool sign_enabled = (p_preset->get("codesign/codesign").operator int() > 0);
		bool ad_hoc = false;
		int codesign_tool = p_preset->get("codesign/codesign");
		switch (codesign_tool) {
			case 1: { // built-in ad-hoc
				ad_hoc = true;
			} break;
			case 2: { // "rcodesign"
				ad_hoc = p_preset->get_or_env("codesign/certificate_file", ENV_MAC_CODESIGN_CERT_FILE).operator String().is_empty() || p_preset->get_or_env("codesign/certificate_password", ENV_MAC_CODESIGN_CERT_PASS).operator String().is_empty();
			} break;
#ifdef MACOS_ENABLED
			case 3: { // "codesign"
				ad_hoc = (p_preset->get("codesign/identity") == "" || p_preset->get("codesign/identity") == "-");
			} break;
#endif
			default: {
			};
		}

		String pack_path = tmp_app_path_name + "/Contents/Resources/" + pkg_name + ".pck";
		Vector<SharedObject> shared_objects;
		err = save_pack(p_preset, p_debug, pack_path, &shared_objects);

		bool lib_validation = p_preset->get("codesign/entitlements/disable_library_validation");
		if (!shared_objects.is_empty() && sign_enabled && ad_hoc && !lib_validation) {
			add_message(EXPORT_MESSAGE_INFO, TTR("Entitlements Modified"), TTR("Ad-hoc signed applications require the 'Disable Library Validation' entitlement to load dynamic libraries."));
			lib_validation = true;
		}

		if (!shared_objects.is_empty() && sign_enabled && codesign_tool == 2) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Code Signing"), TTR("'rcodesign' doesn't support signing applications with embedded dynamic libraries."));
		}

		bool sandbox = p_preset->get("codesign/entitlements/app_sandbox/enabled");
		String ent_path = p_preset->get("codesign/entitlements/custom_file");
		String hlp_ent_path = sandbox ? EditorPaths::get_singleton()->get_cache_dir().path_join(pkg_name + "_helper.entitlements") : ent_path;
		if (sign_enabled && (ent_path.is_empty())) {
			ent_path = EditorPaths::get_singleton()->get_cache_dir().path_join(pkg_name + ".entitlements");

			Ref<FileAccess> ent_f = FileAccess::open(ent_path, FileAccess::WRITE);
			if (ent_f.is_valid()) {
				ent_f->store_line("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
				ent_f->store_line("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
				ent_f->store_line("<plist version=\"1.0\">");
				ent_f->store_line("<dict>");
				if (ClassDB::class_exists("CSharpScript")) {
					// These entitlements are required to run managed code, and are always enabled in Mono builds.
					ent_f->store_line("<key>com.apple.security.cs.allow-jit</key>");
					ent_f->store_line("<true/>");
					ent_f->store_line("<key>com.apple.security.cs.allow-unsigned-executable-memory</key>");
					ent_f->store_line("<true/>");
					ent_f->store_line("<key>com.apple.security.cs.allow-dyld-environment-variables</key>");
					ent_f->store_line("<true/>");
				} else {
					if ((bool)p_preset->get("codesign/entitlements/allow_jit_code_execution")) {
						ent_f->store_line("<key>com.apple.security.cs.allow-jit</key>");
						ent_f->store_line("<true/>");
					}
					if ((bool)p_preset->get("codesign/entitlements/allow_unsigned_executable_memory")) {
						ent_f->store_line("<key>com.apple.security.cs.allow-unsigned-executable-memory</key>");
						ent_f->store_line("<true/>");
					}
					if ((bool)p_preset->get("codesign/entitlements/allow_dyld_environment_variables")) {
						ent_f->store_line("<key>com.apple.security.cs.allow-dyld-environment-variables</key>");
						ent_f->store_line("<true/>");
					}
				}

				if (lib_validation) {
					ent_f->store_line("<key>com.apple.security.cs.disable-library-validation</key>");
					ent_f->store_line("<true/>");
				}
				if ((bool)p_preset->get("codesign/entitlements/audio_input")) {
					ent_f->store_line("<key>com.apple.security.device.audio-input</key>");
					ent_f->store_line("<true/>");
				}
				if ((bool)p_preset->get("codesign/entitlements/camera")) {
					ent_f->store_line("<key>com.apple.security.device.camera</key>");
					ent_f->store_line("<true/>");
				}
				if ((bool)p_preset->get("codesign/entitlements/location")) {
					ent_f->store_line("<key>com.apple.security.personal-information.location</key>");
					ent_f->store_line("<true/>");
				}
				if ((bool)p_preset->get("codesign/entitlements/address_book")) {
					ent_f->store_line("<key>com.apple.security.personal-information.addressbook</key>");
					ent_f->store_line("<true/>");
				}
				if ((bool)p_preset->get("codesign/entitlements/calendars")) {
					ent_f->store_line("<key>com.apple.security.personal-information.calendars</key>");
					ent_f->store_line("<true/>");
				}
				if ((bool)p_preset->get("codesign/entitlements/photos_library")) {
					ent_f->store_line("<key>com.apple.security.personal-information.photos-library</key>");
					ent_f->store_line("<true/>");
				}
				if ((bool)p_preset->get("codesign/entitlements/apple_events")) {
					ent_f->store_line("<key>com.apple.security.automation.apple-events</key>");
					ent_f->store_line("<true/>");
				}
				if ((bool)p_preset->get("codesign/entitlements/debugging")) {
					ent_f->store_line("<key>com.apple.security.get-task-allow</key>");
					ent_f->store_line("<true/>");
				}

				int dist_type = p_preset->get("export/distribution_type");
				if (dist_type == 2) {
					String pprof = p_preset->get_or_env("codesign/provisioning_profile", ENV_MAC_CODESIGN_PROFILE);
					String teamid = p_preset->get("codesign/apple_team_id");
					String bid = p_preset->get("application/bundle_identifier");
					if (!pprof.is_empty() && !teamid.is_empty()) {
						ent_f->store_line("<key>com.apple.developer.team-identifier</key>");
						ent_f->store_line("<string>" + teamid + "</string>");
						ent_f->store_line("<key>com.apple.application-identifier</key>");
						ent_f->store_line("<string>" + teamid + "." + bid + "</string>");
					}
				}

				if ((bool)p_preset->get("codesign/entitlements/app_sandbox/enabled")) {
					ent_f->store_line("<key>com.apple.security.app-sandbox</key>");
					ent_f->store_line("<true/>");

					if ((bool)p_preset->get("codesign/entitlements/app_sandbox/network_server")) {
						ent_f->store_line("<key>com.apple.security.network.server</key>");
						ent_f->store_line("<true/>");
					}
					if ((bool)p_preset->get("codesign/entitlements/app_sandbox/network_client")) {
						ent_f->store_line("<key>com.apple.security.network.client</key>");
						ent_f->store_line("<true/>");
					}
					if ((bool)p_preset->get("codesign/entitlements/app_sandbox/device_usb")) {
						ent_f->store_line("<key>com.apple.security.device.usb</key>");
						ent_f->store_line("<true/>");
					}
					if ((bool)p_preset->get("codesign/entitlements/app_sandbox/device_bluetooth")) {
						ent_f->store_line("<key>com.apple.security.device.bluetooth</key>");
						ent_f->store_line("<true/>");
					}
					if ((int)p_preset->get("codesign/entitlements/app_sandbox/files_downloads") == 1) {
						ent_f->store_line("<key>com.apple.security.files.downloads.read-only</key>");
						ent_f->store_line("<true/>");
					}
					if ((int)p_preset->get("codesign/entitlements/app_sandbox/files_downloads") == 2) {
						ent_f->store_line("<key>com.apple.security.files.downloads.read-write</key>");
						ent_f->store_line("<true/>");
					}
					if ((int)p_preset->get("codesign/entitlements/app_sandbox/files_pictures") == 1) {
						ent_f->store_line("<key>com.apple.security.files.pictures.read-only</key>");
						ent_f->store_line("<true/>");
					}
					if ((int)p_preset->get("codesign/entitlements/app_sandbox/files_pictures") == 2) {
						ent_f->store_line("<key>com.apple.security.files.pictures.read-write</key>");
						ent_f->store_line("<true/>");
					}
					if ((int)p_preset->get("codesign/entitlements/app_sandbox/files_music") == 1) {
						ent_f->store_line("<key>com.apple.security.files.music.read-only</key>");
						ent_f->store_line("<true/>");
					}
					if ((int)p_preset->get("codesign/entitlements/app_sandbox/files_music") == 2) {
						ent_f->store_line("<key>com.apple.security.files.music.read-write</key>");
						ent_f->store_line("<true/>");
					}
					if ((int)p_preset->get("codesign/entitlements/app_sandbox/files_movies") == 1) {
						ent_f->store_line("<key>com.apple.security.files.movies.read-only</key>");
						ent_f->store_line("<true/>");
					}
					if ((int)p_preset->get("codesign/entitlements/app_sandbox/files_movies") == 2) {
						ent_f->store_line("<key>com.apple.security.files.movies.read-write</key>");
						ent_f->store_line("<true/>");
					}
					if ((int)p_preset->get("codesign/entitlements/app_sandbox/files_user_selected") == 1) {
						ent_f->store_line("<key>com.apple.security.files.user-selected.read-only</key>");
						ent_f->store_line("<true/>");
					}
					if ((int)p_preset->get("codesign/entitlements/app_sandbox/files_user_selected") == 2) {
						ent_f->store_line("<key>com.apple.security.files.user-selected.read-write</key>");
						ent_f->store_line("<true/>");
					}
				}

				const String &additional_entitlements = p_preset->get("codesign/entitlements/additional");
				if (!additional_entitlements.is_empty()) {
					ent_f->store_line(additional_entitlements);
				}

				ent_f->store_line("</dict>");
				ent_f->store_line("</plist>");
			} else {
				add_message(EXPORT_MESSAGE_ERROR, TTR("Code Signing"), TTR("Could not create entitlements file."));
				err = ERR_CANT_CREATE;
			}

			if ((err == OK) && sandbox && (helpers.size() > 0 || shared_objects.size() > 0)) {
				ent_f = FileAccess::open(hlp_ent_path, FileAccess::WRITE);
				if (ent_f.is_valid()) {
					ent_f->store_line("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
					ent_f->store_line("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
					ent_f->store_line("<plist version=\"1.0\">");
					ent_f->store_line("<dict>");
					ent_f->store_line("<key>com.apple.security.app-sandbox</key>");
					ent_f->store_line("<true/>");
					ent_f->store_line("<key>com.apple.security.inherit</key>");
					ent_f->store_line("<true/>");
					ent_f->store_line("</dict>");
					ent_f->store_line("</plist>");
				} else {
					add_message(EXPORT_MESSAGE_ERROR, TTR("Code Signing"), TTR("Could not create helper entitlements file."));
					err = ERR_CANT_CREATE;
				}
			}
		}

		if ((err == OK) && helpers.size() > 0) {
			Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			for (int i = 0; i < helpers.size(); i++) {
				String hlp_path = helpers[i];
				err = da->copy(hlp_path, tmp_app_path_name + "/Contents/Helpers/" + hlp_path.get_file());
				if (err == OK && sign_enabled) {
					_code_sign(p_preset, tmp_app_path_name + "/Contents/Helpers/" + hlp_path.get_file(), hlp_ent_path, false, true);
				}
				FileAccess::set_unix_permissions(tmp_app_path_name + "/Contents/Helpers/" + hlp_path.get_file(), 0755);
#ifndef UNIX_ENABLED
				if (export_format == "app") {
					add_message(EXPORT_MESSAGE_INFO, TTR("Export"), vformat(TTR("Unable to set Unix permissions for executable \"%s\". Use \"chmod +x\" to set it after transferring the exported .app to macOS or Linux."), "Contents/Helpers/" + hlp_path.get_file()));
				}
#endif
			}
		}

		if (err == OK) {
			Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			for (int i = 0; i < shared_objects.size(); i++) {
				String src_path = ProjectSettings::get_singleton()->globalize_path(shared_objects[i].path);
				if (shared_objects[i].target.is_empty()) {
					String path_in_app = tmp_app_path_name + "/Contents/Frameworks/" + src_path.get_file();
					err = _copy_and_sign_files(da, src_path, path_in_app, sign_enabled, p_preset, ent_path, hlp_ent_path, true, sandbox);
				} else {
					String path_in_app = tmp_app_path_name.path_join(shared_objects[i].target);
					tmp_app_dir->make_dir_recursive(path_in_app);
					err = _copy_and_sign_files(da, src_path, path_in_app.path_join(src_path.get_file()), sign_enabled, p_preset, ent_path, hlp_ent_path, false, sandbox);
				}
				if (err != OK) {
					break;
				}
			}

			Vector<Ref<EditorExportPlugin>> export_plugins{ EditorExport::get_singleton()->get_export_plugins() };
			for (int i = 0; i < export_plugins.size(); ++i) {
				err = _export_macos_plugins_for(export_plugins[i], tmp_app_path_name, da, sign_enabled, p_preset, ent_path, hlp_ent_path, sandbox);
				if (err != OK) {
					break;
				}
			}
		}

		if (err == OK && sign_enabled) {
			int dist_type = p_preset->get("export/distribution_type");
			if (dist_type == 2) {
				String pprof = p_preset->get_or_env("codesign/provisioning_profile", ENV_MAC_CODESIGN_PROFILE).operator String();
				if (!pprof.is_empty()) {
					Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
					err = da->copy(pprof, tmp_app_path_name + "/Contents/embedded.provisionprofile");
				}
			}

			if (ep.step(TTR("Code signing bundle"), 2)) {
				return ERR_SKIP;
			}
			_code_sign(p_preset, tmp_app_path_name, ent_path, true, false);
		}

		String noto_path = p_path;
		bool noto_enabled = (p_preset->get("notarization/notarization").operator int() > 0);
		if (export_format == "dmg") {
			// Create a DMG.
			if (err == OK) {
				if (ep.step(TTR("Making DMG"), 3)) {
					return ERR_SKIP;
				}
				err = _create_dmg(p_path, pkg_name, tmp_base_path_name);
			}
			// Sign DMG.
			if (err == OK && sign_enabled && !ad_hoc) {
				if (ep.step(TTR("Code signing DMG"), 3)) {
					return ERR_SKIP;
				}
				_code_sign(p_preset, p_path, ent_path, false, false);
			}
		} else if (export_format == "pkg") {
			// Create a Installer.
			if (err == OK) {
				if (ep.step(TTR("Making PKG installer"), 3)) {
					return ERR_SKIP;
				}
				err = _create_pkg(p_preset, p_path, tmp_app_path_name);
			}
		} else if (export_format == "zip") {
			// Create ZIP.
			if (err == OK) {
				if (ep.step(TTR("Making ZIP"), 3)) {
					return ERR_SKIP;
				}
				if (FileAccess::exists(p_path)) {
					OS::get_singleton()->move_to_trash(p_path);
				}

				Ref<FileAccess> io_fa_dst;
				zlib_filefunc_def io_dst = zipio_create_io(&io_fa_dst);
				zipFile zip = zipOpen2(p_path.utf8().get_data(), APPEND_STATUS_CREATE, nullptr, &io_dst);

				zip_folder_recursive(zip, tmp_base_path_name, "", pkg_name);

				zipClose(zip, nullptr);
			}
		} else if (export_format == "app" && noto_enabled) {
			// Create temporary ZIP.
			if (err == OK) {
				noto_path = EditorPaths::get_singleton()->get_cache_dir().path_join(pkg_name + ".zip");

				if (ep.step(TTR("Making ZIP"), 3)) {
					return ERR_SKIP;
				}
				if (FileAccess::exists(noto_path)) {
					OS::get_singleton()->move_to_trash(noto_path);
				}

				Ref<FileAccess> io_fa_dst;
				zlib_filefunc_def io_dst = zipio_create_io(&io_fa_dst);
				zipFile zip = zipOpen2(noto_path.utf8().get_data(), APPEND_STATUS_CREATE, nullptr, &io_dst);

				zip_folder_recursive(zip, tmp_base_path_name, tmp_app_dir_name, pkg_name);

				zipClose(zip, nullptr);
			}
		}

		if (err == OK && noto_enabled) {
			if (export_format == "pkg") {
				add_message(EXPORT_MESSAGE_INFO, TTR("Notarization"), TTR("Notarization requires the app to be archived first, select the DMG or ZIP export format instead."));
			} else {
				if (ep.step(TTR("Sending archive for notarization"), 4)) {
					return ERR_SKIP;
				}
				err = _notarize(p_preset, noto_path);
			}
		}

		if (FileAccess::exists(ent_path)) {
			print_verbose("entitlements:\n" + FileAccess::get_file_as_string(ent_path));
		}

		if (FileAccess::exists(hlp_ent_path)) {
			print_verbose("helper entitlements:\n" + FileAccess::get_file_as_string(hlp_ent_path));
		}

		// Clean up temporary entitlements files.
		if (FileAccess::exists(hlp_ent_path)) {
			DirAccess::remove_file_or_error(hlp_ent_path);
		}

		// Clean up temporary .app dir and generated entitlements.
		if ((String)(p_preset->get("codesign/entitlements/custom_file")) == "") {
			tmp_app_dir->remove(ent_path);
		}
		if (export_format != "app") {
			if (tmp_app_dir->change_dir(tmp_base_path_name) == OK) {
				tmp_app_dir->erase_contents_recursive();
				tmp_app_dir->change_dir("..");
				tmp_app_dir->remove(pkg_name);
			}
		} else if (noto_path != p_path) {
			if (FileAccess::exists(noto_path)) {
				DirAccess::remove_file_or_error(noto_path);
			}
		}
	}

	return err;
}

bool EditorExportPlatformMacOS::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
	String err;
	// Look for export templates (official templates first, then custom).
	bool dvalid = exists_export_template("macos.zip", &err);
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

	bool valid = dvalid || rvalid;
	r_missing_templates = !valid;

	// Check the texture formats, which vary depending on the target architecture.
	String architecture = p_preset->get("binary_format/architecture");
	if (architecture == "universal" || architecture == "x86_64") {
		if (!ResourceImporterTextureSettings::should_import_s3tc_bptc()) {
			err += TTR("Cannot export for universal or x86_64 if S3TC BPTC texture format is disabled. Enable it in the Project Settings (Rendering > Textures > VRAM Compression > Import S3TC BPTC).") + "\n";
			valid = false;
		}
	}
	if (architecture == "universal" || architecture == "arm64") {
		if (!ResourceImporterTextureSettings::should_import_etc2_astc()) {
			err += TTR("Cannot export for universal or arm64 if ETC2 ASTC texture format is disabled. Enable it in the Project Settings (Rendering > Textures > VRAM Compression > Import ETC2 ASTC).") + "\n";
			valid = false;
		}
	}
	if (architecture != "universal" && architecture != "x86_64" && architecture != "arm64") {
		ERR_PRINT("Invalid architecture");
	}

	if (!err.is_empty()) {
		r_error = err;
	}
	return valid;
}

bool EditorExportPlatformMacOS::has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const {
	String err;
	bool valid = true;

	int dist_type = p_preset->get("export/distribution_type");
	bool ad_hoc = false;
	int codesign_tool = p_preset->get("codesign/codesign");
	int notary_tool = p_preset->get("notarization/notarization");
	switch (codesign_tool) {
		case 1: { // built-in ad-hoc
			ad_hoc = true;
		} break;
		case 2: { // "rcodesign"
			ad_hoc = p_preset->get_or_env("codesign/certificate_file", ENV_MAC_CODESIGN_CERT_FILE).operator String().is_empty() || p_preset->get_or_env("codesign/certificate_password", ENV_MAC_CODESIGN_CERT_PASS).operator String().is_empty();
		} break;
#ifdef MACOS_ENABLED
		case 3: { // "codesign"
			ad_hoc = (p_preset->get("codesign/identity") == "" || p_preset->get("codesign/identity") == "-");
		} break;
#endif
		default: {
		};
	}

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

	if (dist_type != 2) {
		if (notary_tool > 0) {
			if (notary_tool == 2 || notary_tool == 3) {
				if (!FileAccess::exists("/usr/bin/xcrun") && !FileAccess::exists("/bin/xcrun")) {
					err += TTR("Notarization: Xcode command line tools are not installed.") + "\n";
					valid = false;
				}
			} else if (notary_tool == 1) {
				String rcodesign = EDITOR_GET("export/macos/rcodesign").operator String();
				if (rcodesign.is_empty()) {
					err += TTR("Notarization: rcodesign path is not set. Configure rcodesign path in the Editor Settings (Export > macOS > rcodesign).") + "\n";
					valid = false;
				}
			}
		} else {
			err += TTR("Warning: Notarization is disabled. The exported project will be blocked by Gatekeeper if it's downloaded from an unknown source.") + "\n";
			if (codesign_tool == 0) {
				err += TTR("Code signing is disabled. The exported project will not run on Macs with enabled Gatekeeper and Apple Silicon powered Macs.") + "\n";
			}
		}
	}

	if (codesign_tool > 0) {
		if (ad_hoc) {
			err += TTR("Code signing: Using ad-hoc signature. The exported project will be blocked by Gatekeeper") + "\n";
		}
		if (codesign_tool == 3) {
			if (!FileAccess::exists("/usr/bin/codesign") && !FileAccess::exists("/bin/codesign")) {
				err += TTR("Code signing: Xcode command line tools are not installed.") + "\n";
				valid = false;
			}
		} else if (codesign_tool == 2) {
			String rcodesign = EDITOR_GET("export/macos/rcodesign").operator String();
			if (rcodesign.is_empty()) {
				err += TTR("Code signing: rcodesign path is not set. Configure rcodesign path in the Editor Settings (Export > macOS > rcodesign).") + "\n";
				valid = false;
			}
		}
	}

	if (!err.is_empty()) {
		r_error = err;
	}
	return valid;
}

Ref<Texture2D> EditorExportPlatformMacOS::get_run_icon() const {
	return run_icon;
}

bool EditorExportPlatformMacOS::poll_export() {
	Ref<EditorExportPreset> preset;

	for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
		Ref<EditorExportPreset> ep = EditorExport::get_singleton()->get_export_preset(i);
		if (ep->is_runnable() && ep->get_platform() == this) {
			preset = ep;
			break;
		}
	}

	int prev = menu_options;
	menu_options = (preset.is_valid() && preset->get("ssh_remote_deploy/enabled").operator bool());
	if (ssh_pid != 0 || !cleanup_commands.is_empty()) {
		if (menu_options == 0) {
			cleanup();
		} else {
			menu_options += 1;
		}
	}
	return menu_options != prev;
}

Ref<ImageTexture> EditorExportPlatformMacOS::get_option_icon(int p_index) const {
	return p_index == 1 ? stop_icon : EditorExportPlatform::get_option_icon(p_index);
}

int EditorExportPlatformMacOS::get_options_count() const {
	return menu_options;
}

String EditorExportPlatformMacOS::get_option_label(int p_index) const {
	return (p_index) ? TTR("Stop and uninstall") : TTR("Run on remote macOS system");
}

String EditorExportPlatformMacOS::get_option_tooltip(int p_index) const {
	return (p_index) ? TTR("Stop and uninstall running project from the remote system") : TTR("Run exported project on remote macOS system");
}

void EditorExportPlatformMacOS::cleanup() {
	if (ssh_pid != 0 && OS::get_singleton()->is_process_running(ssh_pid)) {
		print_line("Terminating connection...");
		OS::get_singleton()->kill(ssh_pid);
		OS::get_singleton()->delay_usec(1000);
	}

	if (!cleanup_commands.is_empty()) {
		print_line("Stopping and deleting previous version...");
		for (const SSHCleanupCommand &cmd : cleanup_commands) {
			if (cmd.wait) {
				ssh_run_on_remote(cmd.host, cmd.port, cmd.ssh_args, cmd.cmd_args);
			} else {
				ssh_run_on_remote_no_wait(cmd.host, cmd.port, cmd.ssh_args, cmd.cmd_args);
			}
		}
	}
	ssh_pid = 0;
	cleanup_commands.clear();
}

Error EditorExportPlatformMacOS::run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) {
	cleanup();
	if (p_device) { // Stop command, cleanup only.
		return OK;
	}

	EditorProgress ep("run", TTR("Running..."), 5);

	const String dest = EditorPaths::get_singleton()->get_cache_dir().path_join("macos");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (!da->dir_exists(dest)) {
		Error err = da->make_dir_recursive(dest);
		if (err != OK) {
			EditorNode::get_singleton()->show_warning(TTR("Could not create temp directory:") + "\n" + dest);
			return err;
		}
	}

	String pkg_name;
	if (String(ProjectSettings::get_singleton()->get("application/config/name")) != "") {
		pkg_name = String(ProjectSettings::get_singleton()->get("application/config/name"));
	} else {
		pkg_name = "Unnamed";
	}
	pkg_name = OS::get_singleton()->get_safe_dir_name(pkg_name);

	String host = p_preset->get("ssh_remote_deploy/host").operator String();
	String port = p_preset->get("ssh_remote_deploy/port").operator String();
	if (port.is_empty()) {
		port = "22";
	}
	Vector<String> extra_args_ssh = p_preset->get("ssh_remote_deploy/extra_args_ssh").operator String().split(" ", false);
	Vector<String> extra_args_scp = p_preset->get("ssh_remote_deploy/extra_args_scp").operator String().split(" ", false);

	const String basepath = dest.path_join("tmp_macos_export");

#define CLEANUP_AND_RETURN(m_err)                      \
	{                                                  \
		if (da->file_exists(basepath + ".zip")) {      \
			da->remove(basepath + ".zip");             \
		}                                              \
		if (da->file_exists(basepath + "_start.sh")) { \
			da->remove(basepath + "_start.sh");        \
		}                                              \
		if (da->file_exists(basepath + "_clean.sh")) { \
			da->remove(basepath + "_clean.sh");        \
		}                                              \
		return m_err;                                  \
	}                                                  \
	((void)0)

	if (ep.step(TTR("Exporting project..."), 1)) {
		return ERR_SKIP;
	}
	Error err = export_project(p_preset, true, basepath + ".zip", p_debug_flags);
	if (err != OK) {
		DirAccess::remove_file_or_error(basepath + ".zip");
		return err;
	}

	String cmd_args;
	{
		Vector<String> cmd_args_list = gen_export_flags(p_debug_flags);
		for (int i = 0; i < cmd_args_list.size(); i++) {
			if (i != 0) {
				cmd_args += " ";
			}
			cmd_args += cmd_args_list[i];
		}
	}

	const bool use_remote = p_debug_flags.has_flag(DEBUG_FLAG_REMOTE_DEBUG) || p_debug_flags.has_flag(DEBUG_FLAG_DUMB_CLIENT);
	int dbg_port = EditorSettings::get_singleton()->get("network/debug/remote_port");

	print_line("Creating temporary directory...");
	ep.step(TTR("Creating temporary directory..."), 2);
	String temp_dir;
	err = ssh_run_on_remote(host, port, extra_args_ssh, "mktemp -d", &temp_dir);
	if (err != OK || temp_dir.is_empty()) {
		CLEANUP_AND_RETURN(err);
	}

	print_line("Uploading archive...");
	ep.step(TTR("Uploading archive..."), 3);
	err = ssh_push_to_remote(host, port, extra_args_scp, basepath + ".zip", temp_dir);
	if (err != OK) {
		CLEANUP_AND_RETURN(err);
	}

	{
		String run_script = p_preset->get("ssh_remote_deploy/run_script");
		run_script = run_script.replace("{temp_dir}", temp_dir);
		run_script = run_script.replace("{archive_name}", basepath.get_file() + ".zip");
		run_script = run_script.replace("{exe_name}", pkg_name);
		run_script = run_script.replace("{cmd_args}", cmd_args);

		Ref<FileAccess> f = FileAccess::open(basepath + "_start.sh", FileAccess::WRITE);
		if (f.is_null()) {
			CLEANUP_AND_RETURN(err);
		}

		f->store_string(run_script);
	}

	{
		String clean_script = p_preset->get("ssh_remote_deploy/cleanup_script");
		clean_script = clean_script.replace("{temp_dir}", temp_dir);
		clean_script = clean_script.replace("{archive_name}", basepath.get_file() + ".zip");
		clean_script = clean_script.replace("{exe_name}", pkg_name);
		clean_script = clean_script.replace("{cmd_args}", cmd_args);

		Ref<FileAccess> f = FileAccess::open(basepath + "_clean.sh", FileAccess::WRITE);
		if (f.is_null()) {
			CLEANUP_AND_RETURN(err);
		}

		f->store_string(clean_script);
	}

	print_line("Uploading scripts...");
	ep.step(TTR("Uploading scripts..."), 4);
	err = ssh_push_to_remote(host, port, extra_args_scp, basepath + "_start.sh", temp_dir);
	if (err != OK) {
		CLEANUP_AND_RETURN(err);
	}
	err = ssh_run_on_remote(host, port, extra_args_ssh, vformat("chmod +x \"%s/%s\"", temp_dir, basepath.get_file() + "_start.sh"));
	if (err != OK || temp_dir.is_empty()) {
		CLEANUP_AND_RETURN(err);
	}
	err = ssh_push_to_remote(host, port, extra_args_scp, basepath + "_clean.sh", temp_dir);
	if (err != OK) {
		CLEANUP_AND_RETURN(err);
	}
	err = ssh_run_on_remote(host, port, extra_args_ssh, vformat("chmod +x \"%s/%s\"", temp_dir, basepath.get_file() + "_clean.sh"));
	if (err != OK || temp_dir.is_empty()) {
		CLEANUP_AND_RETURN(err);
	}

	print_line("Starting project...");
	ep.step(TTR("Starting project..."), 5);
	err = ssh_run_on_remote_no_wait(host, port, extra_args_ssh, vformat("\"%s/%s\"", temp_dir, basepath.get_file() + "_start.sh"), &ssh_pid, (use_remote) ? dbg_port : -1);
	if (err != OK) {
		CLEANUP_AND_RETURN(err);
	}

	cleanup_commands.clear();
	cleanup_commands.push_back(SSHCleanupCommand(host, port, extra_args_ssh, vformat("\"%s/%s\"", temp_dir, basepath.get_file() + "_clean.sh")));

	print_line("Project started.");

	CLEANUP_AND_RETURN(OK);
#undef CLEANUP_AND_RETURN
}

EditorExportPlatformMacOS::EditorExportPlatformMacOS() {
	if (EditorNode::get_singleton()) {
#ifdef MODULE_SVG_ENABLED
		Ref<Image> img = memnew(Image);
		const bool upsample = !Math::is_equal_approx(Math::round(EDSCALE), EDSCALE);

		ImageLoaderSVG::create_image_from_string(img, _macos_logo_svg, EDSCALE, upsample, false);
		logo = ImageTexture::create_from_image(img);

		ImageLoaderSVG::create_image_from_string(img, _macos_run_icon_svg, EDSCALE, upsample, false);
		run_icon = ImageTexture::create_from_image(img);
#endif

		Ref<Theme> theme = EditorNode::get_singleton()->get_editor_theme();
		if (theme.is_valid()) {
			stop_icon = theme->get_icon(SNAME("Stop"), EditorStringName(EditorIcons));
		} else {
			stop_icon.instantiate();
		}
	}
}
