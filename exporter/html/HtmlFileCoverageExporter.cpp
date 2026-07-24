// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>
#include "HtmlFileCoverageExporter.hpp"

#include <fstream>
#include <filesystem>
#include <string_view>

#include "coverage/FileCoverage.hpp"

#include "tools/Error.hpp"

namespace fs = std::filesystem;

namespace Exporter
{
	namespace
	{
		//---------------------------------------------------------------------
		void ReplaceAll(std::wstring& str,
		                std::wstring_view from,
		                std::wstring_view to)
		{
			size_t pos = 0;
			while ((pos = str.find(from, pos)) != std::wstring::npos)
			{
				str.replace(pos, from.size(), to);
				pos += to.size();
			}
		}

		//---------------------------------------------------------------------
		// Same behavior as the previously used
		// boost::spirit::classic::xml::encode.
		std::wstring EncodeHtml(std::wstring str)
		{
			ReplaceAll(str, L"&", L"&amp;");
			ReplaceAll(str, L"<", L"&lt;");
			ReplaceAll(str, L">", L"&gt;");
			ReplaceAll(str, L"\r", L"\\r");
			ReplaceAll(str, L"\n", L"\\n");
			return str;
		}

		//---------------------------------------------------------------------
		bool HaveSameCoverage(
			const Coverage::LineCoverage* lineCoverage,
			const Coverage::LineCoverage* otherLineCoverage)
		{
			if (!lineCoverage || !otherLineCoverage)
				return lineCoverage == otherLineCoverage;
			return lineCoverage->HasBeenExecuted() == otherLineCoverage->HasBeenExecuted();
		}

		//---------------------------------------------------------------------
		std::wstring GetStyle(const Coverage::LineCoverage* lineCoverage)
		{
			if (!lineCoverage)
				return L"";

			return (lineCoverage->HasBeenExecuted())
				? HtmlFileCoverageExporter::StyleBackgroundColorExecuted
				: HtmlFileCoverageExporter::StyleBackgroundColorUnexecuted;
		}

		//---------------------------------------------------------------------
		void AddEndStyleIfNeeded(
			std::wostream& output,
			const Coverage::LineCoverage* previousLineCoverage)
		{
			if (previousLineCoverage)
				output << HtmlFileCoverageExporter::EndStyle;
		}

		//---------------------------------------------------------------------
		bool AddLineCoverageColor(
			std::wostream& output,
			const std::wstring& line, 
			const Coverage::LineCoverage* lineCoverage,
			const Coverage::LineCoverage* previousLineCoverage)
		{
			if (HaveSameCoverage(lineCoverage, previousLineCoverage))
			{
				output << std::endl << line;
				return false;
			}
			
			AddEndStyleIfNeeded(output, previousLineCoverage);
			auto style = GetStyle(lineCoverage);

			output << std::endl;			
			output << style << line;

			return !style.empty();
		}

		const std::wstring StyleBackgroundColor = L"<span style = \"background-color:#";
	}

	const std::wstring HtmlFileCoverageExporter::StyleBackgroundColorExecuted = 
		StyleBackgroundColor + L"dfd" + L"\">";
	const std::wstring HtmlFileCoverageExporter::StyleBackgroundColorUnexecuted = 
		StyleBackgroundColor + L"fdd" + L"\">";
	const std::wstring HtmlFileCoverageExporter::EndStyle = L"</span>";

	//-------------------------------------------------------------------------
	HtmlFileCoverageExporter::HtmlFileCoverageExporter(
		std::int32_t maxSourceLineCount,
		std::int32_t maxSourceLineStyleChangesCount,
		std::int32_t maxStyleChangesCount)
		: maxSourceLineCount_{ maxSourceLineCount }
		, maxSourceLineStyleChangesCount_{ maxSourceLineStyleChangesCount }
		, maxStyleChangesCount_{ maxStyleChangesCount }
	{
	}

	//-------------------------------------------------------------------------
	Tools::Expected<bool> HtmlFileCoverageExporter::Export(
		const Coverage::FileCoverage& fileCoverage,
		std::wostream& output) const
	{
		auto filePath = fileCoverage.GetPath();

		std::wifstream ifs{filePath.string()};
		if (!ifs)
			return std::unexpected(
			    MAKE_ERROR(L"Cannot open file : " << filePath.wstring()));

		std::wstring line;
		const Coverage::LineCoverage* previousLineCoverage = nullptr;
		std::int32_t styleChangesCount = 0;
		std::int32_t lineCount = 0;
		for (std::int32_t i = 1; std::getline(ifs, line); ++i)
		{			
			auto lineCoverage = fileCoverage[i];
			
			line = EncodeHtml(std::move(line));
			if (AddLineCoverageColor(output, line, lineCoverage, previousLineCoverage))
				++styleChangesCount;
			++lineCount;
			previousLineCoverage = lineCoverage;
		}
		AddEndStyleIfNeeded(output, previousLineCoverage);
		output.flush();

		return MustEnableCodePrettify(lineCount, styleChangesCount);
	}

	//-------------------------------------------------------------------------
	bool HtmlFileCoverageExporter::MustEnableCodePrettify(
		std::int32_t lineCount,
		std::int32_t styleChangesCount) const
	{
		if (lineCount > maxSourceLineCount_)
			return false;

		auto ratio = static_cast<double>(maxSourceLineStyleChangesCount_ - maxStyleChangesCount_) 
			/ (maxSourceLineCount_ - maxStyleChangesCount_);
		auto maxStyleChanges = maxStyleChangesCount_ + (lineCount - maxStyleChangesCount_) * ratio;
		
		return styleChangesCount <= maxStyleChanges;
	}
}
