// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2019 OpenCppCoverage
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
