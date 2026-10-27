#ifndef VERSION_H
#define VERSION_H

// The program's version, set by CMake from project(VERSION).
#ifndef SM_VERSION_TEXT
#define SM_VERSION_TEXT "dev"
#endif

#define SM_WIDEN_(x) L##x
#define SM_WIDEN(x) SM_WIDEN_(x)

// "SpaceMonger 1.4.7": the window title and anywhere else the version shows.
#define SM_APP_TITLE L"SpaceMonger " SM_WIDEN(SM_VERSION_TEXT)

#endif
