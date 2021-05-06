#pragma once
#include <winternl.h>
#include <winnt.h>
#include <Windows.h>

class PatternScanner
{
public:
	char* ScanBasic();
	char* ScanModuleInternal();

private:
	char* _strtoc();
};