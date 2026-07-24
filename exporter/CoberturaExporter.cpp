// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>

#include <charconv>
#include <chrono>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>

#include <fmt/format.h>

#include "CoberturaExporter.hpp"
#include "coverage/CoverageData.hpp"
#include "coverage/ModuleCoverage.hpp"
#include "coverage/FileCoverage.hpp"
#include "coverage/LineCoverage.hpp"
#include "talea-cpp-cov/CoverageRateComputer.hpp"
#include "InvalidOutputFileException.hpp"

#include "tools/Tool.hpp"

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
		std::string FormatRate(double value)
		{
			// std::to_chars with general format + max_digits10 precision
			// reproduces the old wostringstream(classic locale, precision
			// max_digits10, defaultfloat) output byte-for-byte, but is
			// locale-free, allocation-free and needs no stream. This is the
			// last string stream on the Cobertura export path.
			char buffer[32];
			auto [ptr, ec] = std::to_chars(
			    buffer,
			    buffer + sizeof(buffer),
			    value,
			    std::chars_format::general,
			    std::numeric_limits<double>::max_digits10);
			return std::string(buffer, ptr);
		}

		//---------------------------------------------------------------------
		std::string ToUtf8(const fs::path& path)
		{
			return Tools::ToUtf8String(path.wstring());
		}

		//---------------------------------------------------------------------
		std::int32_t GetCoveredOutcomeCount(const Coverage::BranchCoverage& branch)
		{
			return (branch.HasBeenTaken() ? 1 : 0) +
			       (branch.HasBeenFallenThrough() ? 1 : 0);
		}

		//---------------------------------------------------------------------
		std::string GetPercentString(std::int32_t covered, std::int32_t total)
		{
			auto percent = (total == 0) ? 0 : (100 * covered) / total;
			return fmt::format("{}%", percent);
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
			explicit XmlWriter(std::ostream& ostr) : ostr_{ostr}
			{
				buffer_.reserve(64 * 1024);
				buffer_ += "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
			}

			// Write the accumulated document to the stream. Called explicitly
			// so an I/O error propagates as an exception (not from a dtor).
			void Flush()
			{
				ostr_.write(buffer_.data(),
				            static_cast<std::streamsize>(buffer_.size()));
				buffer_.clear();
			}

			void OpenTag(std::string_view name)
			{
				Indent();
				buffer_ += '<';
				buffer_ += name;
			}

			void Attr(std::string_view name, std::string_view value)
			{
				buffer_ += ' ';
				buffer_ += name;
				buffer_ += "=\"";
				AppendEscaped(value);
				buffer_ += '"';
			}

			// Integer attributes are the hot path (one per source line). Their
			// decimal text never contains a character that needs escaping, so we
			// format straight into the buffer and skip the escape scan.
			// std::to_chars is locale-independent (matches std::to_string /
			// to_wstring byte-for-byte), allocation-free and needs no
			// format-string parse. Constrained to integral types so
			// string-literal values still pick the overload above; bool is
			// excluded so it can never format as true/false instead of 1/0.
			template <std::integral T>
			    requires(!std::same_as<T, bool>)
			void Attr(std::string_view name, T value)
			{
				buffer_ += ' ';
				buffer_ += name;
				buffer_ += "=\"";
				char digits[24]; // >= digits of any 64-bit integer plus sign
				auto [ptr, ec] =
				    std::to_chars(digits, digits + sizeof(digits), value);
				buffer_.append(digits, ptr);
				buffer_ += '"';
			}

			// Close the opening tag as a self-closed, empty element.
			void CloseEmpty()
			{
				buffer_ += "/>\n";
			}

			// Close the opening tag; the element has children.
			void CloseWithChildren()
			{
				buffer_ += ">\n";
				++depth_;
			}

			// Emit text content on the same line and close: <name>text</name>.
			void CloseWithText(std::string_view name, std::string_view text)
			{
				buffer_ += '>';
				AppendEscaped(text);
				buffer_ += "</";
				buffer_ += name;
				buffer_ += ">\n";
			}

			void CloseTag(std::string_view name)
			{
				--depth_;
				Indent();
				buffer_ += "</";
				buffer_ += name;
				buffer_ += ">\n";
			}

		  private:
			void Indent()
			{
				buffer_.append(static_cast<std::size_t>(depth_) * 2, ' ');
			}

			void AppendEscaped(std::string_view value)
			{
				for (auto c : value)
				{
					switch (c)
					{
						case '&': buffer_ += "&amp;"; break;
						case '<': buffer_ += "&lt;"; break;
						case '>': buffer_ += "&gt;"; break;
						case '\'': buffer_ += "&apos;"; break;
						case '"': buffer_ += "&quot;"; break;
						default: buffer_ += c; break;
					}
				}
			}

			std::ostream& ostr_;
			std::string buffer_;
			std::int32_t depth_ = 0;
		};

		//---------------------------------------------------------------------
		void WriteCoverageAttributes(XmlWriter& writer,
		                             const TaleaCppCov::CoverageRate& coverageRate)
		{
			writer.Attr("line-rate", FormatRate(coverageRate.GetRate()));
			if (coverageRate.GetTotalBranchCount() != 0)
				writer.Attr("branch-rate",
				            FormatRate(coverageRate.GetBranchRate()));
			else
				writer.Attr("branch-rate", "0");
			writer.Attr("complexity", "0");
		}

		//---------------------------------------------------------------------
		void WriteLineBranches(
		    XmlWriter& writer,
		    const std::vector<Coverage::BranchCoverage>& branches)
		{
			std::int32_t coveredOutcomes = 0;
			std::int32_t totalOutcomes = 0;
			for (const auto& branch : branches)
			{
				// A conditional branch has two outcomes: taken and fall through.
				totalOutcomes += 2;
				coveredOutcomes += GetCoveredOutcomeCount(branch);
			}

			writer.Attr("branch", "true");
			writer.Attr("condition-coverage",
			            fmt::format("{} ({}/{})",
			                        GetPercentString(coveredOutcomes,
			                                         totalOutcomes),
			                        coveredOutcomes,
			                        totalOutcomes));
			writer.CloseWithChildren();

			writer.OpenTag("conditions");
			writer.CloseWithChildren();
			for (const auto& branch : branches)
			{
				writer.OpenTag("condition");
				writer.Attr("number", branch.GetBranchIndex());
				writer.Attr("type", "jump");
				writer.Attr(
				    "coverage",
				    GetPercentString(GetCoveredOutcomeCount(branch), 2));
				writer.CloseEmpty();
			}
			writer.CloseTag("conditions");
		}

		//---------------------------------------------------------------------
		void WriteFile(XmlWriter& writer,
		               const TaleaCppCov::CoverageRateComputer& coverageRateComputer,
		               const Coverage::FileCoverage& file)
		{
			const auto& path = file.GetPath();

			writer.OpenTag("class");
			writer.Attr("name", ToUtf8(path.filename()));
			writer.Attr("filename", ToUtf8(path.relative_path()));
			WriteCoverageAttributes(writer,
			                        coverageRateComputer.GetCoverageRate(file));
			writer.CloseWithChildren();

			writer.OpenTag("methods");
			writer.CloseEmpty();

			const auto& lines = file.GetLineCollection();
			if (lines.empty())
			{
				writer.OpenTag("lines");
				writer.CloseEmpty();
			}
			else
			{
				writer.OpenTag("lines");
				writer.CloseWithChildren();
				for (const auto& [lineNumber, line] : lines)
				{
					writer.OpenTag("line");
					writer.Attr("number", lineNumber);
					writer.Attr("hits", line.HasBeenExecuted() ? 1 : 0);

					const auto& branches = line.GetBranches();
					if (branches.empty())
						writer.CloseEmpty();
					else
					{
						WriteLineBranches(writer, branches);
						writer.CloseTag("line");
					}
				}
				writer.CloseTag("lines");
			}

			writer.CloseTag("class");
		}

		//---------------------------------------------------------------------
		void WriteSources(XmlWriter& writer,
		                  const Coverage::CoverageData& coverageData)
		{
			std::unordered_set<std::string> rootPaths;
			for (const auto& module : coverageData.GetModules())
			{
				for (const auto& file : module->GetFiles())
					rootPaths.insert(ToUtf8(file->GetPath().root_name()));
			}

			if (rootPaths.empty())
			{
				writer.OpenTag("sources");
				writer.CloseEmpty();
				return;
			}

			writer.OpenTag("sources");
			writer.CloseWithChildren();
			for (const auto& rootPath : rootPaths)
			{
				writer.OpenTag("source");
				if (rootPath.empty())
					writer.CloseEmpty();
				else
					writer.CloseWithText("source", rootPath);
			}
			writer.CloseTag("sources");
		}

		//---------------------------------------------------------------------
		bool HasModuleWithFiles(const Coverage::CoverageData& coverageData)
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
		                   const TaleaCppCov::CoverageRateComputer& coverageRateComputer,
		                   const Coverage::CoverageData& coverageData)
		{
			if (!HasModuleWithFiles(coverageData))
			{
				writer.OpenTag("packages");
				writer.CloseEmpty();
				return;
			}

			writer.OpenTag("packages");
			writer.CloseWithChildren();

			for (const auto& module : coverageData.GetModules())
			{
				// Do not create a package with no files: the coverage would
				// not be visible by module.
				if (module->GetFiles().empty())
					continue;

				writer.OpenTag("package");
				writer.Attr("name", ToUtf8(module->GetPath()));
				WriteCoverageAttributes(
				    writer, coverageRateComputer.GetCoverageRate(*module));
				writer.CloseWithChildren();

				writer.OpenTag("classes");
				writer.CloseWithChildren();
				for (const auto& file : module->GetFiles())
					WriteFile(writer, coverageRateComputer, *file);
				writer.CloseTag("classes");

				writer.CloseTag("package");
			}

			writer.CloseTag("packages");
		}

		//---------------------------------------------------------------------
		void WriteCoverage(XmlWriter& writer,
		                   const Coverage::CoverageData& coverageData)
		{
			TaleaCppCov::CoverageRateComputer coverageRateComputer(coverageData);
			const auto& coverageRate = coverageRateComputer.GetCoverageRate();

			auto now = std::chrono::system_clock::now();
			auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
			                     now.time_since_epoch())
			                     .count();

			writer.OpenTag("coverage");
			WriteCoverageAttributes(writer, coverageRate);
			writer.Attr("branches-covered",
			            coverageRate.GetCoveredBranchCount());
			writer.Attr("branches-valid",
			            coverageRate.GetTotalBranchCount());
			writer.Attr("timestamp", timestamp);
			writer.Attr("lines-covered",
			            coverageRate.GetExecutedLinesCount());
			writer.Attr("lines-valid", coverageRate.GetTotalLinesCount());
			writer.Attr("version", "0");
			writer.CloseWithChildren();

			WriteSources(writer, coverageData);
			WritePackages(writer, coverageRateComputer, coverageData);

			writer.CloseTag("coverage");
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
		const Coverage::CoverageData& coverageData,
		const std::filesystem::path& output)
	{
		Tools::ThrowOnError(Tools::CreateParentFolderIfNeeded(output));
		std::ofstream ofs{ output.string().c_str() };

		if (!ofs)
			throw InvalidOutputFileException(output, "cobertura");
		Export(coverageData, ofs);
		Tools::ShowOutputMessage(L"Cobertura report generated: ", output);
	}

	//-------------------------------------------------------------------------
	void CoberturaExporter::Export(
		const Coverage::CoverageData& coverageData,
		std::ostream& ostream) const
	{
		XmlWriter writer{ ostream };
		WriteCoverage(writer, coverageData);
		writer.Flush();
	}
}
