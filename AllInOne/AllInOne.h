#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "version.h"
#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "bakkesmod/wrappers/GameEvent/GameEventWrapper.h"
#include <iomanip>
#include <fstream>
#include <iostream>
#include <Windows.h>
#include <regex>
#include <ctime>
#include <sstream>
#include <filesystem>
#include "clipboardxx.hpp"
#include <algorithm>
#include "bakkesmod/wrappers/GuiManagerWrapper.h"
#include "IMGUI/imgui_internal.h"

#define FONT_SIZE 32
#define FONT_NAME "Bourgeois"
#define FONT_FILE "Bourgeois-BoldItalic.ttf"

constexpr int PRIVATE = 6;
constexpr int CUSTOM_TOURNAMENT = 22;
constexpr int AUTO_TOURNAMENT = 34;
constexpr int CASUAL_DUEL = 1;
constexpr int CASUAL_DOUBLES = 2;
constexpr int CASUAL_STANDARD = 3;
constexpr int CASUAL_CHAOS = 4;
constexpr int EXHIBITION = 8;
constexpr int LOCAL_MATCH = 24;
constexpr int SEASON = 7;

constexpr float LEAVE_MMR_DELAY = 0.4F;
constexpr float QUEUE_DELAY = 0.1F;

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

struct StatTickerParams {
	uintptr_t Receiver;
	uintptr_t Victim;
	uintptr_t StatEvent;
};

class AllInOne: public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginSettingsWindow, public BakkesMod::Plugin::PluginWindow
{
	//Boilerplate
	virtual void onLoad();
	virtual void onUnload();
	virtual void Tick();

	void RenderSettings() override;
	std::string GetPluginName() override;
	void SetImGuiContext(uintptr_t ctx) override;

	bool isWindowOpen_ = true;
	bool isMinimized_ = false;
	std::string menuTitle = "All In One";
	std::string menuName = "aiotourney";

	virtual void Render() override;
	virtual std::string GetMenuName() override;
	virtual std::string GetMenuTitle() override;
	virtual bool ShouldBlockInput() override;
	virtual bool IsActiveOverlay() override;
	virtual void OnOpen() override;
	virtual void OnClose() override;

	std::string formatFloat(float value, int precision);

	unsigned int total = 0;

	void StartGame();
	void onStatTickerMessage(ServerWrapper caller, void* params, std::string eventname);
	bool GetBoolCvar(const std::string name, const bool fallback = false);
	float GetFloatCvar(const std::string name, const float fallback = 1);
	void UpdateTotalDemos();

	// Render
	ImFont* font;

	void StartRender();
	void StopRender();

	void RightAlignTextInColumn(const std::string text);
	void CreateToggleableCheckbox(const std::string name, const char* const display);

	/* skip replays */

public:

	void Skip();
	void SkipInit();

	std::string keybind;
	const std::vector<std::string> keys = {
		"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q",
		"R", "S", "T", "U", "V", "W", "X", "Y", "Z", "Escape", "Tab", "Tilde", "ScrollLock", "Pause", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine", "Zero", "Underscore",
		"Equals", "Backslash", "LeftBracket", "RightBracket", "Enter", "CapsLock", "Semicolon", "Quote", "LeftShift", "Comma", "Period", "Slash", "RightShift", "LeftControl", "LeftAlt", "SpaceBar",
		"RightAlt", "RightControl", "Left", "Up", "Down", "Right", "Home", "End", "Insert", "PageUp", "Delete", "PageDown", "NumLock", "Divide", "Multiply", "Subtract", "Add", "NumPadOne", "NumPadTwo",
		"NumPadThree", "NumPadFour", " NumPadFive", "NumPadSix", "NumPadSeven", "NumPadEight", "NumPadNine", "NumPadZero", "Decimal", "LeftMouseButton", "RightMouseButton", "ThumbMouseButton",
		"ThumbMouseButton2", "MouseScrollUp", "MouseScrollDown", "MouseX", "MouseY", "XboxTypeS_LeftThumbStick", "XboxTypeS_RightThumbStick", "XboxTypeS_DPad_Up", "XboxTypeS_DPad_Left",
		"XboxTypeS_DPad_Right", "XboxTypeS_DPad_Down", "XboxTypeS_Back", "XboxTypeS_Start", "XboxTypeS_Y", "XboxTypeS_X", "XboxTypeS_B", "XboxTypeS_A", "XboxTypeS_LeftShoulder", "XboxTypeS_RightShoulder",
		"XboxTypeS_LeftTrigger", "XboxTypeS_RightTrigger", "XboxTypeS_LeftTriggerAxis", "XboxTypeS_RightTriggerAxis", "XboxTypeS_LeftX", "XboxTypeS_LeftY", "XboxTypeS_RightX",	"XboxTypeS_RightY"
	};
	std::vector<std::string>::const_iterator keysIt;


	struct keypress_t {
		int ControllerID;
		struct {
			int Index;
			int Number;
		} key;
		unsigned char EventType;
		float AmountDepressed;
		unsigned int bGamepad;
		unsigned int ReturnValue;
	};

	int keyIndex = 0;
	std::unique_ptr<CVarWrapper> reEnableCvar;
	std::unique_ptr<CVarWrapper> keybindCvar;
	std::unique_ptr<CVarWrapper> missingCvar;

	bool ShouldSkipAsSpectator = true;
	bool ShouldSkipReplay = true;

	bool ShouldShowNotification = true;
	bool ShouldLogInChat = false;

	void OnBind(std::string key);
	void OnKeyPressed(ActorWrapper aw, void* params, std::string eventName);

	/* auto leave */

public:
	void initAllVars();
	void registerCvars();
	void registerNotifiers();
	void toggleCvar(const std::string&);
	bool canLeave();
	void onForfeitChanged();
	void onMatchEnded();
	void onLoadedFreeplay();
	void queue();
	void exitGame();
	void launchTraining();
	bool isFreeplayMap(const std::string&);
	void hookAll();
	bool shouldQueue(int playlistId);
	bool isCasual(int playlistId);
	bool shouldLeave(int playlistId);
	bool isPrivate(int playlistId);
	bool isInParty();
	bool isPartyLeader();

	void renderCheckbox(const std::string&, const char*);

	bool canLeaveMatch;

	std::shared_ptr<bool> enabled;
	std::shared_ptr<std::string> trainingMap;
	std::shared_ptr<bool> delayLeaveEnabled;
	std::shared_ptr<bool> casualEnabled;
	std::shared_ptr<bool> queueEnabled;
	std::shared_ptr<bool> launchFreeplayEnabled;
	std::shared_ptr<bool> tournamentsEnabled;
	std::shared_ptr<bool> privateEnabled;
	std::shared_ptr<bool> manualQueueEnabled;
	std::shared_ptr<bool> manualLaunchFreeplayEnabled;

	std::string BSTtimes[5] = { "14:00", "16:00", "18:00", "20:00", "22:00" };
	std::string GreenWichtimes[5] = { "13:00", "15:00", "17:00", "19:00", "21:00" };
	std::string ESTtimes[5] = { "15:00", "17:00", "19:00", "21:00", "23:00" };
	std::string PSTtimes[5] = { "12:00", "14:00", "16:00", "19:00", "21:00" };

	std::string GameModes[5] = { "Extra Modes", "2s Soccar", "3s Soccar", "2s Soccar", "3s Soccar" };

	static constexpr int numTimezones = 4;
	bool timezoneSelection[numTimezones] = { false, false, false, false };

	int selectedTimezone;
	const char* timezones[4] = { "BST", "GMT", "EST", "PST" };
	bool timezoneChanged = false;

	std::unique_ptr<CVarWrapper> timezoneCvar;

	/*  i actually dont know what to name this add on, 5 seconds of thinking later its called Presets */

	 //FOV HEIGHT ANGLE STIFFNESS TRANSITIONSPEED DISTANCE SWIVELSPEED
	struct CP_CameraSettings {
		std::string name;

		int FOV;
		int Distance;
		int Height;
		int Angle;
		float Stiffness;
		float SwivelSpeed;
		float TransitionSpeed;
		std::string code;
	};

	std::vector<CP_CameraSettings> cameras;
	int oldSelected = 0;
	int selected = 0;
	bool settingsChanged = false;
	int InputNameError = 0;
	std::string CopiedCode; 
	std::string CodeName; // these 2 vars are different i swear 
	std::string PresetName;
	CP_CameraSettings tempCamera;
	CP_CameraSettings PlayerCameraSettings;
	
	float PresetsDistance;
	float PresetsFOV;
	float PresetsHeight;
	float PresetsPitch; // angle
	float PresetsStiffness;
	float PresetsSwivelSpeed;
	float PresetsTransitionSpeed;


	std::vector<std::string> ReadPresetsFromFile(const std::filesystem::path& filepath);
 // where the camera settings are going to be shown in settings


	float aFOV = gameWrapper->GetCamera().GetCameraSettings().FOV;
	float aDistance = gameWrapper->GetCamera().GetCameraSettings().Distance;
	float aHeight = gameWrapper->GetCamera().GetCameraSettings().Height;
	float aPitch = gameWrapper->GetCamera().GetCameraSettings().Pitch;
	float aStiffness = gameWrapper->GetCamera().GetCameraSettings().Stiffness;
	float aSwivelSpeed = gameWrapper->GetCamera().GetCameraSettings().SwivelSpeed;
	float aTransitionSpeed = gameWrapper->GetCamera().GetCameraSettings().TransitionSpeed;

	int FOV = static_cast<int>(aFOV);
	int Distance = static_cast<int>(aDistance);
	int Height = static_cast<int>(aHeight);
	int Angle = static_cast<int>(aPitch);
	int Stiffness = static_cast<int>(aStiffness);;
	int SwivelSpeed = static_cast<int>(aSwivelSpeed);
	int TransitionSpeed = static_cast<int>(aTransitionSpeed);

	void RemovePreset(const std::filesystem::path& filePath, const std::string& presetName);

	const std::filesystem::path TIMEZONE_FILE_PATH = gameWrapper->GetDataFolder() / "AllInOne";


};

