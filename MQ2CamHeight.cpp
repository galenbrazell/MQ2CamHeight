/*
 * MQ2CamHeight - adjustable third-person camera focal-point height for the
 * EQEmu RoF2 client (a MacroQuest plugin).
 * Copyright (C) 2026 Galen Brazell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

// MQ2CamHeight.cpp
//
// Raises/lowers the third-person camera's focal point relative to the character,
// so the character can sit below screen-center (view centered on a point above
// them) instead of dead-center. Adjustable live by command or bindable keys, and
// it can auto-lower in low-ceiling areas so it doesn't fight the camera collision.
//
// Target:  MacroQuest (MQNext) compiled against the EQEmu RoF2 client (emu branch).
//
// Focal lever: pLocalPlayer->ViewHeight (PlayerZoneClient @0x190) -- the eye/focal
// point the third-person camera looks at. Raising it lifts the focal point.
//
// Low-ceiling auto-lower: the client already computes the ceiling at your location
// (pLocalPlayer->CeilingHeightAtCurrLocation @0x11b4). We read it each frame, derive
// headroom = ceiling - playerZ, and smoothly scale our offset toward 0 as headroom
// shrinks, so indoors we back off (less camera fighting) and outdoors we apply full
// offset. The scaling is smoothed (lerp) so our own contribution never snaps.
//
// We don't accumulate: each pulse we read ViewHeight's current value; if the game
// changed it (sit/stand, zone, size), we adopt that as the new baseline, then write
// baseline + effectiveOffset. Disabling/unloading restores the game's baseline. Not
// applied in first person.

#include <mq/Plugin.h>

PreSetup("MQ2CamHeight");
PLUGIN_VERSION(0.2);

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static bool  s_enabled = false;   // is the offset active?
static float s_offset  = 5.0f;    // desired height offset (units); + = higher
static float s_step    = 0.5f;    // nudge size for up/down (command + keybinds)

// low-ceiling auto-lower
static bool  s_auto     = true;   // scale the offset down under low ceilings?
static float s_lowRoom  = 10.0f;  // headroom <= this -> offset fully collapses to 0
static float s_openRoom = 30.0f;  // headroom >= this -> full offset
static float s_smooth   = 0.15f;  // lerp factor for the effective offset (0..1)

// runtime (not persisted)
static bool  s_haveBase    = false;
static float s_base        = 0.0f;   // game's baseline ViewHeight
static float s_lastWritten = 0.0f;   // last value we wrote (to detect game changes)
static float s_effOffset   = 0.0f;   // current smoothed effective offset
static bool  s_watch       = false;  // print ceiling/headroom diagnostics?
static int   s_watchTick   = 0;

static const char* INI_SECTION = "MQ2CamHeight";

// Keybind names (bind in game with:  /bind CamHeightUp <key>  etc.)
static const char* KB_UP     = "CamHeightUp";
static const char* KB_DOWN   = "CamHeightDown";
static const char* KB_TOGGLE = "CamHeightToggle";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool IsFirstPerson()
{
	return CDisplay::cameraType != nullptr && *CDisplay::cameraType == EQ_FIRST_PERSON_CAM;
}

static void RestoreBaseline()
{
	if (s_haveBase && pLocalPlayer)
		pLocalPlayer->ViewHeight = s_base;
	s_haveBase = false;
}

static void PrintStatus()
{
	WriteChatf("\ag[CamHeight]\ax %s | offset \ay%.2f\ax | step %.2f | auto %s",
		s_enabled ? "\agON\ax" : "\arOFF\ax", s_offset, s_step,
		s_auto ? "\agon\ax" : "\aroff\ax");
}

// Headroom-based target offset (full offset when open, 0 under a low ceiling).
static float ComputeTargetOffset(float ceiling, float playerZ)
{
	if (!s_auto)
		return s_offset;

	const float headroom = ceiling - playerZ;
	// ceiling <= playerZ or huge headroom == open/outdoors -> full offset
	if (ceiling <= playerZ || headroom >= s_openRoom)
		return s_offset;

	float t = (headroom - s_lowRoom) / (s_openRoom - s_lowRoom);
	if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
	return s_offset * t;
}

// ---------------------------------------------------------------------------
// Persistence  (plugin INIFileName, [MQ2CamHeight] section)
// ---------------------------------------------------------------------------
static void LoadSettings()
{
	s_enabled  = GetPrivateProfileBool(INI_SECTION, "Enabled", false, INIFileName);
	s_offset   = GetPrivateProfileFloat(INI_SECTION, "Offset", 5.0f, INIFileName);
	s_step     = GetPrivateProfileFloat(INI_SECTION, "Step", 0.5f, INIFileName);
	s_auto     = GetPrivateProfileBool(INI_SECTION, "Auto", true, INIFileName);
	s_lowRoom  = GetPrivateProfileFloat(INI_SECTION, "LowRoom", 10.0f, INIFileName);
	s_openRoom = GetPrivateProfileFloat(INI_SECTION, "OpenRoom", 30.0f, INIFileName);
	s_smooth   = GetPrivateProfileFloat(INI_SECTION, "Smooth", 0.15f, INIFileName);
}

static void SaveSettings()
{
	WritePrivateProfileBool(INI_SECTION, "Enabled", s_enabled, INIFileName);
	WritePrivateProfileFloat(INI_SECTION, "Offset", s_offset, INIFileName);
	WritePrivateProfileFloat(INI_SECTION, "Step", s_step, INIFileName);
	WritePrivateProfileBool(INI_SECTION, "Auto", s_auto, INIFileName);
	WritePrivateProfileFloat(INI_SECTION, "LowRoom", s_lowRoom, INIFileName);
	WritePrivateProfileFloat(INI_SECTION, "OpenRoom", s_openRoom, INIFileName);
	WritePrivateProfileFloat(INI_SECTION, "Smooth", s_smooth, INIFileName);
}

// ---------------------------------------------------------------------------
// Adjustment (shared by command + keybinds)
// ---------------------------------------------------------------------------
static void Nudge(float dir)
{
	s_offset += dir * s_step;
	s_enabled = true;
	SaveSettings();
}

// ---------------------------------------------------------------------------
// Keybind callbacks  (signature: void(const char* Name, bool Down))
// ---------------------------------------------------------------------------
static void CamHeightUpBind(const char* Name, bool Down)   { if (Down) Nudge(+1.0f); }
static void CamHeightDownBind(const char* Name, bool Down) { if (Down) Nudge(-1.0f); }
static void CamHeightToggleBind(const char* Name, bool Down)
{
	if (!Down) return;
	s_enabled = !s_enabled;
	if (!s_enabled) RestoreBaseline();
	SaveSettings();
	PrintStatus();
}

// ---------------------------------------------------------------------------
// Command:  /camheight ...
// ---------------------------------------------------------------------------
static void PrintUsage()
{
	WriteChatf("\ag[CamHeight]\ax usage: /camheight [on|off|toggle|status]");
	WriteChatf("\ag[CamHeight]\ax   up | down | set <n> | reset    adjust offset (step %.2f)", s_step);
	WriteChatf("\ag[CamHeight]\ax   step <n>                       change the up/down nudge size");
	WriteChatf("\ag[CamHeight]\ax   auto on|off                    auto-lower under low ceilings");
	WriteChatf("\ag[CamHeight]\ax   room <low> <open> | smooth <n> tune the auto-lower");
	WriteChatf("\ag[CamHeight]\ax   info | watch on|off            show live ceiling/headroom");
	WriteChatf("\ag[CamHeight]\ax   bind keys:  /bind %s <key>  (also %s, %s)", KB_UP, KB_DOWN, KB_TOGGLE);
}

static void PrintInfo()
{
	if (!pLocalPlayer) { WriteChatf("\ag[CamHeight]\ax no local player."); return; }
	const float pz = pLocalPlayer->Z;
	const float ceiling = pLocalPlayer->CeilingHeightAtCurrLocation;
	WriteChatf("\ag[CamHeight]\ax playerZ=%.1f ceiling=%.1f headroom=%.1f | offset=%.2f eff=%.2f auto=%s",
		pz, ceiling, ceiling - pz, s_offset, s_effOffset, s_auto ? "on" : "off");
	WriteChatf("\ag[CamHeight]\ax thresholds: lowRoom=%.1f openRoom=%.1f smooth=%.2f", s_lowRoom, s_openRoom, s_smooth);
}

static void CamHeightCmd(PlayerClient* pChar, const char* szLine)
{
	char arg[MAX_STRING] = { 0 };
	GetArg(arg, szLine, 1);

	char a2[MAX_STRING] = { 0 };
	GetArg(a2, szLine, 2);

	if (arg[0] == 0 || !_stricmp(arg, "status")) { PrintStatus(); return; }
	else if (!_stricmp(arg, "info"))  { PrintInfo(); return; }
	else if (!_stricmp(arg, "on"))      s_enabled = true;
	else if (!_stricmp(arg, "off"))   { s_enabled = false; RestoreBaseline(); }
	else if (!_stricmp(arg, "toggle")){ s_enabled = !s_enabled; if (!s_enabled) RestoreBaseline(); }
	else if (!_stricmp(arg, "up"))    { Nudge(+1.0f); PrintStatus(); return; }
	else if (!_stricmp(arg, "down"))  { Nudge(-1.0f); PrintStatus(); return; }
	else if (!_stricmp(arg, "reset"))   s_offset = 0.0f;
	else if (!_stricmp(arg, "set"))   { if (a2[0] == 0) { WriteChatf("\ag[CamHeight]\ax usage: /camheight set <number>"); return; } s_offset = GetFloatFromString(a2, s_offset); }
	else if (!_stricmp(arg, "step"))    s_step = GetFloatFromString(a2, s_step);
	else if (!_stricmp(arg, "auto"))    s_auto = _stricmp(a2, "off") != 0;   // "auto" alone or "auto on" -> on
	else if (!_stricmp(arg, "smooth"))  s_smooth = GetFloatFromString(a2, s_smooth);
	else if (!_stricmp(arg, "room"))
	{
		char a3[MAX_STRING] = { 0 };
		GetArg(a3, szLine, 3);
		s_lowRoom  = GetFloatFromString(a2, s_lowRoom);
		s_openRoom = GetFloatFromString(a3, s_openRoom);
	}
	else if (!_stricmp(arg, "watch"))
	{
		s_watch = _stricmp(a2, "off") != 0;
		WriteChatf("\ag[CamHeight]\ax watch %s", s_watch ? "on" : "off");
		return;
	}
	else
	{
		const float parsed = GetFloatFromString(arg, 1e9f);
		if (parsed != 1e9f) s_offset = parsed;
		else { PrintUsage(); return; }
	}

	SaveSettings();
	PrintStatus();
}

// ---------------------------------------------------------------------------
// Plugin lifecycle
// ---------------------------------------------------------------------------
PLUGIN_API void InitializePlugin()
{
	DebugSpewAlways("Initializing MQ2CamHeight");
	LoadSettings();

	AddCommand("/camheight", CamHeightCmd);
	AddMQ2KeyBind(KB_UP, CamHeightUpBind);
	AddMQ2KeyBind(KB_DOWN, CamHeightDownBind);
	AddMQ2KeyBind(KB_TOGGLE, CamHeightToggleBind);

	WriteChatf("\ag[CamHeight]\ax loaded. /camheight [on|off|up|down|set <n>|auto|info|status]  |  bind: /bind %s <key>", KB_UP);
	PrintStatus();
}

PLUGIN_API void ShutdownPlugin()
{
	DebugSpewAlways("Shutting down MQ2CamHeight");
	RestoreBaseline();
	RemoveCommand("/camheight");
	RemoveMQ2KeyBind(KB_UP);
	RemoveMQ2KeyBind(KB_DOWN);
	RemoveMQ2KeyBind(KB_TOGGLE);
}

PLUGIN_API void OnPulse()
{
	if (!s_enabled)
		return;

	if (GetGameState() != GAMESTATE_INGAME || !pLocalPlayer)
	{
		s_haveBase = false;
		return;
	}

	if (IsFirstPerson())
	{
		RestoreBaseline();
		return;
	}

	float& vh = pLocalPlayer->ViewHeight;

	// Track the game's baseline.
	bool justCaptured = false;
	if (!s_haveBase || vh != s_lastWritten)
	{
		s_base = vh;
		s_haveBase = true;
		justCaptured = true;
	}

	const float ceiling = pLocalPlayer->CeilingHeightAtCurrLocation;
	const float playerZ = pLocalPlayer->Z;
	const float target  = ComputeTargetOffset(ceiling, playerZ);

	// Snap on (re)capture so enabling/zoning is immediate; smooth otherwise so our
	// own offset never pops as you move between rooms.
	if (justCaptured) s_effOffset = target;
	else              s_effOffset += (target - s_effOffset) * s_smooth;

	const float written = s_base + s_effOffset;
	vh = written;
	s_lastWritten = written;

	if (s_watch && ++s_watchTick >= 30)
	{
		s_watchTick = 0;
		WriteChatf("\ag[CamHeight]\ax pZ=%.1f ceiling=%.1f headroom=%.1f | eff=%.2f/%.2f",
			playerZ, ceiling, ceiling - playerZ, s_effOffset, s_offset);
	}
}
