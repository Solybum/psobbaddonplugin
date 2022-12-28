-- Core Addon - Main Menu

local present
local init
local key_pressed

local hooked_buttons = {}
local add_button
local remove_button

local window_open = true

local exit_game_window = false

init = function()
  -- close main menu automatically
  key_pressed(192)

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

key_pressed = function(key)
  if (key == 192) then
    window_open = not window_open
  end
end

present = function()
  if (not window_open) then
    return
  end
  local status
  imgui.SetNextWindowSize(200, 250, 'FirstUseEver')
  status, window_open = imgui.Begin('Main', window_open)
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
