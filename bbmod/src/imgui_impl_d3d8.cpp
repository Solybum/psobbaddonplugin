#include "imgui_impl_d3d8.h"

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <list>
#include <iterator>

#include "imgui/imgui.h"
#include "d3d8.h"
#include "log.h"
#include "minhook.h"
#include "imgui_d3d8_dev.h"
#include "util.h"
#include "luastate.h"
#include "shlwapi.h"
#include "lua_hooks.h"


static HWND g_hWnd;
//static IDirect3DDevice8* g_device;

static INT64                    g_Time = 0;
static INT64                    g_TicksPerSecond = 0;
static LPDIRECT3DDEVICE8        g_device = NULL;
static LPDIRECT3DVERTEXBUFFER8  g_pVB = NULL;
static LPDIRECT3DINDEXBUFFER8   g_pIB = NULL;
static LPDIRECT3DVERTEXBUFFER8  g_maskVB = NULL;
static LPDIRECT3DINDEXBUFFER8   g_maskIB = NULL;
static LPDIRECT3DTEXTURE8       g_FontTexture = NULL;
static int                      g_VertexBufferSize = 5000, g_IndexBufferSize = 10000;
static IDirect3DSurface8*       g_DepthBuffer = nullptr;

/* Currently loaded font info */
ImFont                         *g_LoadedFont = NULL;
ImFont                         *g_LoadedFont2 = NULL;
float                           g_LoadedFontSize = 16;
float                           g_LoadedFontSize2 = 16;
char                            g_LoadedFontName[MAX_PATH] = { 0 };
char                            g_LoadedFontName2[MAX_PATH] = { 0 };
int                             g_LoadedFontOversampleH = 1;
int                             g_LoadedFontOversampleV = 1;
bool                            g_LoadedFontMergeMode = false;

/* Desired new font */
bool                            g_NewFontSpecified = false;
float                           g_NewFontSize = 16;
char                            g_NewFontName[MAX_PATH] = { 0 };
int                             g_NewFontOversampleH = 1;
int                             g_NewFontOversampleV = 1;
bool                            g_MergeFonts;
char                            g_NewFontName2[MAX_PATH] = { 0 };
float                           g_NewFontSize2 = 16;

struct CUSTOMVERTEX
{
    float    pos[3];
    D3DCOLOR col;
    float    uv[2];
};

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)

typedef LRESULT(WINAPI *TFNWndProc)(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static TFNWndProc oldWinProc;

// Get the window size from the values in the code passed to CreateWindowExA()
LONG psobb_resolution_x() { return **(int **)0x82D140; }
LONG psobb_resolution_y() { return **(int **)0x82D18A; }

// Convert the WM_MOUSEMOVE parameter coordinates from Windows screen coordinates (relative to the view) to ImGui coordinates.
static void ImGui_ImplD3D8_ConvertMouseCoordsToImGui(signed short xMouse, signed short yMouse)
{
    ImGuiIO& io = ImGui::GetIO();
    RECT rect;
    GetClientRect(g_hWnd, &rect);
    double xView = (double)(rect.right - rect.left);
    double yView = (double)(rect.bottom - rect.top);
    double xRes = (double)psobb_resolution_x();
    double yRes = (double)psobb_resolution_y();
    if (xView > 0 && yView > 0)
    {
        double xMouseScaled = (double)xMouse * (xRes / xView);
        double yMouseScaled = (double)yMouse * (yRes / yView);
        io.MousePos.x = (float)xMouseScaled;
        io.MousePos.y = (float)yMouseScaled;
    }
    else
    {
        io.MousePos.x = xMouse;
        io.MousePos.y = yMouse;
    }
}

IMGUI_API LRESULT ImGui_ImplD3D8_WndProcHandler(HWND, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ImGuiIO& io = ImGui::GetIO();
    switch (msg)
    {
    case WM_LBUTTONDOWN:
        io.MouseDown[0] = true;
        if (io.WantCaptureMouse) return true;
        break;
    case WM_LBUTTONUP:
        io.MouseDown[0] = false;
        if (io.WantCaptureMouse) return true;
        break;
    case WM_RBUTTONDOWN:
        io.MouseDown[1] = true;
        if (io.WantCaptureMouse) return true;
        break;
    case WM_RBUTTONUP:
        io.MouseDown[1] = false;
        if (io.WantCaptureMouse) return true;
        break;
    case WM_MBUTTONDOWN:
        io.MouseDown[2] = true;
        if (io.WantCaptureMouse) return true;
        break;
    case WM_MBUTTONUP:
        io.MouseDown[2] = false;
        if (io.WantCaptureMouse) return true;
        break;
    case WM_MOUSEWHEEL:
        io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? +1.0f : -1.0f;
        if (io.WantCaptureMouse) return true;
        break;
    case WM_MOUSEMOVE:
        ImGui_ImplD3D8_ConvertMouseCoordsToImGui((signed short)(lParam), (signed short)(lParam >> 16));
        if (io.WantCaptureMouse) return true;
        break;
    case WM_KEYDOWN:
        psolua_push_key_pressed(wParam);
        if (wParam < 256)
            io.KeysDown[wParam] = 1;
        if (io.WantCaptureKeyboard) return true;
        break;
    case WM_KEYUP:
        psolua_push_key_released(wParam);
        if (wParam < 256)
            io.KeysDown[wParam] = 0;
        if (io.WantCaptureKeyboard) return true;
        break;
    case WM_CHAR:
        // You can also use ToAscii()+GetKeyboardState() to retrieve characters.
        if (wParam > 0 && wParam < 0x10000)
            io.AddInputCharacter((unsigned short)wParam);
        if (io.WantTextInput) return true;
        break;
    }
    return 0;
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplD3D8_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    if (oldWinProc != nullptr) {
        return oldWinProc(hWnd, msg, wParam, lParam);
    } else {
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

static void build_mask_vbuffer(const RECT* rect) {
    CUSTOMVERTEX* vtx_dst;
    g_maskVB->Lock(0, (UINT)(6 * sizeof(CUSTOMVERTEX)), (BYTE**)&vtx_dst, 0);
    vtx_dst[0].pos[0] = (float)rect->left;
    vtx_dst[0].pos[1] = (float)rect->bottom;
    vtx_dst[0].pos[2] = 0;
    vtx_dst[1].pos[0] = (float)rect->left;
    vtx_dst[1].pos[1] = (float)rect->top;
    vtx_dst[1].pos[2] = 0;
    vtx_dst[2].pos[0] = (float)rect->right;
    vtx_dst[2].pos[1] = (float)rect->top;
    vtx_dst[2].pos[2] = 0;
    vtx_dst[3].pos[0] = (float)rect->left;
    vtx_dst[3].pos[1] = (float)rect->bottom;
    vtx_dst[3].pos[2] = 0;
    vtx_dst[4].pos[0] = (float)rect->right;
    vtx_dst[4].pos[1] = (float)rect->top;
    vtx_dst[4].pos[2] = 0;
    vtx_dst[5].pos[0] = (float)rect->right;
    vtx_dst[5].pos[1] = (float)rect->bottom;
    vtx_dst[5].pos[2] = 0;
    vtx_dst[0].col = 0xFFFFFFFF;
    vtx_dst[1].col = 0xFFFFFFFF;
    vtx_dst[2].col = 0xFFFFFFFF;
    vtx_dst[3].col = 0xFFFFFFFF;
    vtx_dst[4].col = 0xFFFFFFFF;
    vtx_dst[5].col = 0xFFFFFFFF;
    vtx_dst[0].uv[0] = 0;
    vtx_dst[0].uv[1] = 0;
    vtx_dst[1].uv[0] = 0;
    vtx_dst[1].uv[1] = 0;
    vtx_dst[2].uv[0] = 0;
    vtx_dst[2].uv[1] = 0;
    vtx_dst[3].uv[0] = 0;
    vtx_dst[3].uv[1] = 0;
    vtx_dst[4].uv[0] = 0;
    vtx_dst[4].uv[1] = 0;
    vtx_dst[5].uv[0] = 0;
    vtx_dst[5].uv[1] = 0;
    g_maskVB->Unlock();
}

void ImGui_ImplD3D8_RenderDrawLists(ImDrawData* draw_data)
{
    IDirect3DSurface8* realDepthStencilBuffer;

    // Avoid rendering when minimized
    ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f)
        return;

    // Create and grow buffers if needed
    if (!g_pVB || g_VertexBufferSize < draw_data->TotalVtxCount)
    {
        if (g_pVB) { g_pVB->Release(); g_pVB = NULL; }
        g_VertexBufferSize = draw_data->TotalVtxCount + 5000;
        if (g_device->CreateVertexBuffer(g_VertexBufferSize * sizeof(CUSTOMVERTEX), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &g_pVB) < 0)
            return;
    }
    if (!g_pIB || g_IndexBufferSize < draw_data->TotalIdxCount)
    {
        if (g_pIB) { g_pIB->Release(); g_pIB = NULL; }
        g_IndexBufferSize = draw_data->TotalIdxCount + 10000;
        if (g_device->CreateIndexBuffer(g_IndexBufferSize * sizeof(ImDrawIdx), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, sizeof(ImDrawIdx) == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32, D3DPOOL_DEFAULT, &g_pIB) < 0)
            return;
    }
    if (!g_maskVB && !g_maskIB) {
        if (g_device->CreateVertexBuffer(6 * sizeof(CUSTOMVERTEX), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &g_maskVB) < 0) return;
        if (g_device->CreateIndexBuffer(6, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, sizeof(ImDrawIdx) == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32, D3DPOOL_DEFAULT, &g_maskIB) < 0) return;
        ImDrawIdx* idx_dst;
        g_maskIB->Lock(0, 6 * sizeof(ImDrawIdx), (BYTE**)&idx_dst, D3DLOCK_DISCARD);
        idx_dst[0] = 0;
        idx_dst[1] = 1;
        idx_dst[2] = 2;
        idx_dst[3] = 0;
        idx_dst[4] = 2;
        idx_dst[5] = 3;
        g_maskIB->Unlock();
    }

    // Backup the DX9 state
    DWORD stateBlockToken = 0;
    if (g_device->CreateStateBlock(D3DSBT_ALL, &stateBlockToken) < 0) return;
    /*IDirect3DStateBlock9* d3d9_state_block = NULL;
    if (g_pd3dDevice->CreateStateBlock(D3DSBT_ALL, &d3d9_state_block) < 0)
        return;*/


    // Copy and convert all vertices into a single contiguous buffer
    CUSTOMVERTEX* vtx_dst;
    ImDrawIdx* idx_dst;
    if (g_pVB->Lock(0, (UINT)(draw_data->TotalVtxCount * sizeof(CUSTOMVERTEX)), (BYTE**)&vtx_dst, D3DLOCK_DISCARD) < 0)
        return;
    if (g_pIB->Lock(0, (UINT)(draw_data->TotalIdxCount * sizeof(ImDrawIdx)), (BYTE**)&idx_dst, D3DLOCK_DISCARD) < 0)
        return;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const ImDrawVert* vtx_src = cmd_list->VtxBuffer.Data;
        for (int i = 0; i < cmd_list->VtxBuffer.Size; i++)
        {
            vtx_dst->pos[0] = vtx_src->pos.x;
            vtx_dst->pos[1] = vtx_src->pos.y;
            vtx_dst->pos[2] = 0.0f;
            vtx_dst->col = (vtx_src->col & 0xFF00FF00) | ((vtx_src->col & 0xFF0000) >> 16) | ((vtx_src->col & 0xFF) << 16);     // RGBA --> ARGB for DirectX9
            vtx_dst->uv[0] = vtx_src->uv.x;
            vtx_dst->uv[1] = vtx_src->uv.y;
            vtx_dst++;
            vtx_src++;
        }
        memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        idx_dst += cmd_list->IdxBuffer.Size;
    }
    g_pVB->Unlock();
    g_pIB->Unlock();
    g_device->SetStreamSource(0, g_pVB, sizeof(CUSTOMVERTEX));
    g_device->SetIndices(g_pIB, 0);
    //g_pd3dDevice->SetFVF(D3DFVF_CUSTOMVERTEX);

    // Setup render state: fixed-pipeline, alpha-blending, no face culling, no depth testing
    g_device->GetDepthStencilSurface(&realDepthStencilBuffer);
    g_device->SetRenderTarget(nullptr, g_DepthBuffer);
    //g_device->SetRenderTarget(nullptr, realDepthStencilBuffer);
    g_device->SetPixelShader(NULL);
    g_device->SetVertexShader(D3DFVF_CUSTOMVERTEX);
    g_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_device->SetRenderState(D3DRS_LIGHTING, false);
    g_device->SetRenderState(D3DRS_ZENABLE, false);
    g_device->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
    g_device->SetRenderState(D3DRS_ALPHATESTENABLE, false);
    g_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    g_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    //g_device->SetRenderState(D3DRS_SCISSORTESTENABLE, true);

    g_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    g_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    g_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    g_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    //g_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    //g_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    // Setup orthographic projection matrix
    // Being agnostic of whether <d3dx9.h> or <DirectXMath.h> can be used, we aren't relying on D3DXMatrixIdentity()/D3DXMatrixOrthoOffCenterLH() or DirectX::XMMatrixIdentity()/DirectX::XMMatrixOrthographicOffCenterLH()
    {
        const float L = 0.5f, R = io.DisplaySize.x + 0.5f, T = 0.5f, B = io.DisplaySize.y + 0.5f;
        D3DMATRIX mat_identity = { { 1.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 0.0f, 1.0f } };
        D3DMATRIX mat_projection =
        {
            2.0f / (R - L),   0.0f,         0.0f,  0.0f,
            0.0f,         2.0f / (T - B),   0.0f,  0.0f,
            0.0f,         0.0f,         0.5f,  0.0f,
            (L + R) / (L - R),  (T + B) / (B - T),  0.5f,  1.0f,
        };
        g_device->SetTransform(D3DTS_WORLD, &mat_identity);
        g_device->SetTransform(D3DTS_VIEW, &mat_identity);
        g_device->SetTransform(D3DTS_PROJECTION, &mat_projection);
    }

    // Render command lists
    int vtx_offset = 0;
    int idx_offset = 0;
    //static int frame = 0;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            {
                pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                //int dbcount = 0;
                const RECT r = { (LONG)pcmd->ClipRect.x, (LONG)pcmd->ClipRect.y, (LONG)pcmd->ClipRect.z, (LONG)pcmd->ClipRect.w };
                build_mask_vbuffer(&r);
                g_device->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
                g_device->SetRenderState(D3DRS_ZENABLE, true);
                g_device->SetRenderState(D3DRS_STENCILENABLE, true);
                g_device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFF);
                g_device->SetRenderState(D3DRS_STENCILMASK, 0xFF);
                g_device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
                g_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
                g_device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
                g_device->SetRenderState(D3DRS_STENCILREF, 0xFF);
                g_device->Clear(0, NULL, D3DCLEAR_STENCIL, 0, 1.0f, 0);
                g_device->SetStreamSource(0, g_maskVB, sizeof(CUSTOMVERTEX));
                g_device->SetIndices(g_maskIB, 0);
                //g_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 4, 0, 2);
                g_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                g_device->SetStreamSource(0, g_pVB, sizeof(CUSTOMVERTEX));
                g_device->SetIndices(g_pIB, vtx_offset);
                g_device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
                g_device->SetRenderState(D3DRS_ZENABLE, false);
                g_device->SetRenderState(D3DRS_STENCILENABLE, true);
                g_device->SetRenderState(D3DRS_STENCILWRITEMASK, 0);
                g_device->SetRenderState(D3DRS_STENCILMASK, 0xFF);
                g_device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
                g_device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
                g_device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
                g_device->SetRenderState(D3DRS_STENCILREF, 0xFF);
                g_device->SetTexture(0, (LPDIRECT3DTEXTURE8)pcmd->TextureId);
                //g_device->SetScissorRect(&r);
                g_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, (UINT)cmd_list->VtxBuffer.Size, idx_offset, pcmd->ElemCount / 3);
            }
            idx_offset += pcmd->ElemCount;
        }
        vtx_offset += cmd_list->VtxBuffer.Size;
    }

    // Restore the DX9 state
    //d3d9_state_block->Apply();
    //d3d9_state_block->Release();
    g_device->SetRenderTarget(nullptr, realDepthStencilBuffer);
    g_device->ApplyStateBlock(stateBlockToken);
    g_device->DeleteStateBlock(stateBlockToken);
}

static bool ImGui_ImplDX9_CreateFontsTexture()
{
    // Build texture atlas
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height, bytes_per_pixel;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytes_per_pixel);

    // Upload texture to graphics system
    g_FontTexture = NULL;
    if (g_device->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &g_FontTexture) < 0)
        return false;
    D3DLOCKED_RECT tex_locked_rect;
    if (g_FontTexture->LockRect(0, &tex_locked_rect, NULL, 0) != D3D_OK)
        return false;
    for (int y = 0; y < height; y++)
        memcpy((unsigned char *)tex_locked_rect.pBits + tex_locked_rect.Pitch * y, pixels + (width * bytes_per_pixel) * y, (width * bytes_per_pixel));
    g_FontTexture->UnlockRect(0);

    // Store our identifier
    io.Fonts->TexID = (void *)g_FontTexture;

    return true;
}

static bool ImGui_ImplD3D8_CreateDepthStencilBuffer() {
    if (g_device == nullptr) {
        return false;
    }
    if (g_DepthBuffer == nullptr) {
        IDirect3DSurface8* realDepth;
        D3DSURFACE_DESC sfcDesc;

        g_device->GetDepthStencilSurface(&realDepth);
        if (realDepth->GetDesc(&sfcDesc)) {
            return false;
        }
        
        if (g_device->CreateDepthStencilSurface(sfcDesc.Width, sfcDesc.Height, D3DFMT_D24S8, sfcDesc.MultiSampleType, &g_DepthBuffer)) {
            return false;
        }
    }

    return true;
}

bool ImGui_ImplDX9_CreateDeviceObjects()
{
    if (!g_device) {
        g_log << "ImGui_ImplD3D8_CreateDeviceObjects: Device is null" << std::endl;
        return false;
    }
    if (!ImGui_ImplDX9_CreateFontsTexture()) {
        g_log << "ImGui_ImplD3D8_CreateDeviceObjects: Unable to create fonts texture" << std::endl;
        return false;
    }
    if (!ImGui_ImplD3D8_CreateDepthStencilBuffer()) {
        g_log << "ImGui_ImplD3D8_CreateDeviceObjects: Unable to create depth buffer" << std::endl;
        return false;
    }
    return true;
}

void ImGui_ImplDX9_InvalidateDeviceObjects()
{
    if (!g_device)
        return;
    if (g_pVB)
    {
        g_pVB->Release();
        g_pVB = NULL;
    }
    if (g_pIB)
    {
        g_pIB->Release();
        g_pIB = NULL;
    }
    if (g_maskVB) {
        g_maskVB->Release();
        g_maskVB = nullptr;
    }
    if (g_maskIB) {
        g_maskIB->Release();
        g_maskIB = nullptr;
    }
    if (g_DepthBuffer) {
        g_DepthBuffer->Release();
        g_DepthBuffer = nullptr;
    }
    if (LPDIRECT3DTEXTURE8 tex = (LPDIRECT3DTEXTURE8)ImGui::GetIO().Fonts->TexID)
    {
        tex->Release();
        ImGui::GetIO().Fonts->TexID = 0;
    }
    g_FontTexture = NULL;
}

bool ImGui_ImplD3D8_Init(void* hwnd, IDirect3DDevice8** device) {
    g_hWnd = (HWND)hwnd;
    
    oldWinProc = (TFNWndProc) GetWindowLong(g_hWnd, GWL_WNDPROC);
    SetWindowLong(g_hWnd, GWL_WNDPROC, (LONG)WndProc);

    if (!QueryPerformanceFrequency((LARGE_INTEGER *)&g_TicksPerSecond))
        return false;
    if (!QueryPerformanceCounter((LARGE_INTEGER *)&g_Time))
        return false;

    *device = (IDirect3DDevice8*)new ImguiD3D8Device(*device);
    g_device = *device;

    ImGuiIO& io = ImGui::GetIO();

    io.KeyMap[ImGuiKey_Tab] = VK_TAB;                       // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array that we will update during the application lifetime.
    io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
    io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
    io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
    io.KeyMap[ImGuiKey_Home] = VK_HOME;
    io.KeyMap[ImGuiKey_End] = VK_END;
    io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
    io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
    io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
    io.KeyMap[ImGuiKey_A] = 'A';
    io.KeyMap[ImGuiKey_C] = 'C';
    io.KeyMap[ImGuiKey_V] = 'V';
    io.KeyMap[ImGuiKey_X] = 'X';
    io.KeyMap[ImGuiKey_Y] = 'Y';
    io.KeyMap[ImGuiKey_Z] = 'Z';

    io.RenderDrawListsFn = ImGui_ImplD3D8_RenderDrawLists;
    io.ImeWindowHandle = g_hWnd;

    // Add default font
    io.Fonts->AddFontDefault();

    return true;
}

void ImGui_ImplD3D8_Shutdown(void) {

}

void ImGui_ImplD3D8_NewFrame(void) {
    if (!g_FontTexture || !g_DepthBuffer)
        ImGui_ImplDX9_CreateDeviceObjects();

    ImGuiIO& io = ImGui::GetIO();
    
    // Set displaySize to the rendering size. Suppose you're running 1024x768
    // on a monitor currently set to 1920x1080, and you're running fullscreen.
    // We want imgui to use 1024x768 for the DisplaySize and scale the UI up.
    // If we were to render with 1920x1080, then the image would be scaled down
    // and lose pixels, which makes text and more elements unreadable.
    io.DisplaySize.x = (float)psobb_resolution_x();
    io.DisplaySize.y = (float)psobb_resolution_y();

    // Setup time step
    INT64 current_time;
    QueryPerformanceCounter((LARGE_INTEGER *)&current_time);
    io.DeltaTime = (float)(current_time - g_Time) / g_TicksPerSecond;
    g_Time = current_time;

    // Read keyboard modifiers inputs
    io.KeyCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    io.KeyShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    io.KeyAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    io.KeySuper = false;
    // io.KeysDown : filled by WM_KEYDOWN/WM_KEYUP events
    // io.MousePos : filled by WM_MOUSEMOVE events
    // io.MouseDown : filled by WM_*BUTTON* events
    // io.MouseWheel : filled by WM_MOUSEWHEEL events

    // Hide OS mouse cursor if ImGui is drawing it
    if (io.MouseDrawCursor)
        SetCursor(NULL);

    // Start the frame
    ImGui::NewFrame();
}

// Change the font if necessary... Really need to clean up how the arguments are passed through globals
void ImGui_BetweenFrameChanges(void) {
    ImGuiIO& io = ImGui::GetIO();
    bool fontLoadFailed = false;

    if (g_NewFontSpecified) {
        // Clear flag so we try this once until user makes another change
        g_NewFontSpecified = false;

        // Check if something changed. Font names, font sizes, oversampling, or merge mode
        if (strncmp(g_NewFontName, g_LoadedFontName, MAX_PATH) ||
            strncmp(g_NewFontName2, g_LoadedFontName2, MAX_PATH) ||
            abs(g_LoadedFontSize - g_NewFontSize) > 0.01 ||
            abs(g_LoadedFontSize2 - g_NewFontSize2) > 0.01 || 
            g_NewFontOversampleH != g_LoadedFontOversampleH ||
            g_NewFontOversampleV != g_LoadedFontOversampleV ||
            g_LoadedFontMergeMode != g_MergeFonts) {

            // Something changed

            ImFontConfig font_cfg = ImFontConfig();
            font_cfg.SizePixels = g_NewFontSize;
            font_cfg.MergeMode = false;
            font_cfg.OversampleH = g_NewFontOversampleH;
            font_cfg.OversampleV = g_NewFontOversampleV;

            // Free up memory and clear the font atlas
            if (LPDIRECT3DTEXTURE8 tex = (LPDIRECT3DTEXTURE8)ImGui::GetIO().Fonts->TexID) {
                tex->Release();
                ImGui::GetIO().Fonts->TexID = 0;
            }
            io.Fonts->Clear();

            // Sanity check that the file exists, of course this could fail right after but
            // we should try to prevent some failures.
            std::fstream file_test;
            file_test.open(g_NewFontName);
            if (file_test.is_open()) {
                file_test.close();

                ImFont *ptmp;
                // Load the font, if we are merging fonts then load only default glyph range
                ptmp = io.Fonts->AddFontFromFileTTF(g_NewFontName, g_NewFontSize, &font_cfg,
                                                    g_MergeFonts ? io.Fonts->GetGlyphRangesDefault() : io.Fonts->GetGlyphRangesChinese());
                if (ptmp) {
                    // Font was loaded successfully, save the info about it.
                    g_LoadedFont = ptmp;
                    g_LoadedFontSize = g_NewFontSize;
                    strncpy(g_LoadedFontName, g_NewFontName, MAX_PATH);
                    g_LoadedFontOversampleH = g_NewFontOversampleH;
                    g_LoadedFontOversampleV = g_NewFontOversampleV;

                    if (g_MergeFonts) {
                        // Now we try to load the merged font if specified
                        // But first we do another sanity check to prevent imgui from failing
                        file_test.open(g_NewFontName2);
                        if (file_test.is_open()) {
                            file_test.close();

                            // Merge this font into the previously loaded font
                            font_cfg.SizePixels = g_NewFontSize2;
                            font_cfg.MergeMode = true;

                            // Load this secondary font for the Chinese glyph range. We do not care
                            // too much if this fails because we successfully loaded the primary font.
                            ptmp = io.Fonts->AddFontFromFileTTF(g_NewFontName2, g_NewFontSize2, &font_cfg, io.Fonts->GetGlyphRangesChinese());
                            if (ptmp) {
                                // Loaded successfully
                                g_LoadedFont2 = ptmp;
                                g_LoadedFontSize2 = g_NewFontSize2;
                                strncpy(g_LoadedFontName2, g_NewFontName2, MAX_PATH);
                                g_LoadedFontMergeMode = true;
                            }
                            else {
                                std::string errorMessage = "Failed to load the secondary font: " + std::string(g_NewFontName2) + "\n";
                                psoluah_Log(errorMessage);
                            }
                        }
                        else {
                            // Failed to load the merged font. Not a big problem.
                            g_LoadedFontMergeMode = false;
                            snprintf(g_LoadedFontName2, MAX_PATH, "");
                            g_LoadedFontSize2 = 13;
                            g_LoadedFont2 = NULL;

                            std::string errorMessage = "Invalid secondary font file " + std::string(g_NewFontName2) +
                                                       " specified.\n";
                            psoluah_Log(errorMessage);
                        }
                    }
                    else {
                        g_LoadedFontMergeMode = false;
                        snprintf(g_LoadedFontName2, MAX_PATH, "");
                        g_LoadedFontSize2 = 13;
                        g_LoadedFont2 = NULL;
                    }

                    ImGui_ImplDX9_CreateFontsTexture();
                }
                else {
                    std::string errorMessage = "Failed to load the primary font " + std::string(g_NewFontName) +
                                               ". Restoring default font.\n";
                    psoluah_Log(errorMessage);

                    // Failed to load the primary font. Remember this so that we restore
                    // the default font.
                    fontLoadFailed = true;
                }
            }
            else {
                std::string errorMessage = "Failed to open the primary font file " + std::string(g_NewFontName) +
                                           ". Does this file exist on disk? Restoring default font.\n";
                psoluah_Log(errorMessage);

                // The font file cannot even be opened. Somehow the Settings Editor
                // provided a bad file name?
                fontLoadFailed = true;
            }
        }
    }

    if (fontLoadFailed) {
        // Failed to load, we also cleared the current font above and we may be able to
        // restore it, but we'll leave it up to addon to provide correct file.
        snprintf(g_LoadedFontName, MAX_PATH, "");
        g_LoadedFontSize = 13;
        g_LoadedFont = NULL;

        // Setup the default font so that we can keep the client running with readable text.
        io.Fonts->AddFontDefault();
        ImGui_ImplDX9_CreateFontsTexture();
    }
}
