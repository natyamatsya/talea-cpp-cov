// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2014 OpenCppCoverage
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
#include <cstdint>

#include <chrono>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <unordered_set>

#include <fmt/format.h>
#include <fmt/xchar.h>

#include "CoberturaExporter.hpp"
#include "Plugin/Exporter/CoverageData.hpp"
#include "Plugin/Exporter/ModuleCoverage.hpp"
#include "Plugin/Exporter/FileCoverage.hpp"
#include "Plugin/Exporter/LineCoverage.hpp"
#include "CppCoverage/CoverageRateComputer.hpp"
#include "InvalidOutputFileException.hpp"

#include "Tools/Tool.hpp"

namespace fs = std::filesystem;

// This exporter writes the Cobertura XML directly to the output stream rather
// than building an intermediate boost::property_tree DOM. The output is kept
// byte-for-byte identical to what property_tree's write_xml produced (2-space
// indentation, self-closing empty elements, insertion-ordered attributes,
// max_digits10 floating point, and &<>'" entity escaping).
namespace Exporter
{
	namespace
	{
		//---------------------------------------------------------------------
		std::wstring FormatRate(double value)
		{
			std::wostringstream ostr;
			ostr.imbue(std::locale::classic());
			ostr.precision(std::numeric_limits<double>::max_digits10);
			ostr << value;
			return ostr.str();
		}

		//---------------------------------------------------------------------
		std::wstring ToUft8WString(const fs::path& path)
		{
			auto str = Tools::ToUtf8String(path.wstring());
			return Tools::LocalToWString(str);
		}

		//---------------------------------------------------------------------
		std::int32_t GetCoveredOutcomeCount(const Plugin::BranchCoverage& branch)
		{
			return (branch.HasBeenTaken() ? 1 : 0) +
			       (branch.HasBeenFallenThrough() ? 1 : 0);
		}

		//---------------------------------------------------------------------
		std::wstring GetPercentString(std::int32_t covered, std::int32_t total)
		{
			auto percent = (total == 0) ? 0 : (100 * covered) / total;
			return fmt::format(L"{}%", percent);
		}

		//---------------------------------------------------------------------
		// Minimal indentation-aware XML writer matching property_tree output.
		//
		// The whole document is built in an in-memory std::wstring and written
		// to the stream once by Flush(). A wostream applies its locale/codecvt
		// and dispatches through the stream buffer on every insertion, so the
		// per-element operator<< form used previously spent most of its time in
		// stream machinery rather than formatting; appending to a reused buffer
		// and writing it in a single call is several times faster on documents
		// with many <line/> elements. The produced characters are identical.
		class XmlWriter
		{
		  public:
			explicit XmlWriter(std::wostream& ostr) : ostr_{ostr}
			{
				buffer_.reserve(64 * 1024);
				buffer_ += L"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
			}

			// Write the accumulated document to the stream. Called explicitly
			// so an I/O error propagates as an exception (not from a dtor).
			void Flush()
			{
				ostr_.write(buffer_.data(),
				            static_cast<std::streamsize>(buffer_.size()));
				buffer_.clear();
			}

			void OpenTag(const std::wstring& name)
			{
				Indent();
				buffer_ += L'<';
				buffer_ += name;
			}

			void Attr(const std::wstring& name, const std::wstring& value)
			{
				buffer_ += L' ';
				buffer_ += name;
				buffer_ += L"=\"";
				AppendEscaped(value);
				buffer_ += L'"';
			}

			// Integer attributes are the hot path (one per source line). Their
			// decimal text never contains a character that needs escaping, so we
			// format straight into the buffer and skip both the std::to_wstring
			// and the escape scan. fmt is locale-independent, so the output
			// matches std::to_wstring byte-for-byte. Constrained to integral
			// types so string-literal values still pick the overload above;
			// bool is excluded so it can never format as true/false instead
			// of 1/0.
			template <std::integral T>
			    requires(!std::same_as<T, bool>)
			void Attr(const std::wstring& name, T value)
			{
				fmt::format_to(std::back_inserter(buffer_),
				               L" {}=\"{}\"",
				               name,
				               value);
			}

			// Close the opening tag as a self-closed, empty element.
			void CloseEmpty()
			{
				buffer_ += L"/>\n";
			}

			// Close the opening tag; the element has children.
			void CloseWithChildren()
			{
				buffer_ += L">\n";
				++depth_;
			}

			// Emit text content on the same line and close: <name>text</name>.
			void CloseWithText(const std::wstring& name, const std::wstring& text)
			{
				buffer_ += L'>';
				AppendEscaped(text);
				buffer_ += L"</";
				buffer_ += name;
				buffer_ += L">\n";
			}

			void CloseTag(const std::wstring& name)
			{
				--depth_;
				Indent();
				buffer_ += L"</";
				buffer_ += name;
				buffer_ += L">\n";
			}

		  private:
			void Indent()
			{
				buffer_.append(static_cast<std::size_t>(depth_) * 2, L' ');
			}

			void AppendEscaped(const std::wstring& value)
			{
				for (auto c : value)
				{
					switch (c)
					{
						case L'&': buffer_ += L"&amp;"; break;
						case L'<': buffer_ += L"&lt;"; break;
						case L'>': buffer_ += L"&gt;"; break;
						case L'\'': buffer_ += L"&apos;"; break;
						case L'"': buffer_ += L"&quot;"; break;
						default: buffer_ += c; break;
					}
				}
			}

			std::wostream& ostr_;
			std::wstring buffer_;
			std::int32_t depth_ = 0;
		};

		//---------------------------------------------------------------------
		void WriteCoverageAttributes(XmlWriter& writer,
		                             const CppCoverage::CoverageRate& coverageRate)
		{
			writer.Attr(L"line-rate", FormatRate(coverageRate.GetRate()));
			if (coverageRate.GetTotalBranchCount() != 0)
				writer.Attr(L"branch-rate",
				            FormatRate(coverageRate.GetBranchRate()));
			else
				writer.Attr(L"branch-rate", L"0");
			writer.Attr(L"complexity", L"0");
		}

		//---------------------------------------------------------------------
		void WriteLineBranches(
		    XmlWriter& writer,
		    const std::vector<Plugin::BranchCoverage>& branches)
		{
			std::int32_t coveredOutcomes = 0;
			std::int32_t totalOutcomes = 0;
			for (const auto& branch : branches)
			{
				// A conditional branch has two outcomes: taken and fall through.
				totalOutcomes += 2;
				coveredOutcomes += GetCoveredOutcomeCount(branch);
			}

			writer.Attr(L"branch", L"true");
			writer.Attr(L"condition-coverage",
			            fmt::format(L"{} ({}/{})",
			                        GetPercentString(coveredOutcomes,
			                                         totalOutcomes),
			                        coveredOutcomes,
			                        totalOutcomes));
			writer.CloseWithChildren();

			writer.OpenTag(L"conditions");
			writer.CloseWithChildren();
			for (const auto& branch : branches)
			{
				writer.OpenTag(L"condition");
				writer.Attr(L"number", branch.GetBranchIndex());
				writer.Attr(L"type", L"jump");
				writer.Attr(
				    L"coverage",
				    GetPercentString(GetCoveredOutcomeCount(branch), 2));
				writer.CloseEmpty();
			}
			writer.CloseTag(L"conditions");
		}

		//---------------------------------------------------------------------
		void WriteFile(XmlWriter& writer,
		               const CppCoverage::CoverageRateComputer& coverageRateComputer,
		               const Plugin::FileCoverage& file)
		{
			const auto& path = file.GetPath();

			writer.OpenTag(L"class");
			writer.Attr(L"name", ToUft8WString(path.filename()));
			writer.Attr(L"filename", ToUft8WString(path.relative_path()));
			WriteCoverageAttributes(writer,
			                        coverageRateComputer.GetCoverageRate(file));
			writer.CloseWithChildren();

			writer.OpenTag(L"methods");
			writer.CloseEmpty();

			const auto& lines = file.GetLineCollection();
			if (lines.empty())
			{
				writer.OpenTag(L"lines");
				writer.CloseEmpty();
			}
			else
			{
				writer.OpenTag(L"lines");
				writer.CloseWithChildren();
				for (const auto& [lineNumber, line] : lines)
				{
					writer.OpenTag(L"line");
					writer.Attr(L"number", lineNumber);
					writer.Attr(L"hits", line.HasBeenExecuted() ? 1 : 0);

					const auto& branches = line.GetBranches();
					if (branches.empty())
						writer.CloseEmpty();
					else
					{
						WriteLineBranches(writer, branches);
						writer.CloseTag(L"line");
					}
				}
				writer.CloseTag(L"lines");
			}

			writer.CloseTag(L"class");
		}

		//---------------------------------------------------------------------
		void WriteSources(XmlWriter& writer,
		                  const Plugin::CoverageData& coverageData)
		{
			std::unordered_set<std::wstring> rootPaths;
			for (const auto& module : coverageData.GetModules())
			{
				for (const auto& file : module->GetFiles())
					rootPaths.insert(file->GetPath().root_name().wstring());
			}

			if (rootPaths.empty())
			{
				writer.OpenTag(L"sources");
				writer.CloseEmpty();
				return;
			}

			writer.OpenTag(L"sources");
			writer.CloseWithChildren();
			for (const auto& rootPath : rootPaths)
			{
				writer.OpenTag(L"source");
				if (rootPath.empty())
					writer.CloseEmpty();
				else
					writer.CloseWithText(L"source", rootPath);
			}
			writer.CloseTag(L"sources");
		}

		//---------------------------------------------------------------------
		bool HasModuleWithFiles(const Plugin::CoverageData& coverageData)
		{
			for (const auto& module : coverageData.GetModules())
			{
				if (!module->GetFiles().empty())
					return true;
			}
			return false;
		}

		//---------------------------------------------------------------------
		void WritePackages(XmlWriter& writer,
		                   const CppCoverage::CoverageRateComputer& coverageRateComputer,
		                   const Plugin::CoverageData& coverageData)
		{
			if (!HasModuleWithFiles(coverageData))
			{
				writer.OpenTag(L"packages");
				writer.CloseEmpty();
				return;
			}

			writer.OpenTag(L"packages");
			writer.CloseWithChildren();

			for (const auto& module : coverageData.GetModules())
			{
				// Do not create a package with no files: the coverage would
				// not be visible by module.
				if (module->GetFiles().empty())
					continue;

				writer.OpenTag(L"package");
				writer.Attr(L"name", ToUft8WString(module->GetPath()));
				WriteCoverageAttributes(
				    writer, coverageRateComputer.GetCoverageRate(*module));
				writer.CloseWithChildren();

				writer.OpenTag(L"classes");
				writer.CloseWithChildren();
				for (const auto& file : module->GetFiles())
					WriteFile(writer, coverageRateComputer, *file);
				writer.CloseTag(L"classes");

				writer.CloseTag(L"package");
			}

			writer.CloseTag(L"packages");
		}

		//---------------------------------------------------------------------
		void WriteCoverage(XmlWriter& writer,
		                   const Plugin::CoverageData& coverageData)
		{
			CppCoverage::CoverageRateComputer coverageRateComputer(coverageData);
			const auto& coverageRate = coverageRateComputer.GetCoverageRate();

			auto now = std::chrono::system_clock::now();
			auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
			                     now.time_since_epoch())
			                     .count();

			writer.OpenTag(L"coverage");
			WriteCoverageAttributes(writer, coverageRate);
			writer.Attr(L"branches-covered",
			            coverageRate.GetCoveredBranchCount());
			writer.Attr(L"branches-valid",
			            coverageRate.GetTotalBranchCount());
			writer.Attr(L"timestamp", timestamp);
			writer.Attr(L"lines-covered",
			            coverageRate.GetExecutedLinesCount());
			writer.Attr(L"lines-valid", coverageRate.GetTotalLinesCount());
			writer.Attr(L"version", L"0");
			writer.CloseWithChildren();

			WriteSources(writer, coverageData);
			WritePackages(writer, coverageRateComputer, coverageData);

			writer.CloseTag(L"coverage");
		}
	}

	//-------------------------------------------------------------------------
	CoberturaExporter::CoberturaExporter() = default;

	//-------------------------------------------------------------------------
	std::filesystem::path CoberturaExporter::GetDefaultPath(const std::wstring& prefix) const
	{
		std::filesystem::path path{ prefix };

		path += "Coverage.xml";

		return path;
	}

	//-------------------------------------------------------------------------
	void CoberturaExporter::Export(
		const Plugin::CoverageData& coverageData,
		const std::filesystem::path& output)
	{
		Tools::CreateParentFolderIfNeeded(output);
		std::wofstream ofs{ output.string().c_str() };

		if (!ofs)
			throw InvalidOutputFileException(output, "cobertura");
		Export(coverageData, ofs);
		Tools::ShowOutputMessage(L"Cobertura report generated: ", output);
	}

	//-------------------------------------------------------------------------
	void CoberturaExporter::Export(
		const Plugin::CoverageData& coverageData,
		std::wostream& ostream) const
	{
		XmlWriter writer{ ostream };
		WriteCoverage(writer, coverageData);
		writer.Flush();
	}
}
