// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#include "stdafx.h"
#include <windows.h>
#include <regex>
#include "tools/StringUtils.hpp"

#include "file-filter/RelocationsExtractor.hpp"
#include "test-coverage-optimized-build/TestCoverageOptimizedBuild.hpp"

#include "test-helper/Tools.hpp"

namespace fs = std::filesystem;

namespace FileFilterTest
{	
	//-------------------------------------------------------------------------
	fs::path GetDumpBinPath()
	{
		auto msvc = TestHelper::GetVisualStudioPath() / "VC" / "Tools" / "MSVC";
		fs::recursive_directory_iterator end;

		const auto it = std::find_if(fs::recursive_directory_iterator(msvc), end,
			[](const fs::directory_entry& entry) {
			return Tools::IEquals(entry.path().filename().string(), std::string_view{"dumpbin.exe"});
		});
		if (it == end)
			throw std::runtime_error("Cannot find dumpbin.exe");
		return it->path();
	}
	
	//-------------------------------------------------------------------------
	DWORD64 ToDWord64(const std::string& str)
	{
		return strtoll(str.c_str(), nullptr, 16);
	}

	//-------------------------------------------------------------------------
	std::unordered_set<DWORD64> ExtractRelocations(const fs::path& dumpBinPath)
	{		
		std::vector<std::string> args = { 
			"/RELOCATIONS", 
			TestCoverageOptimizedBuild::GetOutputBinaryPath().string() };
		const std::string value = TestHelper::RunProcess(dumpBinPath, args);
		auto current = value.begin();
		auto end = value.end();
		std::smatch match;
		std::unordered_set<DWORD64> relocations;

		// Example: "      CCF  HIGHLOW            1001C3E8"
		//       or "      150  DIR64      000000018001B7D0"
		// multiline: recent MSVC versions no longer match '^' at the
		// beginning of every line by default.
		std::regex r(R"(^\s*[[:xdigit:]]*\s*(HIGHLOW|DIR64)\s*([[:xdigit:]]*))",
		             std::regex::ECMAScript | std::regex::multiline);

		while (std::regex_search(current, end, match, r))
		{			
			if (match.size() == 3)
			{
				auto addressStr = match[2];
				auto address = ToDWord64(addressStr);
				relocations.insert(address);
			}
			current += match.position() + match.length();
		}
		return relocations;
	}

	//-------------------------------------------------------------------------
	DWORD64 ExtractBaseAddress(const fs::path& dumpBinPath)
	{		
		std::vector<std::string> args = { 
			"/HEADERS", 
			TestCoverageOptimizedBuild::GetOutputBinaryPath().string() };
		const std::string value = TestHelper::RunProcess(dumpBinPath, args);
		std::smatch match;
		
		// Example: "        10000000 image base (10000000 to 1001FFFF)"
		std::regex r(R"(^\s*([[:xdigit:]]*) image base )",
		             std::regex::ECMAScript | std::regex::multiline);

		if (!std::regex_search(value, match, r) || match.size() != 2)
			throw std::runtime_error("Cannot extract image base");
				
		auto addressStr = match[1];
		return ToDWord64(addressStr);
	}

	//-------------------------------------------------------------------------
	TEST(RelocationsExtractorTest, Extract)
	{			
		FileFilter::RelocationsExtractor extractor;

		auto hProcess = GetCurrentProcess();
		auto hModule = GetModuleHandle(TestCoverageOptimizedBuild::GetOutputBinaryPath().c_str());
		auto baseOfImage = reinterpret_cast<DWORD64>(hModule);
		ASSERT_NE(0, baseOfImage);

		auto dumpBinPath = GetDumpBinPath();
		auto baseAddress = ExtractBaseAddress(dumpBinPath);
		auto relocations = extractor.Extract(hProcess, baseOfImage);

		std::unordered_set<DWORD64> relocationsWithBaseAddress;
		for (auto relocation : relocations)
			relocationsWithBaseAddress.insert(relocation + baseAddress);
		auto expectedRelocations = ExtractRelocations(dumpBinPath);

		// Bounds of the loaded module, read straight from its in-memory PE
		// header (the module is mapped in this process).
		const auto* dosHeader =
		    reinterpret_cast<const IMAGE_DOS_HEADER*>(hModule);
		const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(
		    reinterpret_cast<const std::uint8_t*>(hModule) +
		    dosHeader->e_lfanew);
		const DWORD64 imageEnd =
		    baseAddress + ntHeaders->OptionalHeader.SizeOfImage;
		const auto isInModule = [&](DWORD64 value) {
			return value >= baseAddress && value < imageEnd;
		};

		// The extractor reads relocated pointer values from live process memory.
		// Every relocation whose target is inside the module must match dumpbin
		// of the file on disk exactly. The only exceptions are Control Flow
		// Guard check/dispatch pointers: the static CRT embeds them, and the
		// loader repoints them outside the module at load time, so the extractor
		// reads an out-of-module address while the file still holds the original
		// in-module stub. There are only a couple of these.
		constexpr std::size_t maxGuardPointers = 4;

		std::size_t extractorOutOfModule = 0;
		for (auto relocation : relocationsWithBaseAddress)
		{
			if (isInModule(relocation))
				ASSERT_TRUE(expectedRelocations.count(relocation))
				    << "in-module relocation absent from file: " << std::hex
				    << relocation;
			else
				++extractorOutOfModule;
		}

		std::size_t fileOnlyInModule = 0;
		for (auto relocation : expectedRelocations)
			if (isInModule(relocation) &&
			    !relocationsWithBaseAddress.count(relocation))
				++fileOnlyInModule;

		ASSERT_LE(extractorOutOfModule, maxGuardPointers);
		ASSERT_LE(fileOnlyInModule, maxGuardPointers);
	}
}