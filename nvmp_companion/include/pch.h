#pragma once

// Minimal precompiled header — we define our own NVSE types
// instead of pulling in the full xNVSE header chain
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

typedef unsigned char  UInt8;
typedef unsigned short UInt16;
typedef unsigned long  UInt32;
typedef unsigned long long UInt64;
typedef signed long    SInt32;
typedef float          Float32;
