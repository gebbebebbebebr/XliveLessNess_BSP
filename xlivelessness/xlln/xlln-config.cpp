#include <winsock2.h>
#include "../xlive/xdefs.hpp"
#include "xlln-config.hpp"
#include "debug-text.hpp"
#include "../utils/utils.hpp"
#include "../xlln/xlln.hpp"
#include "../resource.h"
#include "../xlive/xrender.hpp"
#include "../xlive/xsocket.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/voice.hpp"
#include <stdio.h>
#include <vector>
// Link with iphlpapi.lib
#include <iphlpapi.h>

wchar_t *xlln_file_config_path = 0;

static const char XllnConfigHeader[] = "XLLN-Config-Version";
static const char XllnConfigVersion[] = DLL_VERSION_STR;

typedef struct {
	bool keepInvalidLines = false;
	bool saveValuesRead = true;
	std::vector<char*> badConfigEntries;
	std::vector<char*> otherConfigHeaderEntries;
	std::vector<char*> readSettings;
} INTERPRET_CONFIG_CONTEXT;

static int interpretConfigSetting(const char *fileLine, const char *version, size_t lineNumber, void *interpretationContext)
{
	if (!version) {
		return 0;
	}
	INTERPRET_CONFIG_CONTEXT &configContext = *(INTERPRET_CONFIG_CONTEXT*)interpretationContext;

	bool unrecognised = false;
	bool duplicated = false;
	bool incorrect = false;
	size_t fileLineLen = strlen(fileLine);

	if (fileLine[0] == '#' || fileLine[0] == ';' || fileLineLen <= 2) {
		unrecognised = true;
	}
	// When the version of the header has changed.
	else if (version[-1] != 0) {
		unrecognised = true;
	}
	else {
		const char *equals = strchr(fileLine, '=');

		if (!equals || equals == fileLine) {
			unrecognised = true;
		}
		else {
			size_t iValueSearch = 0;
			const char *value = equals + 1;
			while (*value == '\t' || *value == ' ') {
				value++;
			}

			const char *whitespace = equals;
			while (&whitespace[-1] != fileLine && (whitespace[-1] == '\t' || whitespace[-1] == ' ')) {
				whitespace--;
			}
			size_t settingStrLen = whitespace - fileLine;

			char *settingName = new char[settingStrLen + 1];
			memcpy(settingName, fileLine, settingStrLen);
			settingName[settingStrLen] = 0;

			for (char *&readSettingName : configContext.readSettings) {
				if (_stricmp(readSettingName, settingName) == 0) {
					duplicated = true;
					break;
				}
			}

#define SettingNameMatches(x) _stricmp(x, settingName) == 0

			if (!unrecognised && !duplicated && !incorrect) {

				if (SettingNameMatches("xlln_debug_log_blacklist")) {
					if (configContext.saveValuesRead) {
						char *blacklist = CloneString(value);
						char *nextValue = blacklist;
						char *comma;

						do {
							comma = strchr(nextValue, ',');
							if (comma) {
								*comma = 0;
							}

							addDebugTextBlacklist((const char*)nextValue);

							nextValue = comma + 1;
						} while (comma);
						free(blacklist);
					}
				}
				else if (SettingNameMatches("XLiveRender_fps_limit")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							SetFPSLimit(tempuint32);
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_base_port")) {
					uint16_t tempuint16;
					if (sscanf_s(value, "%hu", &tempuint16) == 1) {
						if (configContext.saveValuesRead) {
							if (tempuint16 == 0) {
								tempuint16 = 0xFFFF;
							}
							xlive_base_port = tempuint16;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_net_disable")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							xlive_netsocket_abort = tempuint32 > 0;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_xhv_engine_enabled")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							xlive_xhv_engine_enabled = tempuint32 > 0;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlln_debuglog_level")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "0x%x", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							xlln_debuglog_level = tempuint32;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_network_adapter")) {
					size_t adapterNameLen = strlen(value);
					if (adapterNameLen == 0) {
						if (configContext.saveValuesRead) {
							if (xlive_config_preferred_network_adapter_name) {
								delete[] xlive_config_preferred_network_adapter_name;
								xlive_config_preferred_network_adapter_name = NULL;
							}
						}
					}
					else if (adapterNameLen > 0 && adapterNameLen <= MAX_ADAPTER_NAME_LENGTH) {
						if (configContext.saveValuesRead) {
							if (xlive_config_preferred_network_adapter_name) {
								delete[] xlive_config_preferred_network_adapter_name;
							}
							xlive_config_preferred_network_adapter_name = CloneString(value);
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_ignore_title_network_adapter")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							xlive_ignore_title_network_adapter = tempuint32 > 0;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (_strnicmp("xlive_username_p", settingName, 16) == 0) {
					uint32_t iUser;
					if (sscanf_s(&settingName[16], "%u", &iUser) == 1 || iUser == 0 || iUser > XLIVE_LOCAL_USER_COUNT) {
						iUser--;
						char *username = CloneString(value);
						if (TrimRemoveConsecutiveSpaces(username) <= XUSER_MAX_NAME_LENGTH) {
							if (configContext.saveValuesRead && xlive_users_info[iUser]) {
								memcpy(xlive_users_username[iUser], username, XUSER_NAME_SIZE);
								xlive_users_username[iUser][XUSER_NAME_SIZE - 1] = 0;
							}
						}
						else {
							incorrect = true;
						}
						delete[] username;
					}
					else {
						unrecognised = true;
					}
				}
				else if (_strnicmp("xlive_user_live_enabled_p", settingName, 25) == 0) {
					uint32_t iUser;
					if (sscanf_s(&settingName[25], "%u", &iUser) == 1 || iUser == 0 || iUser > XLIVE_LOCAL_USER_COUNT) {
						iUser--;
						uint32_t tempuint32;
						if (sscanf_s(value, "%u", &tempuint32) == 1) {
							if (configContext.saveValuesRead) {
								xlive_users_live_enabled[iUser] = tempuint32 > 0;
							}
						}
						else {
							incorrect = true;
						}
					}
					else {
						unrecognised = true;
					}
				}
				else if (_strnicmp("xlive_user_auto_login_p", settingName, 23) == 0) {
					uint32_t iUser;
					if (sscanf_s(&settingName[23], "%u", &iUser) == 1 || iUser == 0 || iUser > XLIVE_LOCAL_USER_COUNT) {
						iUser--;
						uint32_t tempuint32;
						if (sscanf_s(value, "%u", &tempuint32) == 1) {
							if (configContext.saveValuesRead) {
								xlive_users_auto_login[iUser] = tempuint32 > 0;
							}
						}
						else {
							incorrect = true;
						}
					}
					else {
						unrecognised = true;
					}
				}
				else {
					unrecognised = true;
				}

				if (!unrecognised && !duplicated && !incorrect) {
					configContext.readSettings.push_back(CloneString(settingName));
				}
			}

			delete[] settingName;
		}
	}

	if (unrecognised || duplicated || incorrect) {

		if (configContext.keepInvalidLines) {
			// When the version of the header has changed.
			if (version[-1] != 0) {
				configContext.otherConfigHeaderEntries.push_back(CloneString(fileLine));
			}
			else {
				configContext.badConfigEntries.push_back(CloneString(fileLine));
			}
		}
	}
	return 0;
}

static uint32_t SaveXllnConfig(const wchar_t *file_config_path, INTERPRET_CONFIG_CONTEXT *configContext)
{
	FILE* fileConfig = 0;

	uint32_t err = _wfopen_s(&fileConfig, file_config_path, L"wb");
	if (err) {
		return err;
	}

#define WriteText(text) fputs(text, fileConfig)
#define WriteTextF(format, ...) fprintf_s(fileConfig, format, __VA_ARGS__)

	WriteText("#--- XLiveLessNess Configuration File ---");
	WriteText("\n");

	WriteText("\n# xlln_debug_log_blacklist:");
	WriteText("\n# A comma separated list of trace function names to ignore in the logging to reduce spam.");
	WriteText("\n");

	WriteText("\n# XLiveRender_fps_limit:");
	WriteText("\n# Valid values: 0 to (2^32 - 1).");
	WriteText("\n#   0 - Disables the built in limiter.");
	WriteText("\n#   60 - (DEFAULT)");
	WriteText("\n# The frequency (times per second) that XLiveRender is permitted to execute.");
	WriteText("\n# XLiveRender is typically used in Title render loops so this value can be considered as an FPS limit.");
	WriteText("\n");

	WriteText("\n# xlive_base_port:");
	WriteText("\n# Valid values: 0 to 65535.");
	WriteText("\n#   0, 65535 - (DEFAULT) Disables the base port override mechanism.");
	WriteText("\n#   2000 - (RECOMMENDED) Use this if enabling.");
	WriteText("\n# The port to base all other ports off of assuming their port numbers are divisable by 100 with the remainder being the offset from the base port. Note that this value is the base port, so a base of 65534 with an offset of anything greater than +1 is not going to be a valid port number (overflows back to 0 which is bad).");
	WriteText("\n# The purpose of this base port is to allow multiple instances of the Title to run where each instance would be spaced from each other by this base port. If the Title doesn't have port mappings compatible with this concept and/or prevents the user from opening multiple instances of the game, an XLLN-Module could be made to hack it to be compatible.");
	WriteText("\n# When this feature is disabled the ports will not be mapped differently from what the Title originally requested.");
	WriteText("\n# For example:");
	WriteText("\n#   - if the Title attempts to use ports 1001 and 1201 then both offsets would be +1 from the base (say the base is 2000 which makes it 2001 in both cases) which also becomes a problem and is bad.");
	WriteText("\n#   - if the the base port is 65534 and the Title attempts to use the port 1002 (offset +2), then it will be mapped to 65536 which isn't a valid 16-bit integer (overflow) and is actually port 0. Port 0 is not a valid port which is an indicator for any port available and the operating system will select one at random which is also bad news.");
	WriteText("\n#   - if the the base port is 65534 and the Title attempts to use the port 1003 (offset +3), then it will be mapped to 65537 which is actually port 1.");
	WriteText("\n");

	WriteText("\n# xlive_net_disable:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - (DEFAULT) All network functionality related to xlive functions is enabled.");
	WriteText("\n#   1 - All network functionality related to xlive functions is disabled.");
	WriteText("\n# Allows turning off xlive network functionality.");
	WriteText("\n");

	WriteText("\n# xlive_xhv_engine_enabled:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - (DEFAULT) The XHV Engine will not run.");
	WriteText("\n#   1 - The XHV Engine is enabled for use.");
	WriteText("\n# Allows activating the XHV Engine which is used for voice chat.");
	WriteText("\n");

	WriteText("\n# xlln_debuglog_level:");
	WriteText("\n# Saves the log level from last use.");
	WriteText("\n");

	WriteText("\n# xlive_network_adapter:");
	WriteText("\n# Example value: {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}");
	WriteText("\n# The network adapter to use if it is available. If it is not available or this setting is blank then it will be auto-selected.");
	WriteText("\n");

	WriteText("\n# xlive_ignore_title_network_adapter:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - (DEFAULT) Factors in the Title's preference for available network adapters.");
	WriteText("\n#   1 - Ignores the setting from the Title.");
	WriteText("\n# On startup in XLiveInitialize(...) the title specifies its preferred network adapter to use (if any). This setting can ignore its preference.");
	WriteText("\n");

	WriteText("\n# xlive_username_p1...:");
	WriteText("\n# Max username length is 15 characters.");
	WriteText("\n# The username for each local profile to use.");
	WriteText("\n# This option does not apply when logging in from XLLN-Modules via XLLNLogin(...).");
	WriteText("\n");

	WriteText("\n# xlive_user_live_enabled_p1...:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - The user will be signed in to a local offline account.");
	WriteText("\n#   1 - The user will be signed in to an online LIVE account.");
	WriteText("\n# This option does not apply when logging in from XLLN-Modules via XLLNLogin(...).");
	WriteText("\n");

	WriteText("\n# xlive_user_auto_login_p1...:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - Do not sign in automatically.");
	WriteText("\n#   1 - Auto sign in.");
	WriteText("\n# Whether the user will be automatically signed in when the Title calls XShowSigninUI(...) and there are no other users already logged in.");
	WriteText("\n# This option does not apply when logging in from XLLN-Modules via XLLNLogin(...).");
	WriteText("\n");

	WriteText("\n");
	WriteTextF("\n[%s:%s]", XllnConfigHeader, XllnConfigVersion);
	WriteText("\nxlln_debug_log_blacklist = ");
	EnterCriticalSection(&xlln_critsec_debug_log);
	for (int i = 0; i < blacklist_len; i++) {
		if (i != 0) {
			WriteText(",");
		}
		WriteText(blacklist[i]);
	}
	LeaveCriticalSection(&xlln_critsec_debug_log);
	WriteTextF("\nXLiveRender_fps_limit = %u", (uint32_t)xlive_fps_limit);
	WriteTextF("\nxlive_base_port = %hu", xlive_base_port == 0xFFFF ? 0 : xlive_base_port);
	WriteTextF("\nxlive_net_disable = %u", xlive_netsocket_abort ? 1 : 0);
	WriteTextF("\nxlive_xhv_engine_enabled = %u", xlive_xhv_engine_enabled ? 1 : 0);
	WriteTextF("\nxlln_debuglog_level = 0x%08x", xlln_debuglog_level);
	WriteTextF("\nxlive_network_adapter = %s", xlive_config_preferred_network_adapter_name ? xlive_config_preferred_network_adapter_name : "");
	WriteTextF("\nxlive_ignore_title_network_adapter = %u", xlive_ignore_title_network_adapter ? 1 : 0);
	for (uint32_t iUser = 0; iUser < XLIVE_LOCAL_USER_COUNT; iUser++) {
		WriteTextF("\nxlive_username_p%u = %s", iUser + 1, xlive_users_username[iUser]);
		WriteTextF("\nxlive_user_live_enabled_p%u = %u", iUser + 1, xlive_users_live_enabled[iUser] ? 1 : 0);
		WriteTextF("\nxlive_user_auto_login_p%u = %u", iUser + 1, xlive_users_auto_login[iUser] ? 1 : 0);
	}
	WriteText("\n\n");

	if (configContext) {
		for (char *&badEntry : configContext->badConfigEntries) {
			WriteText(badEntry);
			WriteText("\n");
		}
		if (configContext->badConfigEntries.size()) {
			WriteText("\n\n");
		}
		for (char *&otherEntry : configContext->otherConfigHeaderEntries) {
			WriteText(otherEntry);
			WriteText("\n");
		}
		if (configContext->otherConfigHeaderEntries.size()) {
			WriteText("\n");
		}
	}

	fclose(fileConfig);

	return ERROR_SUCCESS;
}

static uint32_t XllnConfig(bool save_config)
{
	// Always read the/a config file before saving.
	uint32_t result = ERROR_FUNCTION_FAILED;
	errno_t errorFopen = ERROR_SUCCESS;
	FILE* fileConfig = 0;

	if (xlln_file_config_path) {
		errorFopen = _wfopen_s(&fileConfig, xlln_file_config_path, L"rb");
		if (!fileConfig) {
			if (errorFopen == ENOENT) {
				result = ERROR_FILE_NOT_FOUND;
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "Config file not found: \"%ls\".", xlln_file_config_path);
			}
			else {
				XLLN_DEBUG_LOG_ECODE(errorFopen, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "Config file \"%ls\" read error:", xlln_file_config_path);
				return ERROR_FUNCTION_FAILED;
			}
		}
	}

	wchar_t *configAutoPath = 0;
	wchar_t *configAutoFallbackPath1 = 0;
	wchar_t *configAutoFallbackPath2 = 0;
	if (!fileConfig && !xlln_file_config_path) {
		wchar_t* appdataPath = 0;
		errno_t errorEnvVar = _wdupenv_s(&appdataPath, NULL, L"LOCALAPPDATA");
		if (errorEnvVar) {
			XLLN_DEBUG_LOG_ECODE(errorEnvVar, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "%s %%LOCALAPPDATA%% path unable to be resolved with error:", __func__);
		}

		for (uint32_t searchInstanceId = xlln_local_instance_id; searchInstanceId != 0; searchInstanceId--) {
			bool appdataDirectory = false;
			do {
				if (configAutoPath) {
					free(configAutoPath);
				}
				if (appdataDirectory) {
					configAutoPath = FormMallocString(L"%s/XLiveLessNess/xlln-config-%u.ini", appdataPath, searchInstanceId);
					if (!configAutoFallbackPath1) {
						configAutoFallbackPath1 = CloneString(configAutoPath);
					}
				}
				else {
					configAutoPath = FormMallocString(L"./XLiveLessNess/xlln-config-%u.ini", searchInstanceId);
					if (!configAutoFallbackPath2) {
						configAutoFallbackPath2 = CloneString(configAutoPath);
					}
				}
				errorFopen = _wfopen_s(&fileConfig, configAutoPath, L"rb");
				if (fileConfig) {
					searchInstanceId = 0;
					break;
				}
				if (errorFopen == ENOENT) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "Auto config file not found: \"%ls\".", configAutoPath);
				}
				else {
					XLLN_DEBUG_LOG_ECODE(errorFopen, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "Auto config file \"%ls\" read error:", configAutoPath);
				}
			} while (!errorEnvVar && (appdataDirectory = !appdataDirectory));
			if (searchInstanceId == 0) {
				break;
			}
		}

		if (appdataPath) {
			free(appdataPath);
		}
	}

	INTERPRET_CONFIG_CONTEXT *interpretationContext = 0;
	if (fileConfig) {
		interpretationContext = new INTERPRET_CONFIG_CONTEXT;
		interpretationContext->keepInvalidLines = true;
		interpretationContext->saveValuesRead = !save_config;

		ReadIniFile(fileConfig, true, XllnConfigHeader, XllnConfigVersion, interpretConfigSetting, (void*)interpretationContext);

		for (char *&readSettingName : interpretationContext->readSettings) {
			delete[] readSettingName;
		}
		interpretationContext->readSettings.clear();

		fclose(fileConfig);
		fileConfig = 0;
	}

	if (configAutoPath) {
		free(configAutoPath);
		configAutoPath = 0;
	}

	for (uint8_t i = 0; i < 2; i++) {
		wchar_t *saveToConfigFilePath = 0;
		if (xlln_file_config_path) {
			saveToConfigFilePath = xlln_file_config_path;
			i = 2;
		}
		else if (i == 0) {
			saveToConfigFilePath = configAutoFallbackPath1;
		}
		else if (i == 1) {
			saveToConfigFilePath = configAutoFallbackPath2;
		}

		if (saveToConfigFilePath) {
			wchar_t *saveToConfigPath = PathFromFilename(saveToConfigFilePath);
			uint32_t errorMkdir = EnsureDirectoryExists(saveToConfigPath);
			delete[] saveToConfigPath;
			if (errorMkdir) {
				result = ERROR_DIRECTORY;
				XLLN_DEBUG_LOG_ECODE(errorMkdir, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "%s EnsureDirectoryExists(...) error on path \"%ls\".", __func__, saveToConfigFilePath);
				break;
			}
			else {
				uint32_t errorSaveConfig = SaveXllnConfig(saveToConfigFilePath, interpretationContext);
				if (errorSaveConfig) {
					result = errorSaveConfig;
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "SaveXllnConfig(...) error: %u, \"%ls\".", errorSaveConfig, saveToConfigFilePath);
				}
				else {
					result = ERROR_SUCCESS;
					if (!xlln_file_config_path) {
						xlln_file_config_path = CloneString(saveToConfigFilePath);
					}
				}
				break;
			}
		}
	}
	
	if (configAutoFallbackPath1) {
		delete[] configAutoFallbackPath1;
		configAutoFallbackPath1 = 0;
	}
	if (configAutoFallbackPath2) {
		delete[] configAutoFallbackPath2;
		configAutoFallbackPath2 = 0;
	}

	if (interpretationContext) {
		for (char *&badEntry : interpretationContext->badConfigEntries) {
			delete[] badEntry;
		}
		interpretationContext->badConfigEntries.clear();
		for (char *&otherEntry : interpretationContext->otherConfigHeaderEntries) {
			delete[] otherEntry;
		}
		interpretationContext->otherConfigHeaderEntries.clear();
	}

	return result;
}

uint32_t InitXllnConfig()
{
	XllnConfig(false);
	return ERROR_SUCCESS;
}

uint32_t UninitXllnConfig()
{
	XllnConfig(true);
	return ERROR_SUCCESS;
}
