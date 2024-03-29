/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#ifndef _DEBUG_H
#define _DEBUG_H

#include <string>
#include <stdio.h>
#include <assert.h>

#include <iostream>

using namespace std;

#define SHORT_FILE (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#ifdef ECHOLIB_DEBUG

#define DEBUGGING if (__is_debug_enabled())

#define DEBUGMSG(...) if (__is_debug_enabled()) { fprintf(__debug_get_target(), "%s(%d): ", __short_file_name(__FILE__), __LINE__); fprintf(__debug_get_target(), __VA_ARGS__); }
#define PING if (__is_debug_enabled()) { fprintf(__debug_get_target(), "%s(%d): PING\n", __short_file_name(__FILE__), __LINE__); }

#else

#define DEBUGGING if (false)
#define DEBUGMSG(...)
#define PING

#endif

void __debug_enable();

void __debug_disable();

int __is_debug_enabled();

void __debug_set_target(FILE* stream);

FILE* __debug_get_target();

const char* __short_file_name(const char* filename);

void __debug_flush();

void tic();

void toc();

void print_buffer(ostream& output, uint8_t* buffer, size_t length);

std::string format_string(char const* fmt, ...) __attribute__((format(printf,1,2)));

#endif

