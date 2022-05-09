# BB UI addons plugin

A plugin for Phantasy Star Online Blue Burst that enables graphical UI addons,
which can read the game's memory and present additional useful information.

- [Wiki](/../../wiki)
- [Releases](/../../releases)

## Installation

1. Download the latest release from the [Releases](/../../releases) page
and extract the zip contents to your PSOBB directory.
2. Install the [Visual C++ Redistributable for Visual Studio 2015](https://www.microsoft.com/en-us/download/details.aspx?id=48145).
3. Run the game.

## Building and Testing

To test the lua code, install luacheck and busted from luarocks and

    $ luacheck addons
    $ busted

To build the plugin dll, use Visual Studio 2015.

## Usage

Press the \` key to open the Main Menu of the addon.

The UI theme is configured via the `addons/theme.ini` file.

## Changelog

See CHANGELOG.md for more info

**The API is completely unstable for now, expect it to change.**

ImGui is exposed via the `imgui` module, which is in the global environment as `imgui`.

PSO specific functions are in the `pso` global table.

### API Docs

* Lua 5.1: https://www.lua.org/manual/5.1/
* LuaJit Extensions: http://bitop.luajit.org/api.html
* ImGui: https://github.com/ocornut/imgui
* ImGui API (C++ Header): https://github.com/ocornut/imgui/blob/master/imgui.h

`pso` global table:

 * read_u8, u16, u32, u64, i8, i16, i32, i64, f32, f64, -- read mem at address as type (little endian)
 * read_cstr(addr, len) -- read c_str at address with len bytes, or null terminated (0 len)
 * read_wstr(addr, len) -- read utf16 str to utf8 at address with len characters, or double null terminated (0 len)
 * read_mem(table, addr, len) -- read len bytes from addr into table. the table should be initialized as empty, i.e. `local table = {}; pso.read_mem(table, 0x00400000, 0x7fffffff-0x00400000)` (don't read the entire address space, that's silly and will probably kill the process)
 * read_mem_str(addr, len) -- read len bytes from addr into a string (not null terminated). lua treats binary files the same way it does text files so this can be used to efficiently write sections of memory directly to a file.
 * reload() -- at the end of present, re-initialize the lua state. all addons and modules will be reloaded, no state will be preserved.
 * base_address -- the base address of the PSOBB process
 * reload_custom_theme -- after the current frame, the custom theme is applied to handle changes.
 * get_tick_count -- returns the number of ticks for a timer.
 * change_global_font -- after the current frame, changes the font at runtime using the specified settings.
 * list_directory_files -- list files in the specified directory under the addons directory.
 * set_language -- used by the Settings Editor to set the internal language for addons.
 * get_language -- retrieves the language value for addons to handle translation.
 * get_version -- returns a table containing fields `version_string`, `major`, `minor`, and `patch`. The latter three are integer values corresponding to the plugin's version number. The `version_string` is a string representation.
 * require_version -- accepts three arguments specifying the version and returns true if the plugin's version is at that level or higher.
