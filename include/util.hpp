#pragma once

#include <fstream>

#include "microlog.h"

inline std::string readfile(const std::string &path)
{
	std::ifstream file(path);
	ulog_assert(file.is_open(), "Could not open file: %s\n", path.c_str());

	std::stringstream buffer;
	buffer << file.rdbuf();

	return buffer.str();
}