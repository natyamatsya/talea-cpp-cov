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

#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

#include "CppCoverageExport.hpp"

namespace CppCoverage
{
	// Reads a PE image's code straight from the file on disk and exposes it in
	// RVA space. The VEH engine needs the original (unpatched) bytes of a
	// function to reconstruct its straight-line runs without a live process,
	// and control-flow opcodes are unaffected by load-time relocation, so the
	// on-disk bytes are exactly what the disassembler needs.
	class CPPCOVERAGE_DLL PeImageFileReader
	{
	  public:
		// Returns nullptr when the file is not a readable PE image.
		static std::unique_ptr<PeImageFileReader>
		TryCreate(const std::filesystem::path&);

		~PeImageFileReader();

		// The code bytes for [rva, rva + size), or empty when the range is not
		// fully contained in a single initialized section.
		std::vector<std::uint8_t> ReadCode(std::uint32_t rva,
		                                     std::uint32_t size) const;

	  private:
		class Impl;
		explicit PeImageFileReader(std::unique_ptr<Impl>);

		PeImageFileReader(const PeImageFileReader&) = delete;
		PeImageFileReader& operator=(const PeImageFileReader&) = delete;

		std::unique_ptr<Impl> impl_;
	};
}
