#pragma once

struct lua_State;

void psolua_register_image_library(struct lua_State* L);
void psolua_release_all_textures(void);
