-- Core Addon - Main Menu

local present
local init
local key_pressed

local hooked_buttons = {}
local add_button
local remove_button

local optionsLoaded, options = pcall(require, "core_mainmenu.options")

local optionsFileName = "addons/core_mainmenu/options.lua"

if optionsLoaded then
    options.windowOpen = options.windowOpen == nil and true or options.windowOpen
else
    options =
    {
        windowOpen = true
    }
end

local exit_game_window = false

init = function()
  return {
    name = "Core - Main Menu",
    author = "Eidolon",
    version = "0.3.2",
    description = "An addon main menu with hooks for adding buttons.",
    present = present,
    key_pressed = key_pressed,
    toggleable = false,
  }
end

local function SaveOptions(options)
    local file = io.open(optionsFileName, "w")
    if file ~= nil then
        io.output(file)

        io.write("return\n")
        io.write("{\n")
        io.write(string.format("    windowOpen = %s,\n", tostring(options.windowOpen)))
        io.write("}\n")

        io.close(file)
    end
end

key_pressed = function(key)
  if (key == 192) then
    options.windowOpen = not options.windowOpen
    SaveOptions(options)
  end
end

present = function()
  if (not options.windowOpen) then
    return
  end
  local status
  imgui.SetNextWindowSize(200, 250, 'FirstUseEver')
  status, options.windowOpen = imgui.Begin('Main', options.windowOpen)
  for name, func in pairs(hooked_buttons) do
    if (imgui.Button(name)) then
      func()
    end
  end
  if (imgui.Button('Reload')) then
    pso.reload()
  end
  if (imgui.Button('Exit Game')) then
    exit_game_window = true
  end
  imgui.End()

  if (exit_game_window) then
    status, exit_game_window = imgui.Begin('Exit Game', exit_game_window)
    if (exit_game_window) then
      imgui.Text('Are you sure?')
      if (imgui.Button('Yes')) then
        os.exit()
      end
      imgui.SameLine()
      if (imgui.Button('No')) then
        exit_game_window = false
      end
      imgui.End()
    else
      imgui.End()
    end
  end
end

add_button = function(name, func)
  hooked_buttons[name] = func
end

remove_button = function(name)
  hooked_buttons[name] = nil
end

return {
  __addon = {
    init = init
  },
  add_button = add_button,
  remove_button = remove_button
}
