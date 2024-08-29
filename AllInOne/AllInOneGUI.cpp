#include "pch.h"
#include "AllInOne.h"
#include "bakkesmod/wrappers/GuiManagerWrapper.h"
#include "IMGUI/imgui_internal.h"

const char* timezones[] = { "BST", "EST", "PST" };
static int selectedTimezone = -1;

std::string AllInOne::GetPluginName() {
	return menuTitle;
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



		/*ImGui::TextUnformatted("Press the Set Keybind button below to bind command toggleskipreplay to a key:");
		if (ImGui::SearchableCombo("##keybind combo", &keyIndex, keys, keyText, hintText, 20))
			OnBind(keys[keyIndex]);*/

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

	if (ImGui::CollapsingHeader("Tournament Finder")) {
		// Settings
		CreateToggleableCheckbox("aio_enabled", "Enable overlay");
		CreateToggleableCheckbox("aio_display_game", "Show Tournament starting time");
		CreateToggleableCheckbox("aio_display_session", "Show Time left");
		CreateToggleableCheckbox("aio_display_total", "Show Gamemode");


		// Opacity Slider
		CVarWrapper cvar = cvarManager->getCvar("aio_background_opacity");
		if (!cvar) return;


		float opacity = cvar.getFloatValue();

		ImGui::TextUnformatted("Background opacity:");
		if (ImGui::SliderFloat("", &opacity, 0, 1, "%.2f")) {
			cvar.setValue(opacity);
		}

		if (ImGui::BeginCombo("Select Timezone", selectedTimezone == -1 ? "Select Timezone" : timezones[selectedTimezone]))
		{
			for (int i = 0; i < 3; i++)
			{
				if (ImGui::Selectable(timezones[i], selectedTimezone == i))
				{
					selectedTimezone = i;
					timezoneChanged = true;
				}

				if (selectedTimezone == i)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}


		if (selectedTimezone != -1)
		{
			// Get the current time
			std::time_t now = std::time(0);
			std::tm* localTime = std::localtime(&now);
			int currentHour = localTime->tm_hour;
			int currentMinute = localTime->tm_min;
			std::string currentTime = std::to_string(currentHour) + ":" + (currentMinute < 10 ? "0" : "") + std::to_string(currentMinute);

			// Array pointer to hold the selected timezone times
			std::string* selectedTimes = nullptr;

			// Assign the correct times based on the selected timezone
			switch (selectedTimezone)
			{
			case 0: selectedTimes = BSTtimes; break;
			case 1: selectedTimes = ESTtimes; break;
			case 2: selectedTimes = PSTtimes; break;
			}

			// Find the next tournament time and its position
			int nextTournamentIndex = -1;
			for (int i = 0; i < 5; i++)
			{
				if (selectedTimes[i] > currentTime)
				{
					nextTournamentIndex = i;
					break;
				}
			}

			// Display the next tournament time and corresponding game mode
			if (nextTournamentIndex != -1)
			{
				ImGui::Text("Next Tournament:");
				ImGui::BulletText("Time: %s", selectedTimes[nextTournamentIndex].c_str());
				ImGui::BulletText("Mode: %s", GameModes[nextTournamentIndex].c_str());
			}
			else
			{
				ImGui::Text("No upcoming tournaments today.");
			}
		}
		ImGui::Separator();
		ImGui::TextUnformatted("Note:\nReposition the overlay by dragging it whilst the Bakkesmod menu is open.");

	}

	if (ImGui::CollapsingHeader("Presets")) {
		std::fstream inputFile(gameWrapper->GetDataFolder() / "cameras_rlcs.data", std::ios::in); // where the camera settings are going to be shown in settings

		ImGui::BeginTabBar("Tab");

		if (ImGui::BeginTabItem("Camera")) {
			ImGui::TextUnformatted("E");

			// tf does this code do i forgot

			



			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Controls")) {
			ImGui::TextUnformatted("not yet");
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Help/Info")) {
			ImGui::TextUnformatted("Here because you dont know how to use this addon? Look below");
			ImGui::Separator();
			ImGui::TextUnformatted("no");
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	

	// Credit
	ImGui::Separator();
	ImGui::TextUnformatted("AllInOne by ");
	ImGui::SameLine(0, 0);
	ImGui::TextColored({ 1, 1, 1, 1 }, "IReallyLikeMilk");

    ImGui::Separator();

    ImGui::Text("Socials");

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 40);
    if (ImGui::Button("Github")) {
        system("start https://github.com/somerandomguythatneedshelp");
    }
}

void AllInOne::Render()
{
	// Check enabled
	if (!GetBoolCvar("aio_enabled")) return;

	// Settings
	bool displayGame = GetBoolCvar("aio_display_game");
	bool displaySession = GetBoolCvar("aio_display_session");
	bool displayTotal = GetBoolCvar("aio_display_total");

	if (!displayGame && !displaySession && !displayTotal) return;

	// Style
	float opacity = GetFloatCvar("aio_background_opacity", 0.5);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, { 0, 0, 0, opacity });

	// Font
	if (!font) {
		auto gui = gameWrapper->GetGUIManager();
		ImFont* retrievedFont = gui.GetFont(FONT_NAME);
		if (retrievedFont) font = retrievedFont;
	}
	if (font) ImGui::PushFont(font);

	const ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;
	if (!ImGui::Begin(menuTitle.c_str(), &isWindowOpen_, flags))
	{
		ImGui::End();
		return;
	}

	// Size & Position
	float height = 16.0f + (FONT_SIZE + 4) * (displayGame + displaySession + displayTotal);
	ImGui::SetWindowSize({ 330, height });
	ImGui::SetWindowPos({ ImGui::GetIO().DisplaySize.x - ImGui::GetWindowSize().x + 10, ImGui::GetWindowSize().x / 3 }, ImGuiCond_FirstUseEver);

	// Get current time and tournament information
	std::time_t now = std::time(0);
	std::tm* localTime = std::localtime(&now);
	int currentHour = localTime->tm_hour;
	int currentMinute = localTime->tm_min;
	std::string currentTime = std::to_string(currentHour) + ":" + (currentMinute < 10 ? "0" : "") + std::to_string(currentMinute);

	std::string* selectedTimes = nullptr;
	switch (selectedTimezone)
	{
	case 0: selectedTimes = BSTtimes; break;
	case 1: selectedTimes = ESTtimes; break;
	case 2: selectedTimes = PSTtimes; break;
	default: break;
	}
	if (selectedTimes)
	{
		std::time_t now = std::time(0);
		std::tm* localTime = std::localtime(&now);
		int currentHour = localTime->tm_hour;
		int currentMinute = localTime->tm_min;
		std::string currentTime =
			(currentHour < 10 ? "0" : "") + std::to_string(currentHour) + ":" +
			(currentMinute < 10 ? "0" : "") + std::to_string(currentMinute);

		int nextTournamentIndex = -1;
		for (int i = 0; i < 5; i++)
		{
			if (selectedTimes[i] > currentTime)
			{
				nextTournamentIndex = i;
				break;
			}
		}

		if (nextTournamentIndex != -1)
		{
			std::string nextTime = selectedTimes[nextTournamentIndex];
			std::string gameMode = GameModes[nextTournamentIndex];

			int nextHour, nextMinute;
			sscanf(nextTime.c_str(), "%d:%d", &nextHour, &nextMinute);

			int currentTotalMinutes = currentHour * 60 + currentMinute;
			int nextTotalMinutes = nextHour * 60 + nextMinute;
			int minutesUntilNextTournament = nextTotalMinutes - currentTotalMinutes;

			// Handle case where next tournament is tomorrow
			if (minutesUntilNextTournament < 0)
			{
				minutesUntilNextTournament += 24 * 60;
			}

			int hoursRemaining = minutesUntilNextTournament / 60;
			int minutesRemaining = minutesUntilNextTournament % 60;

			std::stringstream timeRemainingStream;
			timeRemainingStream << std::setfill('0') << std::setw(2) << hoursRemaining << "h "
				<< std::setfill('0') << std::setw(2) << minutesRemaining << "m";
			std::string timeRemaining = timeRemainingStream.str();

			// Content in columns
			ImGui::Columns(2, 0, false);

			ImGui::SetColumnWidth(0, 160);
			if (displayGame) ImGui::Text("TOURNAMENT TIME");
			if (displaySession) ImGui::Text("TIME LEFT");
			if (displayTotal) ImGui::Text("GAMEMODE");

			ImGui::NextColumn();
			ImGui::SetColumnWidth(1, 100);

			if (displayGame) {
				RightAlignTextInColumn(nextTime);
				ImGui::Text(nextTime.c_str());
			}
			if (displaySession) {
				RightAlignTextInColumn(timeRemaining);
				ImGui::Text(timeRemaining.c_str());
			}
			if (displayTotal) {
				RightAlignTextInColumn(gameMode);
				ImGui::Text(gameMode.c_str());
			}

			ImGui::Columns();
		}
		else
		{
			ImGui::Text("No upcoming tournaments today.");
		}
	}
	else
	{
		ImGui::Text("Please select a timezone.");
	}

	ImGui::End();

	// Remove font
	if (font) ImGui::PopFont();

	// Remove style vars
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();
}


std::string AllInOne::GetMenuTitle()
{
	return menuTitle;
}

std::string AllInOne::GetMenuName()
{
	return menuName;
}

void AllInOne::SetImGuiContext(uintptr_t ctx)
{
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

bool AllInOne::ShouldBlockInput()
{
	return ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
}

bool AllInOne::IsActiveOverlay()
{
	return false;
}

void AllInOne::OnOpen()
{
	isWindowOpen_ = true;
}

void AllInOne::RightAlignTextInColumn(const std::string text)
{
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(text.c_str()).x
		- ImGui::GetScrollX() - 2 * ImGui::GetStyle().ItemSpacing.x);
}

void AllInOne::OnClose()
{
	isWindowOpen_ = false;
}

void AllInOne::StartRender()
{
	cvarManager->executeCommand("openmenu " + GetMenuName());
}

void AllInOne::StopRender()
{
	cvarManager->executeCommand("closemenu " + GetMenuName());
}

void AllInOne::CreateToggleableCheckbox(const std::string name, const char* const display)
{
	CVarWrapper cvar = cvarManager->getCvar(name);
	if (!cvar) return;

	bool state = cvar.getBoolValue();

	if (ImGui::Checkbox(display, &state)) {
		cvar.setValue(state);
	}
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