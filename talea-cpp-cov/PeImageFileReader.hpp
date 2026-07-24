// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

#include "TaleaCppCovExport.hpp"

namespace TaleaCppCov
{
	// Reads a PE image's code straight from the file on disk and exposes it in
	// RVA space. The VEH engine needs the original (unpatched) bytes of a
	// function to reconstruct its straight-line runs without a live process,
	// and control-flow opcodes are unaffected by load-time relocation, so the
	// on-disk bytes are exactly what the disassembler needs.
	class TALEACPPCOV_DLL PeImageFileReader
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
