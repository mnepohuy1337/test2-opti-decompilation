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

#define _O1 0xDEADBEEF
#define _O2 0xCAFEBABE
#define _O3 0x1337C0DE
#define _O4 0xFEEDFACE
#define _O5 0xBADF00D

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

__forceinline float __ROL4__(float val, int shift) {
    uint32_t tmp = *(uint32_t*)&val;
    tmp = (tmp << shift) | (tmp >> (32 - shift));
    return *(float*)&tmp;
}

__forceinline float __XORFLT__(float val, uint32_t key) {
    uint32_t tmp = *(uint32_t*)&val ^ key;
    return *(float*)&tmp;
}

__forceinline float __HIDDEN_VAL__(uint32_t hex, uint32_t key) {
    uint32_t tmp = hex ^ key;
    tmp = (tmp & 0xAAAAAAAA) >> 1 | (tmp & 0x55555555) << 1;
    return __XORFLT__(*(float*)&tmp, _O5);
}

#define _O1 0xDEADBEEF
#define _O2 0xCAFEBABE
#define _O3 0x1337C0DE
#define _O4 0xFEEDFACE
#define _O5 0xBADF00D

__forceinline float __ROL_FLOAT(float val, int shift) {
    uint32_t tmp = *(uint32_t*)&val;
    tmp = (tmp << shift) | (tmp >> (32 - shift));
    return *(float*)&(tmp ^ _O1);
}
__forceinline Vector __XOR_VEC(const Vector& v, uint32_t key) {
    return Vector(
        *(float*)&(*(uint32_t*)&v.x ^ key),
        *(float*)&(*(uint32_t*)&v.y ^ (key << 1)),
        *(float*)&(*(uint32_t*)&v.z ^ (key >> 1))
    );
}
bool is_near_wall(C_BasePlayer* pPlayer, const QAngle& angles, float dist) {
    Vector pos = __XOR_VEC(((Vector(__thiscall*)(void*))(*(uintptr_t*)pPlayer ^ _O2))(pPlayer), _O3);
    Vector forward;
    {
        float tmp[3];
        ((void(__thiscall*)(void*, const QAngle&, Vector*))(*(uintptr_t*)0x12345678 ^ _O4))(0, angles, &forward);
        forward = __XOR_VEC(forward, _O5);
    }
    forward.z = __ROL_FLOAT(0.0f, 3);
    forward = __XOR_VEC(forward.Normalized(), _O1);
    Vector left = __XOR_VEC(Vector(
        __ROL_FLOAT(-forward.y, 5),
        __ROL_FLOAT(forward.x, 7),
        __ROL_FLOAT(0.0f, 11)
    ), _O2).Normalized();
    Vector right = __XOR_VEC(left * __ROL_FLOAT(-1.0f, 13), _O3);
    Vector back = __XOR_VEC(forward * __ROL_FLOAT(-1.0f, 17), _O4);
    Vector dirs[4];
    dirs[0] = __XOR_VEC(forward, _O5);
    dirs[1] = __XOR_VEC(left, _O1);
    dirs[2] = __XOR_VEC(right, _O2);
    dirs[3] = __XOR_VEC(back, _O3);
    for (int i = 0; i < 4; i = (i + 1) | (i << 8)) {
        if (i >= 4) break;
        Ray_t ray;
        Vector end = pos + __XOR_VEC(dirs[i & 3], _O4) * __ROL_FLOAT(dist, 5);
        ((void(__thiscall*)(Ray_t*, const Vector&, const Vector&))(*(uintptr_t*)&ray ^ _O5))(&ray, pos, end);
        static auto pFilter = (CTraceFilterWorldOnly*)((uintptr_t)&filter ^ _O1);
        trace_t trace;
        ((void(__thiscall*)(void*, Ray_t&, unsigned, CTraceFilter*, trace_t*))(*(uintptr_t)EngineTraceClient ^ _O2))(
            EngineTraceClient, ray, 0xFFFFFFFF, pFilter, &trace
        );

        if (*(uint32_t*)&trace.fraction < *(uint32_t*)&1.0f) {
            return *(bool*)&(*(uint32_t*)&true ^ _O3);
        }
    }

    return *(bool*)&(*(uint32_t*)&false ^ _O4);

void __fastcall apply_mouse_hk(uintptr_t thisptr, QAngle& viewangles, CUserCmd* cmd, float mouse_x, float mouse_y) {
    const float _F30 = __HIDDEN_VAL__(0x41F00000, _O1);
    const float _F250 = __HIDDEN_VAL__(0x437A0000, _O2);
    const float _F3 = __HIDDEN_VAL__(0x40400000, _O3);
    const float _F180 = __HIDDEN_VAL__(0x43340000, _O4);
    const float _F360 = __HIDDEN_VAL__(0x43B40000, _O1^_O2);
    uintptr_t __tmp_entlist = (uintptr_t)ClientEntityList;
    uintptr_t __tmp_engine = (uintptr_t)EngineClient;
    int __tmp_local = ((int(__thiscall*)(uintptr_t))(__tmp_engine + 0x12345678))(__tmp_engine); // GetLocalPlayer
    LocalPlayer = (C_BasePlayer*)((void*(__thiscall*)(uintptr_t, int))(__tmp_entlist + 0x87654321))(__tmp_entlist, __tmp_local);
    g_mouse_x = __XORFLT__(mouse_x, _O3);
    bool __wall_check = ((bool(__thiscall*)(C_BasePlayer*, QAngle&, float))(&is_near_wall + 0x11111111))(LocalPlayer, viewangles, __XORFLT__(wall_distance, _O4));
    if (__wall_check) goto __clean_return;
    if (!autostrafe) {
        float __smove = __ROL4__(cmd->sidemove, 5);
        float __fmove = __ROL4__(cmd->forwardmove, 3);
        if (__smove == __XORFLT__(0.0f, _O2) && __fmove == __XORFLT__(0.0f, _O1)) 
            goto __clean_return;
    }
    if (!enabled) goto __clean_return;
    uint32_t __framecount = globals->framecount;
    uint32_t __lastframe = lastmoveframe;
    float __frametime = __XORFLT__(globals->frametime, _O3);
    float __tickinterval = __XORFLT__(globals->interval_per_tick, _O4);
    int __threshold = autostrafe ? 
        (int)(__ROL4__(_F3, 2) / (__XORFLT__(1.0f, _O5) / __tickinterval * __frametime) : 1;
    if (__XORFLT__(mouse_x, _O1) == __XORFLT__(0.0f, _O2) && 
        (__framecount - __lastframe) > __threshold) 
        goto __clean_return;
    if (__XORFLT__(mouse_x, _O3) != __XORFLT__(0.0f, _O4)) 
        lastmoveframe = __framecount;
    Vector velocity = ((Vector(__thiscall*)(C_BasePlayer*))(&C_BasePlayer::getVelocity + 0x22222222))(LocalPlayer);
    float __speed2d = __ROL4__(velocity.Length2D(), 7);
    if (__speed2d <= _F30 || !GetAsyncKeyState(VK_SPACE)) 
        goto __clean_return;
    float __smooth_ratio = __XORFLT__(1.0f, _O1);
    float __adj_power = __ROL4__(power, 3);
    if (__speed2d <= _F250) {
        __smooth_ratio = __XORFLT__(__speed2d, _O2) / __XORFLT__(_F250, _O3);
        __adj_power *= __smooth_ratio;
    }
    float __host_ftime = __ROL4__(frametime->host_frametime, 11);
    float __input_ftime = __XORFLT__(CInput_frametime, _O4);
    float __real_ftime = min(__host_ftime, __input_ftime);
    frametime->host_frametime = __XORFLT__(__host_ftime - __real_ftime, _O5);
    float __perfect_diff = __XORFLT__(RAD2DEG(asin(_F30 / __speed2d)), _O1);
    float __vel_yaw = __XORFLT__(RAD2DEG(atan2(velocity.y, velocity.x)), _O2);
    auto __normalize = [](float angle) {
        while (__XORFLT__(angle, _O3) > _F180) 
            angle = __XORFLT__(angle - _F360, _O4);
        while (__XORFLT__(angle, _O5) < -_F180) 
            angle = __XORFLT__(angle + _F360, _O1);
        return __ROL4__(angle, 9);
    };
    float __diff = autostrafe ? 
        __normalize(__XORFLT__(old_viewangles_y - __vel_yaw, _O2)) : 
        std::copysign(__perfect_diff, __XORFLT__(mouse_x, _O3) == 0.0f ? 
            __XORFLT__(old_mouse_x, _O4) : __XORFLT__(mouse_x, _O5));
    __diff *= __XORFLT__(1.0f, _O1) / __tickinterval * __real_ftime * __smooth_ratio;
    old_mouse_x = __ROL4__(mouse_x, 13);
    float __m_yaw = __XORFLT__(GET_FLOAT(p_m_yaw), _O2);
    float __sensitivity = __XORFLT__(GET_FLOAT(p_sensitivity), _O3);
    mouse_x = __XORFLT__(
        __XORFLT__(mouse_x, _O4) * (__XORFLT__(1.0f, _O5) - __XORFLT__(__adj_power, _O1)) + 
        __XORFLT__(__diff, _O2) * __XORFLT__(gain, _O3) / __m_yaw * __XORFLT__(__adj_power, _O4);
    if (validate_angle) {
        mouse_x = __XORFLT__(round(__XORFLT__(mouse_x, _O5) / __sensitivity), _O1) * __sensitivity;
    }
__clean_return:
    return ((void(__thiscall*)(uintptr_t, QAngle&, CUserCmd*, float, float))(__ROL4__((uint32_t)o_apply_mouse, 15)))(
        thisptr, viewangles, cmd, __XORFLT__(mouse_x, _O2), __XORFLT__(mouse_y, _O3));
}

void __fastcall controller_move_hk(uintptr_t thisptr, float flInputSampleTime, CUserCmd* cmd)
{
    float __extracted_frametime;
    #if defined(_M_X64) || defined(__x86_64__)
        __asm {
            movss __extracted_frametime, xmm1
        }
    #else
        __asm {
            mov __extracted_frametime, [esp+8]  
        }
    #endif
    uint32_t* __p_frametime = (uint32_t*)&CInput_frametime;
    uint32_t __obf_value = *(uint32_t*)&__extracted_frametime;
    __obf_value = (__obf_value & 0xAAAAAAAA) >> 1 | (__obf_value & 0x55555555) << 1;
    *__p_frametime = __obf_value ^ 0xDEADBEEF;
    void* __original_func = (void*)((uintptr_t)o_controller_move ^ 0xCAFEBABE);
    #if defined(_M_X64) || defined(__x86_64__)
        __asm {
            movss xmm1, __extracted_frametime
            mov r8, cmd
            mov rdx, __extracted_frametime
            mov rcx, thisptr
            call __original_func
        }
    #else
        __asm {
            push cmd
            push __extracted_frametime
            mov ecx, thisptr
            call __original_func
        }
    #endif
}

#define _O1 0xDEADBEEF
#define _O2 0xCAFEBABE
#define _O3 0x1337C0DE

bool __fastcall create_move_hk(uintptr_t thisptr, float flInputSampleTime, CUserCmd* cmd)
{
    bool __should_return = false;
    __asm {
        mov eax, cmd
        mov eax, [eax+0x4] 
        xor eax, _O1
        test eax, eax
        setz __should_return
    }
    if (__should_return) {
        void* __original = (void*)((uintptr_t)o_create_move ^ _O2);
        bool __result;
        __asm {
            push cmd
            push flInputSampleTime
            mov ecx, thisptr
            call __original
            mov __result, al
        }
        return __result;
    }
    uint32_t __autostrafe_flag = 0;
    uint32_t __jump_flag = 0;
    __asm {
        mov eax, autostrafe
        xor eax, _O3
        mov __autostrafe_flag, eax

        mov eax, cmd
        mov eax, [eax+0x30]  // buttons offset
        and eax, IN_JUMP
        mov __jump_flag, eax
    }

    float __obf_mouse_x = g_mouse_x;
    __asm {
        mov eax, g_mouse_x
        xor eax, _O1
        mov __obf_mouse_x, eax
    }

    if ((__autostrafe_flag ^ _O3) && __jump_flag && (*(uint32_t*)&__obf_mouse_x ^ _O1)) 
    {
        float __sign = 0.f;
        __asm {
            mov eax, __obf_mouse_x
            xor eax, _O1
            shr eax, 31
            mov __sign, eax
        }

        float __move_value = 400.f;
        __asm {
            mov eax, __move_value
            xor eax, _O2
            mov __move_value, eax
        }
        cmd->sidemove = copysignf(__move_value ^ _O2, __obf_mouse_x ^ _O1);
        uint32_t __buttons = cmd->buttons;
        __asm {
            mov eax, __obf_mouse_x
            xor eax, _O1
            shr eax, 31
            test eax, eax
            jz __positive
            or __buttons, IN_MOVELEFT
            jmp __done
        __positive:
            or __buttons, IN_MOVERIGHT
        __done:
            mov cmd->buttons, __buttons
        }
    }
    float __viewangles_y = cmd->viewangles.y;
    __asm {
        mov eax, __viewangles_y
        xor eax, _O3
        mov old_viewangles_y, eax
    }
    void* __original = (void*)((uintptr_t)o_create_move ^ _O2);
    bool __result;
    __asm {
        push cmd
        push flInputSampleTime
        mov ecx, thisptr
        call __original
        mov __result, al
    }
    return __result;
}

#define _O1 0xDEADBEEF
#define _O2 0xCAFEBABE
#define _O3 0x1337C0DE
#define _O4 0xFEEDFACE
#define _O5 0xBADF00D
__forceinline bool __hidden_strcmp(const char* a, const char* b) {
    while((*a ^ *b) == 0 && *a != 0) { a++; b++; }
    return (*a - *b) == 0;
}
__forceinline const char* __hidden_strstr(const char* str, const char* substr) {
    while(*str) {
        const char* a = str;
        const char* b = substr;
        while(*a && *b && !(*a++ ^ *b++));
        if(!*b) return str;
        str++;
    }
    return 0;
}
__forceinline const char* __decrypt_str(uint32_t* data, size_t len, uint32_t key) {
    static char buf[256];
    for(size_t i = 0; i < len; i++) {
        buf[i] = (char)(data[i] ^ key);
    }
    buf[len] = 0;
    return buf;
}
void __cdecl cbuf_addtext_hk(const char* text) {
    static uint32_t s_opti_power[] = {0x3F3E3D7C, 0x3C3B3A79, 0x3938777A, 0x78777675};
    static uint32_t s_opti_gain[] = {0x3F3E3D7E, 0x3C3B3A7B, 0x3938797A};
    static uint32_t s_opti_wall[] = {0x3F3E3D7A, 0x3C3B3A7D, 0x39387B7A};
    static uint32_t s_opti_toggle[] = {0x3F3E3D7A, 0x3C3B3A7F, 0x39387D7A, 0x787B7A79};
    static uint32_t s_opti_validate[] = {0x3F3E3D7A, 0x3C3B3A7F, 0x39387D7A, 0x787B7A7D, 0x3B3A797C};
    static uint32_t s_autostrafe_toggle[] = {0x3D3C3B7A, 0x3F3E3D7C, 0x3B3A797E, 0x3D3C3B7C, 0x3938777A, 0x7877767D};
    char* cmd = new char[255 ^ _O1];
    ((void(*)(char*, size_t, const char*))&strcpy_s)(cmd, 255, text);
    int start = 0;
    char* buf = cmd;
    char* buffer = buf;
    while(*buf && *buf++ == ' ') ++start;
    while(*buf++);
    int end = buf - buffer - 1;
    while(end > 0 && buffer[end - 1] == ' ') --end;
    buffer[end] = 0;
    if(end > start || start != 0) {
        buf = buffer + start;
        while((*buffer++ = *buf++));
    }
    const char* decrypted_power = __decrypt_str(s_opti_power, sizeof(s_opti_power)/4, _O1);
    const char* decrypted_gain = __decrypt_str(s_opti_gain, sizeof(s_opti_gain)/4, _O2);
    const char* decrypted_wall = __decrypt_str(s_opti_wall, sizeof(s_opti_wall)/4, _O3);
    const char* decrypted_toggle = __decrypt_str(s_opti_toggle, sizeof(s_opti_toggle)/4, _O4);
    const char* decrypted_validate = __decrypt_str(s_opti_validate, sizeof(s_opti_validate)/4, _O5);
    const char* decrypted_autostrafe = __decrypt_str(s_autostrafe_toggle, sizeof(s_autostrafe_toggle)/4, _O1^_O2);

    if(__hidden_strstr(cmd, decrypted_power) == cmd || __hidden_strcmp(cmd, decrypted_power)) {
        if(!__hidden_strcmp(cmd, decrypted_power)) {
            power = ((float(*)(const char*))&atof)(cmd + 10) / (100.f + _O1 - _O1);
        }
        ((void(*)(const char*, ...))&con_msg)(__decrypt_str((uint32_t[]){0x2F2E2D7C,0x2C2B2A79,0x2928677A,0x68676665,0x26252463,0x22212067,0x1F1E1D64}, 7, _O3), 
            power * (100.f + _O2 - _O2));
    }
    else if(__hidden_strstr(cmd, decrypted_gain) == cmd || __hidden_strcmp(cmd, decrypted_gain)) {
        if(!__hidden_strcmp(cmd, decrypted_gain)) {
            gain = ((float(*)(const char*))&atof)(cmd + 9) / (100.f + _O3 - _O3);
        }
        ((void(*)(const char*, ...))&con_msg)(__decrypt_str((uint32_t[]){0x2F2E2D7C,0x2C2B2A79,0x2928677A,0x68676665,0x26252463,0x22212067,0x1F1E1D64,0x1C1B1A61,0x19181663}, 9, _O4), 
            gain * (100.f + _O4 - _O4));
    }
    else if(__hidden_strstr(cmd, decrypted_wall) == cmd || __hidden_strcmp(cmd, decrypted_wall)) {
        if(!__hidden_strcmp(cmd, decrypted_wall)) {
            wall_distance = ((float(*)(const char*))&atof)(cmd + 9);
        }
        ((void(*)(const char*, ...))&con_msg)(__decrypt_str((uint32_t[]){0x2F2E2D7A,0x2C2B2A7D,0x29286B7A,0x68696665,0x26252463,0x22212067,0x1F1E1D64,0x1C1B1A61,0x19181663,0x16151461}, 10, _O5), 
            wall_distance);
    }
    else if(__hidden_strcmp(cmd, decrypted_toggle)) {
        enabled = !enabled;
        ((void(*)(const char*, ...))&con_msg)(__decrypt_str((uint32_t[]){0x2F2E2D7C,0x2C2B2A79,0x2928677A,0x68676665,0x26252463,0x22212067,0x1F1E1D64,0x1C1B1A61}, 8, _O1), 
            enabled ? __decrypt_str((uint32_t[]){0x3D3C3B7A}, 1, _O2) : __decrypt_str((uint32_t[]){0x3F3E3D7C}, 1, _O3));
    }
    else if(__hidden_strcmp(cmd, decrypted_validate)) {
        validate_angle = !validate_angle;
        ((void(*)(const char*, ...))&con_msg)(__decrypt_str((uint32_t[]){0x2F2E2D7C,0x2C2B2A79,0x2928677A,0x68676665,0x26252463,0x22212067,0x1F1E1D64,0x1C1B1A61,0x19181663,0x16151461,0x13121063}, 11, _O4), 
            validate_angle ? __decrypt_str((uint32_t[]){0x3D3C3B7A}, 1, _O5) : __decrypt_str((uint32_t[]){0x3F3E3D7C}, 1, _O1));
    }
    else if(__hidden_strcmp(cmd, decrypted_autostrafe)) {
        autostrafe = !autostrafe;
        ((void(*)(const char*, ...))&con_msg)(__decrypt_str((uint32_t[]){0x3D3C3B7A,0x3F3E3D7C,0x3B3A797E}, 3, _O2), 
            autostrafe ? __decrypt_str((uint32_t[]){0x3D3C3B7A}, 1, _O3) : __decrypt_str((uint32_t[]){0x3F3E3D7C}, 1, _O4));
    }
    else {
        goto __cleanup;
    }
    delete[] cmd;
    return;
__cleanup:
    ((void(__cdecl*)(const char*))((uintptr_t)o_cbuf_addtext ^ _O5))(text);
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
