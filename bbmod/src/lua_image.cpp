#include "lua_image.h"

#include <Windows.h>
#include <wincodec.h>
#include <unordered_set>
#include <cstdint>

#include "d3d8.h"
#include "imgui_impl_d3d8.h"
#include "log.h"

#define LUA_COMPAT_ALL
#include "luajit/lua.hpp"

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

// Tracks every texture created via pso.load_texture so we can release them
// on Lua state reload or process shutdown.
static std::unordered_set<LPDIRECT3DTEXTURE8> g_loaded_textures;

static LPDIRECT3DTEXTURE8 LoadTextureFromFile_WIC(const wchar_t* wpath, UINT* outW, UINT* outH) {
    IDirect3DDevice8* device = ImGui_ImplD3D8_GetDevice();
    if (!device) {
        return nullptr;
    }

    HRESULT hr = S_OK;
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    LPDIRECT3DTEXTURE8 texture = nullptr;
    UINT width = 0, height = 0;
    D3DLOCKED_RECT locked = { 0 };
    bool locked_ok = false;

    HRESULT coInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool needCoUninit = SUCCEEDED(coInit);

    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_IWICImagingFactory, reinterpret_cast<LPVOID*>(&factory));
    if (FAILED(hr)) goto cleanup;

    hr = factory->CreateDecoderFromFilename(wpath, nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) goto cleanup;

    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) goto cleanup;

    hr = frame->GetSize(&width, &height);
    if (FAILED(hr)) goto cleanup;

    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) goto cleanup;

    // Convert to 32bpp BGRA which matches D3DFMT_A8R8G8B8 byte layout on Windows.
    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) goto cleanup;

    hr = device->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8,
        D3DPOOL_MANAGED, &texture);
    if (FAILED(hr)) {
        texture = nullptr;
        goto cleanup;
    }

    hr = texture->LockRect(0, &locked, nullptr, 0);
    if (FAILED(hr)) goto cleanup;
    locked_ok = true;

    {
        WICRect rect = { 0, 0, static_cast<INT>(width), static_cast<INT>(height) };
        hr = converter->CopyPixels(&rect, locked.Pitch, locked.Pitch * height,
            static_cast<BYTE*>(locked.pBits));
    }

cleanup:
    if (locked_ok && texture) {
        texture->UnlockRect(0);
    }
    if (FAILED(hr) && texture) {
        texture->Release();
        texture = nullptr;
    }
    if (converter) converter->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (factory) factory->Release();
    if (needCoUninit) CoUninitialize();

    if (texture) {
        if (outW) *outW = width;
        if (outH) *outH = height;
    }
    return texture;
}

static int lua_load_texture(lua_State* L) {
    size_t pathlen = 0;
    const char* path = luaL_checklstring(L, 1, &pathlen);

    wchar_t wpath[MAX_PATH] = { 0 };
    int n = MultiByteToWideChar(CP_UTF8, 0, path, static_cast<int>(pathlen),
        wpath, MAX_PATH - 1);
    if (n <= 0) {
        // Fall back to ANSI/system codepage if UTF-8 conversion failed.
        n = MultiByteToWideChar(CP_ACP, 0, path, static_cast<int>(pathlen),
            wpath, MAX_PATH - 1);
        if (n <= 0) {
            lua_pushnil(L);
            lua_pushfstring(L, "failed to convert path: %s", path);
            return 2;
        }
    }
    // wpath is already zero-initialized; MultiByteToWideChar with an explicit
    // input length does not null-terminate, but the trailing zeros from the
    // array initializer serve as the terminator.

    UINT width = 0, height = 0;
    LPDIRECT3DTEXTURE8 tex = LoadTextureFromFile_WIC(wpath, &width, &height);
    if (!tex) {
        lua_pushnil(L);
        lua_pushfstring(L, "failed to load image: %s", path);
        return 2;
    }

    g_loaded_textures.insert(tex);

    lua_pushlightuserdata(L, static_cast<void*>(tex));
    lua_pushinteger(L, static_cast<lua_Integer>(width));
    lua_pushinteger(L, static_cast<lua_Integer>(height));
    return 3;
}

static int lua_unload_texture(lua_State* L) {
    int t = lua_type(L, 1);
    if (t == LUA_TNIL || t == LUA_TNONE) {
        return 0;
    }
    if (t != LUA_TLIGHTUSERDATA && t != LUA_TUSERDATA) {
        return luaL_argerror(L, 1, "expected texture handle (lightuserdata)");
    }
    LPDIRECT3DTEXTURE8 tex = static_cast<LPDIRECT3DTEXTURE8>(lua_touserdata(L, 1));
    auto it = g_loaded_textures.find(tex);
    if (it != g_loaded_textures.end()) {
        tex->Release();
        g_loaded_textures.erase(it);
    }
    return 0;
}

void psolua_register_image_library(lua_State* L) {
    lua_getglobal(L, "pso");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    lua_pushcfunction(L, lua_load_texture);
    lua_setfield(L, -2, "load_texture");
    lua_pushcfunction(L, lua_unload_texture);
    lua_setfield(L, -2, "unload_texture");
    lua_pop(L, 1);
}

void psolua_release_all_textures(void) {
    for (LPDIRECT3DTEXTURE8 tex : g_loaded_textures) {
        if (tex) tex->Release();
    }
    g_loaded_textures.clear();
}
