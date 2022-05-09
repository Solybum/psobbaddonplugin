#include "lua_psolib.h"

#include <Windows.h>
#include "sol.hpp"

#include "lua_hooks.h"
#include "luastate.h"
#include "log.h"
#include "version.h"
#include "wrap_imgui_impl.h"
#define PSOBB_HWND_PTR (HWND*)(0x00ACBED8 - 0x00400000 + g_PSOBaseAddress)
static int wrap_exceptions(lua_State *L, lua_CFunction f);
static int psolua_print(lua_State *L);
static std::string psolualib_read_cstr(int memory_address, int len = 2048);
static std::string psolualib_read_wstr(int memory_address, int len = 1024);
static sol::table psolualib_read_mem(sol::table t, int memory_address, int len);
static std::string psolualib_read_mem_str(int memory_address, int len = 2048);
static sol::table psolualib_list_addons();
static void psolualib_change_global_font(std::string font_name, float font_size, int oversampleH = 1, int oversampleV = 1,
                                        bool mergeFonts = false, std::string font_name2 = "", float font_size2 = -1);
static sol::table psolualib_list_font_files();
static void psolualib_set_language(std::string lang = "EN");
static std::string psolualib_get_language();
static sol::table psolualib_get_version();
static bool psolualib_require_version(int major, int minor, int patch);

bool psolua_initialize_on_next_frame = false;

DWORD g_PSOBaseAddress;

std::string g_LanguageSetting = "EN";

// Catch C++ exceptions and convert them to Lua error messages.
// Customize as needed for your own exception classes.
static int wrap_exceptions(lua_State *L, lua_CFunction f) {
    try {
        return f(L);  // Call wrapped function and return result.
    }
    catch (const char *s) {  // Catch and convert exceptions.
        lua_pushstring(L, s);
    }
    catch (std::exception& e) {
        lua_pushstring(L, e.what());
    }
    catch (...) {
        lua_pushliteral(L, "caught (...)");
    }
    return lua_error(L);  // Rethrow as a Lua error.
}

static errno_t psolualib_memcpy_s(void* dest, size_t destSize, const void* src, size_t count) {
    // According to MSDN, these cases will cause invalid parameter handler to abort the application
    // See: https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/memcpy-s-wmemcpy-s?view=vs-2019#remarks
    if (count > 0 && (NULL == dest || NULL == src))
        throw "memcpy_s error";
    if (destSize < count)
        throw "memcpy_s error";
    __try { return memcpy_s(dest, destSize, src, count); }
    __except (EXCEPTION_EXECUTE_HANDLER) { throw "memcpy_s error"; }
}

std::string psolualib_error_handler(std::string msg) {
    sol::state_view lua(g_LuaState);

    try {
        g_log << "uncaught error: " << msg << std::endl;
        std::string traceback = lua["debug"]["traceback"]();
        g_log << traceback << std::endl;
        psoluah_UnhandledError(msg);
    }
    catch (...) {
        // do nothing
    }
    return msg;
}

static int psolua_print(lua_State *L) {
    sol::state_view lua(L);
    int nargs = lua_gettop(L);
    std::string coalesce;

    for (int i = 1; i <= nargs; i++) {
        sol::object o = sol::stack::get<sol::object>(lua, i);
        if (i > 1) g_log << "\t";
        if (i > 1) coalesce += "\t";
        std::string out = lua["tostring"](o);
        coalesce += out;
    }
    g_log << coalesce << std::endl;
    psoluah_Log(coalesce);

    return 0;
}

static void psolualib_set_sleep_hack_enabled(bool enabled) {
    uint8_t* location = (uint8_t*)(0x0042C47E + g_PSOBaseAddress);
    if (enabled) {
        location[1] = 0x01;
    }
    else {
        location[1] = 0x00;
    }
}

template <typename T> std::function<T(int)> read_t(void) {
    return [=](int addr) {
        char buf[sizeof(T)];
        if (psolualib_memcpy_s((void*)buf, sizeof(T), (void*)addr, sizeof(T))) {
            throw "memcpy_s error";
        }
        return *(T*)buf;
    };
}

void psolua_store_fpu_state(struct FPUSTATE& fpustate) {
    char* state = fpustate.state;
    __asm {
        mov ebx, state
        fsave [ebx]
    }
}

void psolua_restore_fpu_state(struct FPUSTATE& fpustate) {
    char* state = fpustate.state;
    __asm {
        mov ebx, state
        frstor [ebx]
    }
}

void play_sound(std::string soundPath) {
    PlaySoundA(soundPath.c_str(), NULL, SND_FILENAME | SND_NODEFAULT | SND_ASYNC);
}

bool is_pso_focused() {
    return GetActiveWindow() == *PSOBB_HWND_PTR;
}

void reload_custom_theme() {
   loadCustomTheme();
}

int get_tick_count() {
    return GetTickCount();
}

static std::string get_cwd() {
    wchar_t* buf = _wgetcwd(NULL, 255);
    std::wstring ws(buf);
    free(buf);
    return std::string(ws.begin(), ws.end());
}

void psolua_load_library(lua_State * L) {
    sol::state_view lua(L);

    lua["print"] = psolua_print;
    sol::table psoTable = lua.create_named_table("pso");

    psoTable["error_handler"] = psolualib_error_handler;
    psoTable["reload"] = []() { psolua_initialize_on_next_frame = true; };

    psoTable["read_i8"] = read_t<int8_t>();
    psoTable["read_i16"] = read_t<int16_t>();
    psoTable["read_i32"] = read_t<int32_t>();
    psoTable["read_i64"] = read_t<int64_t>();
    psoTable["read_u8"] = read_t<uint8_t>();
    psoTable["read_u16"] = read_t<uint16_t>();
    psoTable["read_u32"] = read_t<uint32_t>();
    psoTable["read_u64"] = read_t<uint64_t>();
    psoTable["read_f32"] = read_t<float>();
    psoTable["read_f64"] = read_t<double>();
    psoTable["read_cstr"] = psolualib_read_cstr;
    psoTable["read_wstr"] = psolualib_read_wstr;
    psoTable["read_mem"] = psolualib_read_mem;
    psoTable["read_mem_str"] = psolualib_read_mem_str;
    psoTable["set_sleep_hack_enabled"] = psolualib_set_sleep_hack_enabled;
    psoTable["base_address"] = g_PSOBaseAddress;
    psoTable["list_addon_directories"] = psolualib_list_addons;
    psoTable["log_items"] = lua.create_table();
    psoTable["get_cwd"] = get_cwd;
    psoTable["play_sound"] = play_sound;
    psoTable["is_pso_focused"] = is_pso_focused;
    psoTable["reload_custom_theme"] = reload_custom_theme;
    psoTable["get_tick_count"] = get_tick_count;
    psoTable["change_global_font"] = psolualib_change_global_font;
    psoTable["list_font_files"] = psolualib_list_font_files;
    psoTable["set_language"] = psolualib_set_language;
    psoTable["get_language"] = psolualib_get_language;
    psoTable["get_version"] = psolualib_get_version;
    psoTable["require_version"] = psolualib_require_version;
    lua["print"]("PSOBB Base address is ", g_PSOBaseAddress);

    // Exception handling
    lua_pushlightuserdata(L, (void*)wrap_exceptions);
    luaJIT_setmode(L, -1, LUAJIT_MODE_WRAPCFUNC | LUAJIT_MODE_ON);
    lua_pop(L, 1);

    // ImGui library
    luaopen_imgui(L);
}

void psolua_initialize_state(void) {
    if (g_LuaState != nullptr) {
        lua_close(g_LuaState);
        g_LuaState = nullptr;
    }
    g_LuaState = luaL_newstate();

    if (!g_LuaState) {
        MessageBoxA(nullptr, "LuaJit new state failed.", "Lua error", 0);
        exit(1);
    }

    g_lualog.Clear();
    g_log << "Initializing Lua state" << std::endl;

    sol::state_view lua(g_LuaState);

    luaL_openlibs(g_LuaState);
    psolua_load_library(g_LuaState);
    sol::protected_function_result res = lua.do_file("addons/init.lua");
    if (res.status() != sol::call_status::ok) {
        sol::error what = res;
        g_log << (int)res.status() << std::endl;
        g_log << what.what() << std::endl;
        lua["pso"]["error_handler"](what);
        MessageBoxA(nullptr, "Failed to load init.lua", "Lua error", 0);
        exit(1);
    }
    psoluah_Init();

    psolua_initialize_on_next_frame = false;

    loadCustomTheme();
}

static std::string psolualib_read_cstr(int memory_address, int len) {
    char buf[8192] = { 0 };
    if (len >= 8192) {
        len = 8191;
    }
    if (psolualib_memcpy_s((void*)buf, len, (void*)memory_address, len)) {
        throw "memcpy_s error";
    }
    return buf;
}

static char *psolualib_read_wstr_SEH(int memory_address, int len, char *buf, int buf_len)
{
    __try {
        if (!WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)memory_address, len, buf, buf_len, nullptr, nullptr)) {
            throw "invalid utf-16 string";
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        throw "WideCharToMultiByte error";
    }
    return buf;
}

static std::string psolualib_read_wstr(int memory_address, int len) {
    char buf[8192] = { 0 };

    try { psolualib_read_wstr_SEH(memory_address, len, buf, 8192); }
    catch (...) { throw; }

    return buf;
}

static sol::table psolualib_read_mem(sol::table t, int memory_address, int len) {
    sol::state_view lua(g_LuaState);
    unsigned char buf[8192] = { 0 };
    if (len <= 0) {
        throw "length must be greater than 0";
    }
    if (len >= 8192) {
        len = 8191;
    }
    if (psolualib_memcpy_s((void*)buf, len, (void*)memory_address, len)) {
        throw "memcpy_s error";
    }
    for (int i = 0; i < len; i++) {
        t.add((int)buf[i]);
    }
    return t;
}

static std::string psolualib_read_mem_str(int memory_address, int len) {
    char buf[8192] = { 0 };
    if (len >= 8192) {
        len = 8191;
    }
    if (psolualib_memcpy_s((void*)buf, len, (void*)memory_address, len)) {
        throw "memcpy_s error";
    }
    return std::string(buf, len);
}

static sol::table psolualib_list_addons() {
    sol::state_view lua(g_LuaState);

    HANDLE hFind;
    WIN32_FIND_DATA find;

    sol::table ret = lua.create_table();

    hFind = FindFirstFileA("addons/*", &find);
    do {
        std::string filename(find.cFileName);
        if (filename == "..") continue;
        if (filename == ".") continue;
        if (filename == "fonts") continue;
        if (filename == "customdlls") continue;
        if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ret.add(filename);
        }
    } while (FindNextFileA(hFind, &find));

    return ret;
}

wchar_t *themeElements[ImGuiCol_COUNT]
{
    L"Text",
    L"TextDisabled",
    L"WindowBg",
    L"ChildWindowBg",
    L"PopupBg",
    L"Border",
    L"BorderShadow",
    L"FrameBg",
    L"FrameBgHovered",
    L"FrameBgActive",
    L"TitleBg",
    L"TitleBgCollapsed",
    L"TitleBgActive",
    L"MenuBarBg",
    L"ScrollbarBg",
    L"ScrollbarGrab",
    L"ScrollbarGrabHovered",
    L"ScrollbarGrabActive",
    L"ComboBg",
    L"CheckMark",
    L"SliderGrab",
    L"SliderGrabActive",
    L"Button",
    L"ButtonHovered",
    L"ButtonActive",
    L"Header",
    L"HeaderHovered",
    L"HeaderActive",
    L"Column",
    L"ColumnHovered",
    L"ColumnActive",
    L"ResizeGrip",
    L"ResizeGripHovered",
    L"ResizeGripActive",
    L"CloseButton",
    L"CloseButtonHovered",
    L"CloseButtonActive",
    L"PlotLines",
    L"PlotLinesHovered",
    L"PlotHistogram",
    L"PlotHistogramHovered",
    L"TextSelectedBg",
    L"ModalWindowDarkening",
};

void loadCustomTheme()
{
    int ret;
    int i;
    float s = 1.0f / 255.0f;
    wchar_t *themeFile = L"addons\\theme.ini";
    const ImGuiIO default_io;
    const ImGuiStyle default_style;

    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    // GetPrivateProfile* functions require absolute path to the file
    // it will be reading from to be able to read from the application's directory
    wchar_t lpAppName[128] = { 0 };
    wchar_t lpKeyName[128] = { 0 };
    wchar_t lpDefault[128] = { 0 };
    wchar_t lpReturnedString[256] = { 0 };
    wchar_t lpFileName[MAX_PATH] = { 0 };
    wchar_t lpBuffer[MAX_PATH] = { 0 };

    GetCurrentDirectoryW(_countof(lpBuffer), lpBuffer);
    wcscat_s(lpFileName, _countof(lpFileName), lpBuffer);
    wcscat_s(lpFileName, _countof(lpFileName), L"\\");
    wcscat_s(lpFileName, _countof(lpFileName), themeFile);

    wcscpy_s(lpAppName, _countof(lpAppName), L"ImGuiIO");

    // IO Font scale
    swprintf_s(lpKeyName, _countof(lpKeyName), L"FontGlobalScale");
    ret = GetPrivateProfileStringW(lpAppName, lpKeyName, lpDefault, lpReturnedString, _countof(lpReturnedString), lpFileName);
    if (ret != 0)
    {
        io.FontGlobalScale = wcstof(lpReturnedString, NULL);
    }
    else
    {
        io.FontGlobalScale = default_io.FontGlobalScale;
    }

    wcscpy_s(lpAppName, _countof(lpAppName), L"ImGuiStyle");

    // Theme stuff
    swprintf_s(lpKeyName, _countof(lpKeyName), L"Alpha");
    ret = GetPrivateProfileStringW(lpAppName, lpKeyName, lpDefault, lpReturnedString, _countof(lpReturnedString), lpFileName);
    if (ret != 0) {
        style.Alpha = wcstof(lpReturnedString, NULL);
    }
    else
    {
        style.Alpha = default_style.Alpha;
    }

    for (i = 0; i < ImGuiCol_COUNT; i++) {
        swprintf_s(lpKeyName, _countof(lpKeyName), themeElements[i]);
        ret = GetPrivateProfileStringW(lpAppName, lpKeyName, lpDefault, lpReturnedString, _countof(lpReturnedString), lpFileName);
        if (ret != 0) {
            unsigned int color = wcstoul(lpReturnedString, NULL, 16);
            style.Colors[i] = ImVec4(
                ((color >> 16) & 0xFF) * s,
                ((color >> 8) & 0xFF) * s,
                ((color >> 0) & 0xFF) * s,
                ((color >> 24) & 0xFF) * s);
        }
        else
        {
            style.Colors[i] = default_style.Colors[i];
        }
    }
}


// Change global font settings. Must be done between frames but we're in a frame, so we set some globals and it'll be handled later.
void psolualib_change_global_font(std::string font_name, float font_size, int oversampleH, int oversampleV,
                                  bool mergeFonts, std::string font_name2, float font_size2) {
    g_NewFontSpecified = true;
    g_NewFontSize = font_size;
    g_NewFontOversampleH = oversampleH;
    g_NewFontOversampleV = oversampleV;
    sprintf(g_NewFontName, "./addons/fonts/%s", font_name.c_str());
    g_NewFontName[MAX_PATH - 1] = '\0';

    g_MergeFonts = mergeFonts;
    sprintf(g_NewFontName2, "./addons/fonts/%s", font_name2.c_str());
    g_NewFontName2[MAX_PATH - 1] = '\0';
    g_NewFontSize2 = font_size2;
}

// List the files in a directory and return it to the addon
sol::table psolualib_list_font_files() {
    sol::state_view lua(g_LuaState);
    char            fontsPath[MAX_PATH];
    HANDLE          hFind;
    WIN32_FIND_DATA find;
    sol::table      ret = lua.create_table();

    snprintf(fontsPath, MAX_PATH, "./addons/fonts/*");
    fontsPath[MAX_PATH - 1] = '\0';

    hFind = FindFirstFileA(fontsPath, &find);
    do {
        std::string filename(find.cFileName);
        if (filename == "..") continue;
        if (filename == ".") continue;
        if (!(find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            ret.add(filename);
        }
    } while (FindNextFileA(hFind, &find));

    return ret;
}

void psolualib_set_language(std::string lang) {
    g_LanguageSetting = lang;
}

std::string psolualib_get_language() {
    return g_LanguageSetting;
}

// Returns false on error or if version specified by t is lower than current.
// Returns true if the specified version is greater or equal to the plugin's version.
static bool psolualib_require_version(int major, int minor, int patch) {

    if (BBMOD_VERSION_MAJOR < major)
        return false; 
    if (BBMOD_VERSION_MAJOR > major)
        return true;

    // major matches, check minor and patch
    if (BBMOD_VERSION_MINOR < minor)
        return false;
    if (BBMOD_VERSION_MINOR > minor)
        return true;

    // major and minor match, check patch
    if (BBMOD_VERSION_PATCH < patch)
        return false;
    if (BBMOD_VERSION_PATCH >= patch)
        return true;

    // doesn't get here
    return false;
}

// Returns the version inside a table.
static sol::table psolualib_get_version() {
    sol::state_view lua(g_LuaState);

    sol::table ret = lua.create_table();

    ret.set("version_string", std::string(BBMOD_VERSION_STRING));
    ret.set("major", BBMOD_VERSION_MAJOR);
    ret.set("minor", BBMOD_VERSION_MINOR);
    ret.set("patch", BBMOD_VERSION_PATCH);

    return ret;
}
