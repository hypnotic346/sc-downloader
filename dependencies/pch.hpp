#pragma once

#define CURL_STATICLIB
#define IMGUI_DEFINE_MATH_OPERATORS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <ostream>
#include <fstream>
#include <iostream>
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <cpr/cpr.h>
#include <string>
#include <windows.h>
#include <downloader/downloader.hpp>
#include <deque>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_impl_win32.h"
