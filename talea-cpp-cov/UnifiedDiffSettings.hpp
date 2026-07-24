// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#pragma once

#include <filesystem>
#include <optional>

#include "TaleaCppCovExport.hpp"

namespace TaleaCppCov
{
	class TALEACPPCOV_DLL UnifiedDiffSettings
	{
	public:
		UnifiedDiffSettings(
			const std::filesystem::path& unifiedDiffPath,
			const std::optional<std::filesystem::path>& rootDiffFolder);
		UnifiedDiffSettings(const UnifiedDiffSettings&) = default;
		UnifiedDiffSettings(UnifiedDiffSettings&&) = default;

		const std::filesystem::path& GetUnifiedDiffPath() const;
		const std::optional<std::filesystem::path>& GetRootDiffFolder() const;

		UnifiedDiffSettings& operator=(const UnifiedDiffSettings&) = default;

	private:
		std::filesystem::path unifiedDiffPath_;
		std::optional<std::filesystem::path> rootDiffFolder_;
	};
}
