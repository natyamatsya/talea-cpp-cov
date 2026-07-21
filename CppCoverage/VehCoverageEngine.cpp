// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2026 OpenCppCoverage
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "stdafx.h"
#include "VehCoverageEngine.hpp"

#include <Windows.h>

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "tools/Log.hpp"
#include "Tools/WarningManager.hpp"

#include "Address.hpp"
#include "CppCoverageException.hpp"
#include "CoverageFilterManager.hpp"
#include "DebugInformationEnumerator.hpp"
#include "ExecutedAddressManager.hpp"
#include "LineRunScanner.hpp"
#include "PdbLineTableCache.hpp"
#include "PeImageFileReader.hpp"
#include "RunCoverageSettings.hpp"
#include "StartInfo.hpp"

#include "FileFilter/ModuleInfo.hpp"
#include "FileFilter/FileInfo.hpp"
#include "FileFilter/LineInfo.hpp"

#include "VehCoverageHelper/VehProtocol.hpp"

namespace CppCoverage
{
	namespace
	{
		constexpr DWORD HelperReadyTimeoutMs = 30000;

		//---------------------------------------------------------------------
		// A selected line, in the module's RVA space.
		struct SelectedLine
		{
			std::uint32_t rva_;
			std::uint32_t length_;
			unsigned int lineNumber_;
			std::wstring filePath_;
		};

		//---------------------------------------------------------------------
		// Collects the function ranges and the filtered, selected lines of the
		// module straight from its PDB - no live process needed. Mirrors what
		// MonitoredLineRegister does in the debugger engine, minus the process.
		class ProbeCollector : public IDebugInformationHandler
		{
		  public:
			ProbeCollector(ICoverageFilterManager& filterManager,
			               const std::filesystem::path& modulePath)
			    : filterManager_{filterManager}, modulePath_{modulePath}
			{
			}

			bool IsSourceFileSelected(
			    const std::filesystem::path& path) override
			{
				return filterManager_.IsSourceFileSelected(path.wstring());
			}

			void OnModuleFunctions(
			    const std::vector<FunctionRange>& functions) override
			{
				functions_ = functions;
			}

			void OnSourceFile(const std::filesystem::path& path,
			                  const std::vector<Line>& lines) override
			{
				std::vector<FileFilter::LineInfo> lineInfos;
				lineInfos.reserve(lines.size());
				for (const auto& line : lines)
				{
					lineInfos.emplace_back(line.lineNumber_,
					                       line.virtualAddress_,
					                       line.symbolIndex_);
				}

				FileFilter::FileInfo fileInfo{path, std::move(lineInfos)};
				// baseOfImage / hProcess are only read by the optimized-build
				// filter, which the VEH engine never enables.
				FileFilter::ModuleInfo moduleInfo{nullptr, modulePath_,
				                                  nullptr};

				const auto& collection = fileInfo.lineInfoColllection_;
				for (std::size_t i = 0; i < collection.size(); ++i)
				{
					const auto& lineInfo = collection[i];
					if (!filterManager_.IsLineSelected(
					        moduleInfo, fileInfo, lineInfo))
						continue;

					selectedLines_.push_back(SelectedLine{
					    static_cast<std::uint32_t>(lineInfo.virtualAddress_),
					    lines[i].length_,
					    static_cast<unsigned int>(lineInfo.lineNumber_),
					    path.wstring()});
				}
			}

			const std::vector<FunctionRange>& GetFunctions() const
			{
				return functions_;
			}
			std::vector<SelectedLine>& GetSelectedLines()
			{
				return selectedLines_;
			}

		  private:
			ICoverageFilterManager& filterManager_;
			const std::filesystem::path modulePath_;
			std::vector<FunctionRange> functions_;
			std::vector<SelectedLine> selectedLines_;
		};

		//---------------------------------------------------------------------
		// Reduces the selected lines to probe leaders (one per straight-line
		// run when merging applies), registering every line under its leader
		// in the RVA-space probe table. Returns the sorted, unique leader RVAs
		// - the spec the helper patches.
		std::vector<std::uint32_t> BuildProbes(
		    const std::filesystem::path& modulePath,
		    bool is64BitModule,
		    bool enableLineMerging,
		    ProbeCollector& collector,
		    ExecutedAddressManager& manager)
		{
			auto& lines = collector.GetSelectedLines();
			std::sort(lines.begin(),
			          lines.end(),
			          [](const auto& lhs, const auto& rhs) {
				          return lhs.rva_ < rhs.rva_;
			          });

			std::vector<std::uint32_t> probeRvas;
			// leaderRva -> selected-line indices registered under it.
			std::unordered_map<std::uint32_t, std::vector<std::size_t>>
			    linesByLeader;
			auto addProbe = [&](std::uint32_t leaderRva, std::size_t index) {
				auto [it, inserted] = linesByLeader.emplace(
				    leaderRva, std::vector<std::size_t>{});
				if (inserted)
					probeRvas.push_back(leaderRva);
				it->second.push_back(index);
			};

			if (!enableLineMerging)
			{
				for (std::size_t i = 0; i < lines.size(); ++i)
					addProbe(lines[i].rva_, i);
			}
			else
			{
				auto reader = PeImageFileReader::TryCreate(modulePath);
				LineRunScanner scanner{is64BitModule};

				constexpr std::uint32_t MaxScannedFunctionSize = 1 << 20;
				std::size_t cursor = 0;
				for (const auto& function : collector.GetFunctions())
				{
					auto functionStart =
					    static_cast<std::uint32_t>(function.virtualAddress_);
					auto functionEnd = functionStart + function.length_;

					while (cursor < lines.size() &&
					       lines[cursor].rva_ < functionStart)
						addProbe(lines[cursor].rva_, cursor++);

					auto firstLine = cursor;
					while (cursor < lines.size() &&
					       lines[cursor].rva_ < functionEnd)
						++cursor;
					if (firstLine == cursor)
						continue;

					std::optional<std::vector<LineRun>> runs;
					if (reader && function.length_ <= MaxScannedFunctionSize)
					{
						auto buffer = reader->ReadCode(functionStart,
						                               function.length_);
						if (!buffer.empty())
						{
							std::vector<LineRunLine> scannerLines;
							scannerLines.reserve(cursor - firstLine);
							for (auto i = firstLine; i < cursor; ++i)
								scannerLines.push_back(LineRunLine{
								    lines[i].rva_, lines[i].length_});
							runs = scanner.ComputeRuns(buffer.data(),
							                           buffer.size(),
							                           functionStart,
							                           scannerLines);
						}
					}

					if (runs)
					{
						std::vector<bool> assigned(cursor - firstLine, false);
						for (const auto& run : *runs)
						{
							for (auto lineIndex : run.lineIndices_)
							{
								addProbe(static_cast<std::uint32_t>(
								             run.leaderVirtualAddress_),
								         firstLine + lineIndex);
								assigned[lineIndex] = true;
							}
						}
						for (std::size_t i = 0; i < assigned.size(); ++i)
						{
							if (!assigned[i])
								addProbe(lines[firstLine + i].rva_,
								         firstLine + i);
						}
					}
					else
					{
						for (auto i = firstLine; i < cursor; ++i)
							addProbe(lines[i].rva_, i);
					}
				}
				for (; cursor < lines.size(); ++cursor)
					addProbe(lines[cursor].rva_, cursor);
			}

			// Register every line under its leader address (RVA space, with a
			// synthetic null process handle). One leader can carry many lines
			// (run merging); one line can appear under several leaders (a line
			// split across runs) - both are native to ExecutedAddressManager.
			for (const auto& [leaderRva, lineIndices] : linesByLeader)
			{
				Address address{nullptr,
				                reinterpret_cast<void*>(
				                    static_cast<std::uintptr_t>(leaderRva))};
				for (auto index : lineIndices)
				{
					manager.RegisterAddress(address,
					                        lines[index].filePath_,
					                        lines[index].lineNumber_,
					                        0);
				}
			}

			std::sort(probeRvas.begin(), probeRvas.end());
			return probeRvas;
		}

		//---------------------------------------------------------------------
		std::vector<wchar_t>
		BuildCommandLine(const std::vector<std::wstring>& arguments)
		{
			std::vector<wchar_t> buffer;
			for (const auto& argument : arguments)
			{
				buffer.push_back(L'"');
				buffer.insert(buffer.end(), argument.begin(), argument.end());
				buffer.push_back(L'"');
				buffer.push_back(L' ');
			}
			buffer.push_back(L'\0');
			return buffer;
		}

		//---------------------------------------------------------------------
		bool
		Is64BitProcess(HANDLE hProcess)
		{
			BOOL isWow64 = FALSE;
			// On x64 Windows a 32-bit process runs under WOW64. The host is
			// x64, so a non-WOW64 process is 64-bit.
			if (!IsWow64Process(hProcess, &isWow64))
				return true;
			return !isWow64;
		}

		//---------------------------------------------------------------------
		// Loads the helper DLL into the target by running LoadLibraryW there.
		// kernel32 is mapped at the same address in every same-bitness process
		// of the session, so its LoadLibraryW address is valid cross-process.
		bool InjectHelper(HANDLE hProcess,
		                  const std::filesystem::path& helperDllPath)
		{
			auto pathString = helperDllPath.wstring();
			auto byteSize = (pathString.size() + 1) * sizeof(wchar_t);

			auto* remoteMemory = VirtualAllocEx(
			    hProcess, nullptr, byteSize, MEM_COMMIT, PAGE_READWRITE);
			if (!remoteMemory)
			{
				LOG_ERROR << L"VEH: cannot allocate memory in the target.";
				return false;
			}

			bool success = false;
			if (WriteProcessMemory(hProcess,
			                       remoteMemory,
			                       pathString.c_str(),
			                       byteSize,
			                       nullptr))
			{
				auto* kernel32 = GetModuleHandleW(L"kernel32.dll");
				auto loadLibrary =
				    kernel32 ? GetProcAddress(kernel32, "LoadLibraryW")
				             : nullptr;
				if (loadLibrary)
				{
					auto thread = CreateRemoteThread(
					    hProcess,
					    nullptr,
					    0,
					    reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibrary),
					    remoteMemory,
					    0,
					    nullptr);
					if (thread)
					{
						WaitForSingleObject(thread, INFINITE);
						CloseHandle(thread);
						success = true;
					}
				}
			}

			if (!success)
				LOG_ERROR << L"VEH: cannot inject the coverage helper.";

			VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
			return success;
		}

		//---------------------------------------------------------------------
		bool WaitForHelperReady(volatile VehCoverage::SharedHeader* header)
		{
			auto deadline = GetTickCount64() + HelperReadyTimeoutMs;
			for (;;)
			{
				auto status = header->status;
				if (status ==
				    static_cast<std::uint32_t>(
				        VehCoverage::HelperStatus::Ready))
					return true;
				if (status ==
				    static_cast<std::uint32_t>(
				        VehCoverage::HelperStatus::Error))
				{
					LOG_ERROR << L"VEH: helper reported error code "
					          << header->helperErrorCode << L".";
					return false;
				}
				if (GetTickCount64() >= deadline)
				{
					LOG_ERROR << L"VEH: timed out waiting for the helper.";
					return false;
				}
				Sleep(1);
			}
		}
	}

	//-------------------------------------------------------------------------
	VehCoverageEngine::VehCoverageEngine(
	    std::shared_ptr<Tools::WarningManager> warningManager)
	    : warningManager_{std::move(warningManager)}
	{
	}

	//-------------------------------------------------------------------------
	VehCoverageEngine::~VehCoverageEngine() = default;

	//-------------------------------------------------------------------------
	std::filesystem::path VehCoverageEngine::GetHelperDllPath()
	{
		HMODULE thisModule = nullptr;
		if (!GetModuleHandleExW(
		        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		        reinterpret_cast<LPCWSTR>(&VehCoverageEngine::GetHelperDllPath),
		        &thisModule))
		{
			return L"VehCoverageHelper.dll";
		}

		wchar_t modulePath[MAX_PATH];
		auto length =
		    GetModuleFileNameW(thisModule, modulePath, MAX_PATH);
		if (length == 0 || length == MAX_PATH)
			return L"VehCoverageHelper.dll";

		std::filesystem::path path{modulePath};
		return path.parent_path() / L"VehCoverageHelper.dll";
	}

	//-------------------------------------------------------------------------
	Plugin::CoverageData
	VehCoverageEngine::RunCoverage(const RunCoverageSettings& settings)
	{
		const auto& startInfo = settings.GetStartInfo();
		const auto& modulePath = startInfo.GetPath();
		auto moduleName = modulePath.filename().wstring();

		CoverageFilterManager filterManager{
		    settings.GetCoverageFilterSettings(),
		    settings.GetUnifiedDiffSettings(),
		    settings.GetExcludedLineRegexes(),
		    /*useReleaseCoverageFilter*/ false};

		ExecutedAddressManager manager;
		manager.AddModule(modulePath.wstring(), nullptr);

		std::vector<std::uint32_t> probeRvas;
		if (filterManager.IsModuleSelected(modulePath.wstring()))
		{
			std::shared_ptr<PdbLineTableCache> pdbLineTableCache;
			if (settings.IsPdbCacheEnabled())
			{
				pdbLineTableCache = std::make_shared<PdbLineTableCache>(
				    PdbLineTableCache::GetDefaultDiskCacheDirectory());
			}

			DebugInformationEnumerator enumerator{
			    settings.GetSubstitutePdbSourcePaths(),
			    /*needSymbolIndex*/ false,
			    pdbLineTableCache};
			ProbeCollector collector{filterManager, modulePath};
			if (!enumerator.Enumerate(modulePath, collector))
			{
				LOG_WARNING << L"VEH: no debug information for "
				            << modulePath.wstring()
				            << L"; the module is not instrumented.";
			}
			else
			{
				probeRvas = BuildProbes(modulePath,
				                        /*is64BitModule*/ true,
				                        settings.IsLineMergingEnabled(),
				                        collector,
				                        manager);
			}
		}

		auto commandLine = BuildCommandLine(startInfo.GetArguments());
		const auto* workingDirectory = startInfo.GetWorkingDirectory();

		STARTUPINFOW startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		PROCESS_INFORMATION processInfo{};
		if (!CreateProcessW(nullptr,
		                    commandLine.data(),
		                    nullptr,
		                    nullptr,
		                    FALSE,
		                    CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
		                    nullptr,
		                    workingDirectory ? workingDirectory->c_str()
		                                     : nullptr,
		                    &startupInfo,
		                    &processInfo))
		{
			THROW(L"VEH: cannot start " + modulePath.wstring());
		}

		if (!Is64BitProcess(processInfo.hProcess))
		{
			TerminateProcess(processInfo.hProcess, 1);
			CloseHandle(processInfo.hThread);
			CloseHandle(processInfo.hProcess);
			THROW(L"VEH mode supports only 64-bit targets.");
		}

		HANDLE mapping = nullptr;
		void* view = nullptr;
		if (!probeRvas.empty())
		{
			auto mappingName = std::wstring{VehCoverage::MappingNamePrefix} +
			                   std::to_wstring(processInfo.dwProcessId);
			auto sharedSize = VehCoverage::GetSharedSize(
			    static_cast<std::uint32_t>(probeRvas.size()));

			mapping = CreateFileMappingW(
			    INVALID_HANDLE_VALUE,
			    nullptr,
			    PAGE_READWRITE,
			    0,
			    static_cast<DWORD>(sharedSize),
			    mappingName.c_str());
			if (mapping)
				view = MapViewOfFile(
				    mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);

			if (!view)
			{
				LOG_ERROR << L"VEH: cannot create the shared section; "
				             L"running without instrumentation.";
				if (mapping)
					CloseHandle(mapping);
				mapping = nullptr;
			}
			else
			{
				auto* header =
				    static_cast<VehCoverage::SharedHeader*>(view);
				header->magic = VehCoverage::ProtocolMagic;
				header->version = VehCoverage::ProtocolVersion;
				header->status = static_cast<std::uint32_t>(
				    VehCoverage::HelperStatus::Initializing);
				header->probeCount =
				    static_cast<std::uint32_t>(probeRvas.size());
				header->helperErrorCode = 0;
				header->reserved = 0;

				auto* rvaArray = const_cast<std::uint32_t*>(
				    VehCoverage::GetRvaArray(header));
				std::copy(
				    probeRvas.begin(), probeRvas.end(), rvaArray);

				if (InjectHelper(processInfo.hProcess, GetHelperDllPath()))
					WaitForHelperReady(header);
				else
					LOG_WARNING << L"VEH: continuing without instrumentation.";
			}
		}

		ResumeThread(processInfo.hThread);
		WaitForSingleObject(processInfo.hProcess, INFINITE);

		DWORD exitCode = 0;
		GetExitCodeProcess(processInfo.hProcess, &exitCode);

		if (view)
		{
			auto* header = static_cast<VehCoverage::SharedHeader*>(view);
			auto* hits = VehCoverage::GetHitArray(header);
			for (std::size_t i = 0; i < probeRvas.size(); ++i)
			{
				if (hits[i])
				{
					Address address{
					    nullptr,
					    reinterpret_cast<void*>(
					        static_cast<std::uintptr_t>(probeRvas[i]))};
					manager.MarkAddressAsExecuted(address);
				}
			}
			UnmapViewOfFile(view);
		}
		if (mapping)
			CloseHandle(mapping);

		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);

		return manager.CreateCoverageData(moduleName,
		                                  static_cast<int>(exitCode));
	}
}
