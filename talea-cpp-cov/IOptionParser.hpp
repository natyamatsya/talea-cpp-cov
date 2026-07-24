// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2019 OpenCppCoverage

#pragma once

#include "TaleaCppCovExport.hpp"
#include "tools/Error.hpp"

namespace TaleaCppCov
{
	class Options;
	struct OptionsSpec;

	// Post-processes an already-parsed OptionsSpec into the Options object.
	// Options are declared as fields of OptionsSpec; an IOptionParser only
	// interprets the ones with non-trivial semantics (e.g. --export_type).
	class TALEACPPCOV_DLL IOptionParser
	{
	  public:
		virtual ~IOptionParser() = default;

		[[nodiscard]] virtual Tools::Expected<void> ParseOption(const OptionsSpec&,
		                                                        Options&) = 0;
	};
}
