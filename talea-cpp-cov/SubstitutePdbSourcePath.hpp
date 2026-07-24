// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017 OpenCppCoverage

#pragma once

#include <filesystem>
#include "TaleaCppCovExport.hpp"

namespace TaleaCppCov
{
	class TALEACPPCOV_DLL SubstitutePdbSourcePath
	{
	  public:
		//---------------------------------------------------------------------
		SubstitutePdbSourcePath(
		    std::filesystem::path&& pdbStartPath,
		    std::filesystem::path&& localPath)
		    : pdbStartPath_{std::move(pdbStartPath)}, localPath_{std::move(localPath)}
		{
		}

		//---------------------------------------------------------------------
		SubstitutePdbSourcePath(SubstitutePdbSourcePath&) = default;
		SubstitutePdbSourcePath(SubstitutePdbSourcePath&&) = default;

		//---------------------------------------------------------------------
		const std::filesystem::path& GetPdbStartPath() const
		{
			return pdbStartPath_;
		}

		//---------------------------------------------------------------------
		const std::filesystem::path& GetLocalPath() const
		{
			return localPath_;
		}

	  private:
		std::filesystem::path pdbStartPath_;
		std::filesystem::path localPath_;
	};
}
