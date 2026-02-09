-- Start LIP
--[[
    Copyright (c) 2012 Carreras Nicolas

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the Software), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED AS IS, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
--]]
--- Lua INI Parser.
-- It has never been that simple to use INI files with Lua.
--@author Dynodzzo

local LIP = {};

--- Returns a table containing all the data from the INI file.
--@param fileName The name of the INI file to parse. [string]
--@return The table containing all data from the INI file. [table]
function LIP.load(fileName)
    assert(type(fileName) == 'string', 'Parameter fileName must be a string.');
    local file = assert(io.open(fileName, 'r'), 'Error loading file : ' .. fileName);
    local data = {};
    local section;
    for line in file:lines() do
        local tempSection = line:match('^%[([^%[%]]+)%]$');
        if(tempSection)then
            section = tonumber(tempSection) and tonumber(tempSection) or tempSection;
            data[section] = data[section] or {};
        end
        local param, value = line:match('^([%w|_]+)%s-=%s-(.+)$');
        if(param and value ~= nil)then
            data[section][param] = value;
        end
    end
    file:close();
    return data;
end

--- Saves all the data from a table to an INI file.
--@param fileName The name of the INI file to fill. [string]
--@param data The table containing all the data to store. [table]
function LIP.save(fileName, data)
    assert(type(fileName) == 'string', 'Parameter fileName must be a string.');
    assert(type(data) == 'table', 'Parameter data must be a table.');
    local file = assert(io.open(fileName, 'w+b'), 'Error loading file :' .. fileName);
    local contents = '';
    for section, param in pairs(data) do
        contents = contents .. ('[%s]\n'):format(section);
        for key, value in pairs(param) do
            contents = contents .. ('%s=%s\n'):format(key, tostring(value));
        end
        contents = contents .. '\n';
    end
    file:write(contents);
    file:close();
end
-- End LIP

local fontGlobalScale = 1.0
local colorList = {
    { name = "Text"                   , color = "FFE5E5E5" },
    { name = "TextDisabled"           , color = "FF999999" },
    { name = "WindowBg"               , color = "B2000000" },
    { name = "ChildWindowBg"          , color = "00000000" },
    { name = "PopupBg"                , color = "E50C0C19" },
    { name = "Border"                 , color = "A5B2B2B2" },
    { name = "BorderShadow"           , color = "00000000" },
    { name = "FrameBg"                , color = "4CCCCCCC" },
    { name = "FrameBgHovered"         , color = "66E5CCCC" },
    { name = "FrameBgActive"          , color = "72E5A5A5" },
    { name = "TitleBg"                , color = "D3444489" },
    { name = "TitleBgCollapsed"       , color = "336666CC" },
    { name = "TitleBgActive"          , color = "DD5151A0" },
    { name = "MenuBarBg"              , color = "CC66668C" },
    { name = "ScrollbarBg"            , color = "99333F4C" },
    { name = "ScrollbarGrab"          , color = "4C6666CC" },
    { name = "ScrollbarGrabHovered"   , color = "666666CC" },
    { name = "ScrollbarGrabActive"    , color = "66CC7F7F" },
    { name = "ComboBg"                , color = "FC333333" },
    { name = "CheckMark"              , color = "7FE5E5E5" },
    { name = "SliderGrab"             , color = "4CFFFFFF" },
    { name = "SliderGrabActive"       , color = "FFCC7F7F" },
    { name = "Button"                 , color = "99AA6666" },
    { name = "ButtonHovered"          , color = "FFAA6666" },
    { name = "ButtonActive"           , color = "FFCC7F7F" },
    { name = "Header"                 , color = "726666E5" },
    { name = "HeaderHovered"          , color = "CC7272E5" },
    { name = "HeaderActive"           , color = "CC8787DD" },
    { name = "Column"                 , color = "FF7F7F7F" },
    { name = "ColumnHovered"          , color = "FFB29999" },
    { name = "ColumnActive"           , color = "FFE5B2B2" },
    { name = "ResizeGrip"             , color = "4CFFFFFF" },
    { name = "ResizeGripHovered"      , color = "99FFFFFF" },
    { name = "ResizeGripActive"       , color = "E5FFFFFF" },
    { name = "CloseButton"            , color = "7F7F7FE5" },
    { name = "CloseButtonHovered"     , color = "99B2B2E5" },
    { name = "CloseButtonActive"      , color = "FFB2B2B2" },
    { name = "PlotLines"              , color = "FFFFFFFF" },
    { name = "PlotLinesHovered"       , color = "FFE5B200" },
    { name = "PlotHistogram"          , color = "FFE5B200" },
    { name = "PlotHistogramHovered"   , color = "FFFF9900" },
    { name = "TextSelectedBg"         , color = "590000FF" },
    { name = "ModalWindowDarkening"   , color = "59333333" },
}

local core_mainmenu = require("core_mainmenu")

local enable = false
local themFileName = "addons/theme.ini"
local theme =
{
    ImGuiIO = {
        FontGlobalScale = 1.0,
    },
    ImGuiStyle = {
        Alpha                = 1.0,
        Text                 = "FFE5E5E5",
        TextDisabled         = "FF999999",
        WindowBg             = "B2000000",
        ChildWindowBg        = "00000000",
        PopupBg              = "E50C0C19",
        Border               = "A5B2B2B2",
        BorderShadow         = "00000000",
        FrameBg              = "4CCCCCCC",
        FrameBgHovered       = "66E5CCCC",
        FrameBgActive        = "72E5A5A5",
        TitleBg              = "D3444489",
        TitleBgCollapsed     = "336666CC",
        TitleBgActive        = "DD5151A0",
        MenuBarBg            = "CC66668C",
        ScrollbarBg          = "99333F4C",
        ScrollbarGrab        = "4C6666CC",
        ScrollbarGrabHovered = "666666CC",
        ScrollbarGrabActive  = "66CC7F7F",
        ComboBg              = "FC333333",
        CheckMark            = "7FE5E5E5",
        SliderGrab           = "4CFFFFFF",
        SliderGrabActive     = "FFCC7F7F",
        Button               = "99AA6666",
        ButtonHovered        = "FFAA6666",
        ButtonActive         = "FFCC7F7F",
        Header               = "726666E5",
        HeaderHovered        = "CC7272E5",
        HeaderActive         = "CC8787DD",
        Column               = "FF7F7F7F",
        ColumnHovered        = "FFB29999",
        ColumnActive         = "FFE5B2B2",
        ResizeGrip           = "4CFFFFFF",
        ResizeGripHovered    = "99FFFFFF",
        ResizeGripActive     = "E5FFFFFF",
        CloseButton          = "7F7F7FE5",
        CloseButtonHovered   = "99B2B2E5",
        CloseButtonActive    = "FFB2B2B2",
        PlotLines            = "FFFFFFFF",
        PlotLinesHovered     = "FFE5B200",
        PlotHistogram        = "FFE5B200",
        PlotHistogramHovered = "FFFF9900",
        TextSelectedBg       = "590000FF",
        ModalWindowDarkening = "59333333",
    }
}

function file_exists(name)
    local f=io.open(name,"r")
    if f~=nil then io.close(f) return true else return false end
 end

local function ParseTheme()
    if file_exists(themFileName) then
        local savedTheme = LIP.load(themFileName);

        if savedTheme.ImGuiStyle == nil or savedTheme.ImGuiIO == nil then
            return;
        end

        theme = savedTheme
    end
end

local function ExportTheme()
    LIP.save(themFileName, theme)
    pso.reload_custom_theme()
end

-- UI stuff
local function PresentColorEditor(label, default, custom)
    custom = custom or 0xFFFFFFFF

    local changed = false
    local i_default =
    {
        bit.band(bit.rshift(default, 24), 0xFF),
        bit.band(bit.rshift(default, 16), 0xFF),
        bit.band(bit.rshift(default, 8), 0xFF),
        bit.band(default, 0xFF)
    }
    local i_custom =
    {
        bit.band(bit.rshift(custom, 24), 0xFF),
        bit.band(bit.rshift(custom, 16), 0xFF),
        bit.band(bit.rshift(custom, 8), 0xFF),
        bit.band(custom, 0xFF)
    }

    local ids = { "##X", "##Y", "##Z", "##W" }
    local fmt = { "A:%3.0f", "R:%3.0f", "G:%3.0f", "B:%3.0f" }

    imgui.BeginGroup()
    imgui.PushID(label)

    imgui.PushItemWidth(50)
    for n = 1, 4, 1 do
        local changedDragInt = false
        if n ~= 1 then
            imgui.SameLine(0, 5)
        end

        changedDragInt, i_custom[n] = imgui.DragInt(ids[n], i_custom[n], 1.0, 0, 255, fmt[n])
    end
    imgui.PopItemWidth()

    imgui.SameLine(0, 5)
    imgui.ColorButton(i_custom[2] / 255, i_custom[3] / 255, i_custom[4] / 255, 1.0)
    if imgui.IsItemHovered() then
        imgui.SetTooltip(
            string.format(
                "#%02X%02X%02X%02X",
                i_custom[1],
                i_custom[2],
                i_custom[3],
                i_custom[4]
            )
        )
    end

    imgui.SameLine(0, 5)
    imgui.Text(label)

    default =
    bit.lshift(i_default[1], 24) +
    bit.lshift(i_default[2], 16) +
    bit.lshift(i_default[3], 8) +
    bit.lshift(i_default[4], 0)

    custom =
    bit.lshift(i_custom[1], 24) +
    bit.lshift(i_custom[2], 16) +
    bit.lshift(i_custom[3], 8) +
    bit.lshift(i_custom[4], 0)

    if custom ~= default then
        imgui.SameLine(0, 5)
        if imgui.Button("Revert") then
            custom = default
        end
    end

    imgui.PopID()
    imgui.EndGroup()

    return custom
end

local function PresentColorEditors()
    imgui.SetNextWindowSize(500, 400, 'FirstUseEver')
    if imgui.Begin("Theme Editor") then
        if imgui.Button("Save") then
            ExportTheme()
        end

        local success = false
        if theme.ImGuiIO['FontGlobalScale'] == nil then
            theme.ImGuiIO['FontGlobalScale'] = fontGlobalScale
        end

        imgui.PushItemWidth(150)
        success, theme.ImGuiIO['FontGlobalScale'] = imgui.DragFloat("Font Global Scale", theme.ImGuiIO['FontGlobalScale'], 0.01, 0.1, 10)
        imgui.PopItemWidth()

        if fontGlobalScale ~= theme.ImGuiIO['FontGlobalScale'] then
            imgui.SameLine(0, 5)
            if imgui.Button("Revert") then
                theme.ImGuiIO['FontGlobalScale'] = fontGlobalScale
            end
        end

        imgui.BeginChild("ColorList", 0)
        for i = 1, table.getn(colorList), 1 do
            theme.ImGuiStyle[colorList[i].name] = string.format("%08X",
                PresentColorEditor(colorList[i].name, tonumber("0x" .. colorList[i].color), tonumber("0x" .. theme.ImGuiStyle[colorList[i].name])))
        end
        imgui.EndChild()
    end
    imgui.End()
end

local function present()
    if enable == false then
        return
    end

    PresentColorEditors()
end

local function init()
    local function mainMenuButtonHandler()
        -- Parse theme since we will enable the window
        if enable == false then
            ParseTheme()
        end
        enable = not enable
    end

    core_mainmenu.add_button("Theme Editor", mainMenuButtonHandler)

    return
    {
        name = "Theme Editor",
        version = "1.1.1",
        author = "Solybum",
        description = "Theme editor for framework global theme",
        present = present,
    }
end

return
{
    __addon =
    {
        init = init
    }
}