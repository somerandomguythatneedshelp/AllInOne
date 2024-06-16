#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "pch.h"
#include "AllInOne.h"


BAKKESMOD_PLUGIN(AllInOne, "All In One", plugin_version, PLUGINTYPE_FREEPLAY)

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
}

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

void AllInOne::renderCheckbox(const std::string& cvar, const char* desc)
{
	CVarWrapper enableCvar = cvarManager->getCvar(cvar);
	if (!enableCvar) return;
	bool enabled = enableCvar.getBoolValue();
	if (ImGui::Checkbox(desc, &enabled))
	{
		enableCvar.setValue(enabled);
	}
}

void AllInOne::RenderSettings() {

	if (ImGui::CollapsingHeader("Skip Replays"))
	{
		static const char* keyText = "Key List";
		static const char* hintText = "Type to Filter";
		static bool reEnable = reEnableCvar->getBoolValue();
		static bool missing = missingCvar->getBoolValue();

		if (!keyIndex) {
			keybind = keybindCvar->getStringValue();
			keyIndex = (keysIt = find(keys.begin(), keys.end(), keybind)) != keys.end() ? (int)(keysIt - keys.begin()) : -1;
		}

		if (ImGui::Checkbox("Don't skip when teammate is missing", &missing))
			missingCvar->setValue(missing);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("To allow disconnected teammate to reconnect");
		if (ImGui::Checkbox("Show Notifications", &ShouldShowNotification))
			reEnableCvar->setValue(ShouldShowNotification);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Show a notification on changing whether to skip or not");
		if (ImGui::Checkbox("Log To Chat", &ShouldLogInChat))
			reEnableCvar->setValue(ShouldLogInChat);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Show a Chat Messgae on changing whether to skip or not");
		if (ImGui::Checkbox("Skip As Spectator", &ShouldSkipAsSpectator))
			reEnableCvar->setValue(ShouldSkipAsSpectator);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("When spectating a game, skip the replay, requires Replay skip to be on (pretty useless ik)");



		ImGui::TextUnformatted("Press the Set Keybind button below to bind command toggleskipreplay to a key:");
		if (ImGui::SearchableCombo("##keybind combo", &keyIndex, keys, keyText, hintText, 20))
			OnBind(keys[keyIndex]);

		ImGui::SameLine();
		if (ImGui::Button("Set Keybind", ImVec2(0, 0))) {
			gameWrapper->Execute([this](GameWrapper* gw) {
				cvarManager->executeCommand("closemenu settings; openmenu skipreplaybind");
				gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.GameViewportClient_TA.HandleKeyPress", std::bind(&AllInOne::OnKeyPressed, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
				});
		}

		ImGui::SameLine();
		if (ImGui::Button("Unbind", ImVec2(0, 0))) {
			if (keyIndex != -1) {
				keybindCvar->setValue("0");
				cvarManager->executeCommand("unbind " + keys[keyIndex]);
				gameWrapper->Toast("SkipReplay", "toggleskipreplay is now unbound!", "skipreplay_logo", 5.0f);
				keyIndex = -1;
			}
		}

		ImGui::TextUnformatted("The toggleskipreplay keybind can be used to disable and enable autoskipping for that rare occasion that you dont want to skip the replay!");
	}

	if (ImGui::CollapsingHeader("Auto Leave")) {
		renderCheckbox("AutoLeaveEnabled", "Enable plugin");
		ImGui::Separator();
		renderCheckbox("queueEnabled", "Enable auto queue");
		renderCheckbox("launchFreeplayEnabled", "Launch freeplay on game end");
		renderCheckbox("delayLeaveEnabled", "Delay leave in order for MMR to update(goal replays will play at the end of matches)");
		ImGui::Separator();
		renderCheckbox("casualEnabled", "Enable for casual");
		renderCheckbox("tournamentsEnabled", "Enable for tournaments");
		renderCheckbox("privateEnabled", "Enable for private matches and custom tournaments");
		ImGui::Separator();
		ImGui::Text("Settings for leaving/queueing using keybind");
		ImGui::Spacing();
		renderCheckbox("manualQueueEnabled", "Enable queueing when leaving using keybind");
		renderCheckbox("manualLaunchFreeplayEnabled", "launch freeplay when leaving using keybind");
	}
	
	ImGui::Separator();
	ImGui::SetCursorPosY(ImGui::GetWindowSize().y - 10);
	ImGui::TextUnformatted("v1 made by milkinabag aka IReallyLikeMilk");
}

void AllInOne::Render() {
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize, ImGuiCond_Once, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(140, 40), ImGuiCond_Once);
	ImGui::Begin("Set Keybind", &isWindowOpen, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
	ImGui::SetWindowFontScale(1.6f);
	ImGui::TextUnformatted("Press any key");
	ImGui::End();
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


/*  ##################################################################################   */

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


void AllInOne::OnOpen()
{
	isWindowOpen = true;
}

void AllInOne::OnClose()
{
	isWindowOpen = false;
}

std::string AllInOne::GetPluginName()
{
	return "All In One";
}

std::string AllInOne::GetMenuName()
{
	return "menu";
}

std::string AllInOne::GetMenuTitle()
{
	return "";
}

void AllInOne::SetImGuiContext(uintptr_t ctx)
{
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

bool AllInOne::ShouldBlockInput()
{
	return false;
}

bool AllInOne::IsActiveOverlay()
{
	return true;
}