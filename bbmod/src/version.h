#pragma once

#define str(a) #a
#define xstr(a) str(a)

#define BBMOD_VERSION_MAJOR  3
#define BBMOD_VERSION_MINOR  6
#define BBMOD_VERSION_PATCH  0
#define BBMOD_VERSION_STRING ("v" xstr(BBMOD_VERSION_MAJOR) "." xstr(BBMOD_VERSION_MINOR) "." xstr(BBMOD_VERSION_PATCH))

