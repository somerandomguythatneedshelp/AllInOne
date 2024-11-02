#include "pch.h"
#include "AllInOne.h"

BAKKESMOD_PLUGIN(AllInOne, "All In One", plugin_version, PLUGINTYPE_CUSTOM_TRAINING)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void AllInOne::onLoad()
{
	_globalCvarManager = cvarManager;

	SkipInit();
	initAllVars();
	registerNotifiers();
	registerCvars();
	hookAll();
	canLeaveMatch = false;


	// Register cVars
	cvarManager->registerCvar("aio_enabled", "1");
	cvarManager->registerCvar("aio_display_game", "1");
	cvarManager->registerCvar("aio_display_session", "1");
	cvarManager->registerCvar("aio_display_total", "1");
	cvarManager->registerCvar("aio_background_opacity", "0.5");
	cvarManager->registerCvar("aio_overlay_height", "0.5");

	// Hook events
	gameWrapper->HookEventWithCaller<ServerWrapper>("Function TAGame.GFxHUD_TA.HandleStatTickerMessage", std::bind(&AllInOne::onStatTickerMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	gameWrapper->HookEvent("Function GameEvent_Soccar_TA.WaitingForPlayers.BeginState", std::bind(&AllInOne::StartGame, this));
	gameWrapper->HookEvent("Function Engine.PlayerInput.InitInputSystem", std::bind(&AllInOne::StartGame, this));

	UpdateTotalDemos();

	// Load font
	auto gui = gameWrapper->GetGUIManager();
	auto [res, font] = gui.LoadFont(FONT_NAME, FONT_FILE, FONT_SIZE);

	if (res == 0) {
		cvarManager->log("Failed to load the font!");
	}
	else if (res == 1) {
		cvarManager->log("The font will be loaded");
	}
	else if (res == 2 && font) {
		this->font = font;
	}

	// Start rendering overlay
	gameWrapper->SetTimeout(std::bind(&AllInOne::StartGame, this), 0.1f);
	
	std::ifstream timezoneFile(TIMEZONE_FILE_PATH);
	if (timezoneFile.is_open()) {
		timezoneFile >> selectedTimezone;
		timezoneFile.close();
		cvarManager->log("Loaded timezone: " + std::to_string(selectedTimezone));
	}
	else {
		cvarManager->log("Failed to load timezone from file.");
	}

	CVarWrapper cvar = cvarManager->getCvar("aio_selected_timezone");
	if (!cvar) return;

	cvarManager->log(std::to_string(selectedTimezone));

}

void AllInOne::onUnload()
{
	gameWrapper->UnhookEvent("Function TAGame.GFxHUD_TA.HandleStatTickerMessage");
	gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.WaitingForPlayers.BeginState");
	gameWrapper->UnhookEvent("Function Engine.PlayerInput.InitInputSystem");
}

std::string AllInOne::formatFloat(float value, int precision) {
	std::ostringstream out;
	out << std::fixed << std::setprecision(precision) << value;
	return out.str();
}

void AllInOne::onStatTickerMessage(ServerWrapper caller, void* params, std::string eventname)
{
	// not needed
}

void AllInOne::StartGame()
{
	StartRender();
}

bool AllInOne::GetBoolCvar(const std::string name, const bool fallback)
{
	CVarWrapper cvar = cvarManager->getCvar(name);
	if (cvar) return cvar.getBoolValue();
	else return fallback;
}

float AllInOne::GetFloatCvar(const std::string name, const float fallback)
{
	CVarWrapper cvar = cvarManager->getCvar(name);
	if (cvar) return cvar.getFloatValue();
	else return fallback;
}

void AllInOne::UpdateTotalDemos()
{
	std::vector<CareerStatsWrapper::StatValue> stats = CareerStatsWrapper().GetStatValues();
	for (size_t i = 0; i < stats.size(); i++)
	{
		CareerStatsWrapper::StatValue stat = stats[i];
		if (stat.stat_name == "Demolish") {
			total = stat.ranked + stat.unranked + stat.private_;
			return;
		}
	}
}

/* skip replays */


void AllInOne::SkipInit() {
	gameWrapper->HookEvent("Function GameEvent_Soccar_TA.ReplayPlayback.ShouldPlayReplay", std::bind(&AllInOne::Skip, this));

	cvarManager->registerNotifier("toggleskipreplay", [this](std::vector<std::string> params) {
		if (ShouldSkipReplay = !ShouldSkipReplay) {
			gameWrapper->ExecuteUnrealCommand("ReadyUp");

			gameWrapper->HookEvent("Function GameEvent_Soccar_TA.ReplayPlayback.ShouldPlayReplay", std::bind(&AllInOne::Skip, this));

			if (ShouldShowNotification)
				gameWrapper->Toast("Skip", "Auto Skipping is now Enabled!", "", 2.0f);

			if (ShouldLogInChat)
				gameWrapper->LogToChatbox("Auto Skipping is now Enabled!", "Skip");
		}
		else {
			gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.ReplayPlayback.ShouldPlayReplay");

			if (ShouldShowNotification)
				gameWrapper->Toast("Skip", "Auto Skipping is now Disabled!", "", 2.0f);

			if (ShouldLogInChat)
				gameWrapper->LogToChatbox("Auto Skipping is now Disabled!", "Skip");
		}}, "Bind to Toggle Replay Skip", PERMISSION_ALL);

		keybindCvar = std::make_unique<CVarWrapper>(cvarManager->registerCvar("skipKeybind", "0", "Toggle Skipify keybind", false));
		missingCvar = std::make_unique<CVarWrapper>(cvarManager->registerCvar("skipMissingCheckbox", "0", "Don't skip when teammate is missing", false));
		(reEnableCvar = std::make_unique<CVarWrapper>(cvarManager->registerCvar("skipEnableCheckbox", "0", "Re-enable skipping when match ends", false)))->addOnValueChanged([this](std::string, CVarWrapper cvar) {
			if (cvar.getBoolValue())
				gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", [&](std::string eventName) {
				if (!ShouldSkipReplay) {
					gameWrapper->Toast("SkipifyV2", "Auto Skipping is re-enabled!", "", 5.0f);
					gameWrapper->HookEvent("Function GameEvent_Soccar_TA.ReplayPlayback.ShouldPlayReplay", [&](std::string eventName) { gameWrapper->ExecuteUnrealCommand("ReadyUp"); });
					ShouldSkipReplay = true;
				}});
			else
				gameWrapper->UnhookEvent("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded");
			});;
} 

void AllInOne::Skip() {

	auto player = gameWrapper->GetPlayerController();
	if (player.IsNull()) { return; }
	auto pri = player.GetPRI();
	if (pri.IsNull()) { return; }
	auto spectating = pri.IsSpectator();


	if (missingCvar->getBoolValue()) {

		if (spectating && ShouldSkipAsSpectator)
			gameWrapper->ExecuteUnrealCommand("ReadyUp");

		unsigned char team = gameWrapper->GetPlayerController().GetPRI().GetTeamNum();
		ServerWrapper server = gameWrapper->GetCurrentGameState();
		ArrayWrapper<PriWrapper> players = server.GetPRIs();
		unsigned int teamCount = 0;

		for (int i = 0; i < players.Count(); i++) {
			if (players.Get(i).GetTeamNum() == team) {
				teamCount++;
			}
		}

		if ((int)teamCount >= server.GetMaxTeamSize()) {
			gameWrapper->ExecuteUnrealCommand("ReadyUp");
		}
		else {

			gameWrapper->Toast("SkipReplay", "Not skipping because teammate is missing!", "", 5.0f);
		}
	}
	else {
		gameWrapper->ExecuteUnrealCommand("ReadyUp");
	}
}

void AllInOne::OnKeyPressed(ActorWrapper aw, void* params, std::string eventName)
{
	std::string key = gameWrapper->GetFNameByIndex(((keypress_t*)params)->key.Index);
	keyIndex = ((keysIt = find(keys.begin(), keys.end(), key)) != keys.end()) ? (int)(keysIt - keys.begin()) : -1;
	cvarManager->executeCommand("closemenu skipreplaybind; openmenu settings");
	gameWrapper->UnhookEvent("Function TAGame.GameViewportClient_TA.HandleKeyPress");
	gameWrapper->Execute([this](GameWrapper* gw) { OnBind(keys[keyIndex]); });
}

void AllInOne::OnBind(std::string key)
{
	if (key != (keybind = keybindCvar->getStringValue())) {
		std::string toastMsg = "toggleskipreplay bound to " + keys[keyIndex];
		if (keybind != "0") {
			toastMsg += " and " + keybind + " is now unbound";
			cvarManager->executeCommand("unbind " + keybind);
		}
		toastMsg += "!";
		keybindCvar->setValue(key);
		cvarManager->executeCommand("bind " + key + " toggleskipreplay");
		gameWrapper->Toast("SkipReplay", toastMsg, "skipreplay_logo", 5.0f);
	}
}

/* leave instantly */

void AllInOne::initAllVars()
{
	enabled = std::make_shared<bool>(true);
	trainingMap = std::make_shared<std::string>("EuroStadium_Night_P");
	delayLeaveEnabled = std::make_shared<bool>(true);
	casualEnabled = std::make_shared<bool>(true);
	queueEnabled = std::make_shared<bool>(true);
	launchFreeplayEnabled = std::make_shared<bool>(true);
	tournamentsEnabled = std::make_shared<bool>(true);
	privateEnabled = std::make_shared<bool>(false);
	manualQueueEnabled = std::make_shared<bool>(true);
	manualLaunchFreeplayEnabled = std::make_shared<bool>(true);
}

void AllInOne::registerNotifiers()
{
	cvarManager->registerNotifier("toggleAllInOne", [this](std::vector<std::string> args)
		{
			toggleCvar("AllInOneEnabled");
			CVarWrapper cvar = cvarManager->getCvar("AllInOneEnabled");
			if (!cvar) return;
			if (cvar.getBoolValue())
			{
				gameWrapper->Toast("AllInOne", "AllInOne is now enabled!");
			}
			else
			{
				gameWrapper->Toast("AllInOne", "AllInOne is now disabled!");
			}
		}, "", PERMISSION_ALL);

	cvarManager->registerNotifier("leaveMatch", [this](std::vector<std::string> args)
		{
			if (!canLeave())
			{
				gameWrapper->Toast("AllInOne", "Can't leave because game hasn't ended");
				return;
			}
			if (*manualLaunchFreeplayEnabled)
			{
				launchTraining();
			}
			else
			{
				cvarManager->executeCommand("unreal_command disconnect");
			}
			if (*manualQueueEnabled)
			{
				gameWrapper->SetTimeout([this](GameWrapper* gw)
					{
						queue();
					}, QUEUE_DELAY);
			}
		}, "", PERMISSION_ALL);
}

void AllInOne::toggleCvar(const std::string& cvarStr)
{
	CVarWrapper cvar = cvarManager->getCvar(cvarStr);
	if (!cvar) return;
	bool value = cvar.getBoolValue();
	cvarManager->executeCommand(cvarStr + " " + std::to_string(!value));
}

void AllInOne::registerCvars()
{
	cvarManager->registerCvar("AllInOneEnabled", "1")
		.bindTo(enabled);
	cvarManager->registerCvar("trainingMap", "EuroStadium_Night_P")
		.bindTo(trainingMap);
	cvarManager->registerCvar("delayLeaveEnabled", "1")
		.bindTo(delayLeaveEnabled);
	cvarManager->registerCvar("casualEnabled", "1")
		.bindTo(casualEnabled);
	cvarManager->registerCvar("queueEnabled", "1")
		.bindTo(queueEnabled);
	cvarManager->registerCvar("launchFreeplayEnabled", "1")
		.bindTo(launchFreeplayEnabled);
	cvarManager->registerCvar("tournamentsEnabled", "1")
		.bindTo(tournamentsEnabled);
	cvarManager->registerCvar("privateEnabled", "0")
		.bindTo(privateEnabled);
	cvarManager->registerCvar("manualQueueEnabled", "1")
		.bindTo(manualQueueEnabled);
	cvarManager->registerCvar("manualLaunchFreeplayEnabled", "1")
		.bindTo(manualLaunchFreeplayEnabled);

	cvarManager->registerCvar("aio_selected_timezone", "", "Selected timezone index");

}

void AllInOne::Tick() {

	/*if (timezoneChanged)
	{
		CVarWrapper timezoneCvar = cvarManager->getCvar("aio_selected_timezone");
		if (timezoneCvar)
		{
			timezoneCvar.setValue(selectedTimezone);
		}
		timezoneChanged = false;*/

		// If you really need to stop and start rendering, do it here
		// StopRender();
		// StartRender();

		// fuck off chatgpt i aint doing that
	//}
}

void AllInOne::hookAll()
{
	gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded",
		[this](std::string eventname)
		{
			onMatchEnded();
		});
	gameWrapper->HookEvent("Function TAGame.GameEvent_TA.OnCanVoteForfeitChanged",
		[this](std::string eventname)
		{
			onForfeitChanged();
		});
	gameWrapper->HookEvent("Function TAGame.Car_Freeplay_TA.HandleAllAssetsLoaded",
		[this](std::string eventname)
		{
			onLoadedFreeplay();
		});
	gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.Destroyed",
		[this](std::string eventname)
		{
			canLeaveMatch = false;
		});
}

bool AllInOne::canLeave()
{
	ServerWrapper game = gameWrapper->GetOnlineGame();
	if (!game)
	{
		return true;
	}
	return canLeaveMatch;
}

void AllInOne::queue()
{
	cvarManager->executeCommand("queue");
}

void AllInOne::exitGame()
{
	if (*launchFreeplayEnabled)
	{
		launchTraining();
	}
	else
	{
		cvarManager->executeCommand("unreal_command disconnect");
	}
}

void AllInOne::launchTraining()
{
	std::stringstream launchTrainingCommandBuilder;
	launchTrainingCommandBuilder << "start " << *trainingMap << "?Game=TAGame.GameInfo_Tutorial_TA?GameTags=Freeplay";
	gameWrapper->ExecuteUnrealCommand(launchTrainingCommandBuilder.str());
}

bool AllInOne::shouldQueue(int playlistId)
{
	if (!*queueEnabled)
	{
		return false;
	}
	if (playlistId == AUTO_TOURNAMENT || isPrivate(playlistId))
	{
		return false;
	}
	if (isInParty() && !isPartyLeader())
	{
		return false;
	}
	return true;
}

bool AllInOne::shouldLeave(int playlistId)
{
	if (isPrivate(playlistId) && !*privateEnabled)
	{
		return false;
	}
	if (playlistId == AUTO_TOURNAMENT && !*tournamentsEnabled)
	{
		return false;
	}
	if (isCasual(playlistId) && !*casualEnabled)
	{
		return false;
	}
	return true;
}

bool AllInOne::isCasual(int playlistId)
{
	return (playlistId == CASUAL_DUEL || playlistId == CASUAL_DOUBLES || playlistId == CASUAL_STANDARD || playlistId == CASUAL_CHAOS);
}

bool AllInOne::isPrivate(int playlistId)
{
	return (playlistId == PRIVATE || playlistId == CUSTOM_TOURNAMENT || playlistId == EXHIBITION || playlistId == LOCAL_MATCH || playlistId == SEASON);
}

bool AllInOne::isInParty()
{
	PlayerControllerWrapper playerController = gameWrapper->GetPlayerController();
	if (!playerController) return false;
	PriWrapper primaryPRI = playerController.GetPRI();
	if (!primaryPRI) return false;

	UniqueIDWrapper partyLeaderId = primaryPRI.GetPartyLeaderID();
	return partyLeaderId.str() != "0";
}

bool AllInOne::isPartyLeader()
{
	PlayerControllerWrapper playerController = gameWrapper->GetPlayerController();
	if (!playerController) return false;
	PriWrapper primaryPRI = playerController.GetPRI();
	if (!primaryPRI) return false;

	UniqueIDWrapper partyLeaderId = primaryPRI.GetPartyLeaderID();
	UniqueIDWrapper primaryId = primaryPRI.GetUniqueIdWrapper();
	return partyLeaderId == primaryId;
}

void AllInOne::onMatchEnded()
{
	canLeaveMatch = true;
	if (!*enabled) return;

	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (server.IsNull()) return;

	GameSettingPlaylistWrapper playlist = server.GetPlaylist();
	int playlistId = playlist.GetPlaylistId();
	if (!shouldLeave(playlistId)) return;

	if (shouldQueue(playlistId))
	{
		queue();
	}
	if (*delayLeaveEnabled && playlist.GetbRanked())
	{
		gameWrapper->SetTimeout([this](GameWrapper* gw)
			{
				exitGame();
			}, LEAVE_MMR_DELAY);
	}
	else
	{
		exitGame();
	}
}

void AllInOne::onForfeitChanged()
{
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (server.IsNull()) return;
	if (server.GetbCanVoteToForfeit()) return;
	canLeaveMatch = true;

	if (!*enabled) return;

	GameSettingPlaylistWrapper playlist = server.GetPlaylist();
	int playlistId = playlist.GetPlaylistId();
	if (!shouldLeave(playlistId)) return;
	if (playlist.GetbRanked() && *delayLeaveEnabled) return;

	bool shouldQ = shouldQueue(playlistId);
	if (isPartyLeader() && shouldQ) return;

	exitGame();
	if (shouldQ)
	{
		gameWrapper->SetTimeout([this](GameWrapper* gw)
			{
				queue();
			}, QUEUE_DELAY);
	}
}

bool AllInOne::isFreeplayMap(const std::string& map)
{
	if (map.length() < 2) return false;
	return (map[map.length() - 2] == '_' && std::tolower(map[map.length() - 1]) == 'p');
}

void AllInOne::onLoadedFreeplay()
{
	std::string map = gameWrapper->GetCurrentMap();
	if (isFreeplayMap(map))
	{
		*trainingMap = map;
	}
}

/* ################################################## PRESETS ############################################################ */

std::vector<std::string> AllInOne::ReadPresetsFromFile(const std::filesystem::path& filepath) {
	std::vector<std::string> presetNames;
	std::ifstream file(filepath);
	std::string line;

	while (std::getline(file, line)) {
		std::istringstream iss(line);
		std::string firstWord;
		iss >> firstWord;  // This reads only the first word
		if (!firstWord.empty()) {
			presetNames.push_back(firstWord);
		}
	}

	return presetNames;
}
