#include "sig.h"
#include <Windows.h>
#include <cmath>
#include "interface.h"
#include "detours/detours.h"
#include "sdk/vector.h"
#include "sdk/in_buttons.h"
#include "sdk/globalvars_base.h"
#include "sdk/cusercmd.h"
#include "sdk/IEngineTrace.h"
#include "sdk/ConVar.h"
#include "sdk/C_BasePlayer.h"
#include "sdk/EngineFrametime.h"
#include "sdk/Entity.h"
#include "sdk/EngineClient.h"
#include "sdk/EngineTool.h"

#define M_PI		3.14159265358979323846
#define M_PI_F		((float)(M_PI))
#define RAD2DEG( x  )  ( (float)(x) * (float)(180.f / M_PI_F) )
#define GET_FLOAT(pvalue) (*(float*)(pvalue))

typedef void(__fastcall* apply_mouse_fn)(uintptr_t, QAngle&, CUserCmd*, float, float);
typedef void(__fastcall* controller_move_fn)(uintptr_t, float, CUserCmd*);
typedef bool(__fastcall* create_move_fn)(uintptr_t, float, CUserCmd*);
typedef void(__cdecl* conmsg_fn)(const char*, ...);
typedef void(__cdecl* cbuf_addtext_fn)(const char*);
typedef double(__cdecl* Plat_FloatTimeFn)();

Plat_FloatTimeFn Plat_FloatTime;
apply_mouse_fn o_apply_mouse;
controller_move_fn o_controller_move;
create_move_fn o_create_move;
conmsg_fn con_msg;
cbuf_addtext_fn o_cbuf_addtext;

uintptr_t p_m_yaw, p_sensitivity, p_gJumpPrediction;

HMODULE pDllMain;

CGlobalVarsBase* globals;
CEngineClient* EngineClient;
CClientEntityList* ClientEntityList;
CEngineTool* EngineTool;
IEngineTrace* EngineTraceClient;
C_BasePlayer* LocalPlayer;
CCvar* CVar;
Frametime* frametime;

bool validate_angle = true;
bool autostrafe = false;
bool enabled = false;
bool IsNearWall;

float power = 0.f, gain = 1.f;
float g_mouse_x, old_mouse_x;
float CInput_frametime;
float wall_distance = 25.f;
float old_viewangles_y;

int lastmoveframe;

bool is_near_wall(C_BasePlayer* LocalPlayer, const QAngle& viewAngles, float wall_distance)
{
	Vector pos = LocalPlayer->GetAbsOrigin();
	Vector forward;
	AngleVectors(viewAngles, forward);
	forward.z = 0;
	forward.NormalizeInPlace();

	Vector left(-forward.y, forward.x, 0);
	left.NormalizeInPlace();
	Vector right = left * -1;
	Vector back = forward * -1;

	Vector directions[4] = { forward, left, right, back };

	for (int DelayStraight = 0; DelayStraight < 4; DelayStraight++) {
		Ray_t ray;
		Vector end = pos + directions[DelayStraight] * wall_distance;
		ray.Init(pos, end);
		static CTraceFilterWorldOnly filter;
		trace_t trace;
		trace.fraction;
		EngineTraceClient->TraceRay(ray, MASK_SOLID, &filter, &trace);
		if (trace.fraction < 1.0f)
			return true;
	}
	return false;
}

void __fastcall apply_mouse_hk(uintptr_t thisptr, QAngle& viewangles, CUserCmd* cmd, float mouse_x, float mouse_y)
{
	LocalPlayer = (C_BasePlayer*)ClientEntityList->GetClientEntity(EngineClient->GetLocalPlayer());

	g_mouse_x = mouse_x;

	IsNearWall = is_near_wall(LocalPlayer, viewangles, wall_distance);

	if (IsNearWall) return o_apply_mouse(thisptr, viewangles, cmd, mouse_x, mouse_y);

	if (!autostrafe) { if (!cmd->sidemove && !cmd->forwardmove) return o_apply_mouse(thisptr, viewangles, cmd, mouse_x, mouse_y); }

	if (!enabled) return o_apply_mouse(thisptr, viewangles, cmd, mouse_x, mouse_y);

	if (!mouse_x && globals->framecount - lastmoveframe > (autostrafe ? (int)(3 / (1 / globals->interval_per_tick * globals->frametime)) : 1))
		return o_apply_mouse(thisptr, viewangles, cmd, mouse_x, mouse_y);

	if (mouse_x) lastmoveframe = globals->framecount;

	Vector velocity = LocalPlayer->getVelocity();

	if (velocity.Length2D() <= 30.f || !GetAsyncKeyState(VK_SPACE)) return o_apply_mouse(thisptr, viewangles, cmd, mouse_x, mouse_y); 

	float start_smooth_ratio = 1.f, adjusted_power = power;
	if (velocity.Length2D() <= 250.f)
	{
		start_smooth_ratio = (velocity.Length2D() / 250.f);
		adjusted_power *= start_smooth_ratio;
	}

	float real_frametime = min(frametime->host_frametime, CInput_frametime);
	frametime->host_frametime -= real_frametime;

	float perfect_diff = RAD2DEG(asin(30.f / velocity.Length2D()));
	float vel_yaw = RAD2DEG(std::atan2(velocity.y, velocity.x));

	auto normalize_yaw = [](float diff)
	{
		while (diff > 180.f) diff -= 360.f;
		while (diff < -180.f) diff += 360.f;
		return diff;
	};

	float diff = autostrafe ? normalize_yaw(old_viewangles_y - vel_yaw) : std::copysign(perfect_diff, mouse_x == 0.f ? old_mouse_x : mouse_x);

	diff *= (1 / globals->interval_per_tick) * real_frametime * start_smooth_ratio;

	old_mouse_x = mouse_x;
	
	float m_yaw = GET_FLOAT(p_m_yaw);
	float sensitivity = GET_FLOAT(p_sensitivity);

	mouse_x = mouse_x * (1 - adjusted_power) + diff * gain / m_yaw * adjusted_power;

	if (validate_angle) mouse_x = round(mouse_x / sensitivity) * sensitivity;
	
	o_apply_mouse(thisptr, viewangles, cmd, mouse_x, mouse_y);

}

void __fastcall controller_move_hk(uintptr_t thisptr, float flInputSampleTime, CUserCmd* cmd)
{
	CInput_frametime = flInputSampleTime;

	return o_controller_move(thisptr, flInputSampleTime, cmd);
}

bool __fastcall create_move_hk(uintptr_t thisptr, float flInputSampleTime, CUserCmd* cmd)
{ 
	if (cmd->command_number == 0) return o_create_move(thisptr, flInputSampleTime, cmd);

	if (autostrafe && (cmd->buttons & IN_JUMP) && g_mouse_x != 0.f)
	{
		cmd->sidemove = copysignf(400.f, g_mouse_x);
		cmd->buttons |= g_mouse_x < 0.f ? IN_MOVELEFT : IN_MOVERIGHT;
	}

	old_viewangles_y = cmd->viewangles.y;

	return o_create_move(thisptr, flInputSampleTime, cmd);
}

void __cdecl cbuf_addtext_hk(const char* text)
{
	char* cmd = new char[255];
	strcpy_s(cmd, 255, text);

	int start = 0;
	char* buf = cmd;
	char* buffer = buf;
	while (*buf && *buf++ == ' ') ++start;
	while (*buf++);
	int end = buf - buffer - 1;
	while (end > 0 && buffer[end - 1] == ' ') --end;
	buffer[end] = 0;
	if (end > start || start != 0)
	{
		buf = buffer + start;
		while ((*buffer++ = *buf++));
	}

	if (strstr(cmd, "opti_power ") == cmd || strcmp(cmd, "opti_power") == 0)
	{
		if (strcmp(cmd, "opti_power") != 0)
			power = atof(cmd + 10) / 100.f;

		con_msg("optimizer power: %.2f%%\n", power * 100.f);
	}
	else if (strstr(cmd, "opti_gain ") == cmd || strcmp(cmd, "opti_gain") == 0)
	{
		if (strcmp(cmd, "opti_gain") != 0)
			gain = atof(cmd + 9) / 100.f;

		con_msg("optimizer target gain: %.2f%%\n", gain * 100.f);
	}
	else if (strstr(cmd, "opti_wall ") == cmd || strcmp(cmd, "opti_wall") == 0)
	{
		if (strcmp(cmd, "opti_wall") != 0)
			wall_distance = atof(cmd + 9);

		con_msg("wall distance to disable opti: %.2f%\n", wall_distance);
	}
	else if (strcmp(cmd, "opti_toggle") == 0)
	{
		enabled = !enabled;
		con_msg("strafe optimizer %s\n", enabled ? "ON" : "OFF");
	}
	else if (strcmp(cmd, "opti_validate") == 0)
	{
		validate_angle = !validate_angle;
		con_msg("optimizer angle validation %s\n", validate_angle ? "ON" : "OFF");
	}
	else if (strcmp(cmd, "autostrafe_toggle") == 0)
	{
		autostrafe = !autostrafe;
		con_msg("autostrafe %s\n", autostrafe ? "ON" : "OFF");
	}
	else
	{
		return o_cbuf_addtext(text);
	}

	delete[] cmd;
}

DWORD WINAPI main_thread(LPVOID lpThreadParameter)
{
	HMODULE tier0 = GetModuleHandle(L"tier0.dll");
	con_msg = (conmsg_fn)GetProcAddress(tier0, "Msg");

	Plat_FloatTime = (Plat_FloatTimeFn)GetProcAddress(tier0, "Plat_FloatTime");
	ClientEntityList = (CClientEntityList*)GetInterface("client.dll", "VClientEntityList003");
	EngineClient = (CEngineClient*)GetInterface("engine.dll", "VEngineClient013");
	EngineTool = (CEngineTool*)GetInterface("engine.dll", "VENGINETOOL003");
	EngineTraceClient = (IEngineTrace*)GetInterface("engine.dll", "EngineTraceClient003");
	CVar = (CCvar*)GetInterface("vstdlib.dll", "VEngineCvar004");

	o_cbuf_addtext = get_sig("engine.dll", "48 8B 4A 28 E8").rel32(0x5).as<cbuf_addtext_fn>();
	o_apply_mouse = get_sig("client.dll", "48 8B C4 48 89 70 ? 48 89 78 18 ? 56 48 81 EC").as<apply_mouse_fn>();
	o_controller_move = get_sig("client.dll", "48 89 5C 24 ? 57 48 83 EC ? 80 B9 A0").as<controller_move_fn>();
	o_create_move = get_sig("client.dll", "40 53 48 83 EC ? 0F 29 74 24 ? 49 8B D8 0F 28").as<create_move_fn>();
	p_m_yaw = get_sig("client.dll", "48 8B 05 ? ? ? ? 41 0F 28 C3 F3 0F 10 3D").rel32(0x3).as<uintptr_t>(0x1C);
	p_sensitivity = get_sig("client.dll", "48 8B 05 ? ? ? ? 48 8B 5C 24 ? 48 8B 74 24 ? F3 0F 59 40 ? F3 0F 11 05 ? ? ? ? 48").rel32(0x3).as<uintptr_t>(0x1C);
	p_gJumpPrediction = get_sig("client.dll", "F6 40 28 02 75").as<uintptr_t>();
	globals = *get_sig("client.dll", "4C 8B 05 ? ? ? ? 8B DA 48 8D 15").rel32(0x3).as<CGlobalVarsBase**>();
	frametime = get_sig("engine.dll", "F3 0F 10 15 ? ? ? ? 0F 57 C0 0F 2F D0").rel32(0x4).as<Frametime*>();
	
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)o_cbuf_addtext, cbuf_addtext_hk);
	DetourAttach(&(PVOID&)o_create_move, create_move_hk);
	DetourAttach(&(PVOID&)o_apply_mouse, apply_mouse_hk);
	DetourAttach(&(PVOID&)o_controller_move, controller_move_hk);
	DetourTransactionCommit();

	con_msg("\nstrafe optimizer\n");
	con_msg("END - exit opti\n");

	while (!(GetAsyncKeyState(VK_END) & 0x8000))
	{
		Sleep(100);
	}

	APE_disable();
	con_msg("Opti - Detached\n");

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)o_cbuf_addtext, cbuf_addtext_hk);
	DetourDetach(&(PVOID&)o_apply_mouse, apply_mouse_hk);
	DetourDetach(&(PVOID&)o_controller_move, controller_move_hk);
	DetourDetach(&(PVOID&)o_create_move, create_move_hk);
	DetourTransactionCommit();

	FreeLibraryAndExitThread(pDllMain, EXIT_SUCCESS);

	return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:

		pDllMain = hModule;

		DisableThreadLibraryCalls(hModule);

		CreateThread(0, 0, main_thread, hModule, 0, 0);
	}
	return TRUE;
}
