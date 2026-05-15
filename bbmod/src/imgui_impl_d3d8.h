#pragma once

struct IDirect3DDevice8;
struct ImFont;
extern ImFont *g_LoadedFont;
extern bool g_LoadedFontMergeMode;

bool ImGui_ImplD3D8_Init(void* hwnd, IDirect3DDevice8** device);
void ImGui_ImplD3D8_Shutdown(void);
void ImGui_ImplD3D8_NewFrame(void);
void ImGui_BetweenFrameChanges(void);
IDirect3DDevice8* ImGui_ImplD3D8_GetDevice(void);
