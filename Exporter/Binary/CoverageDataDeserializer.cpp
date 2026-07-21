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

// Protobuf and abseil headers raise conversion warnings when their
// templates are instantiated, which happens after any warning(pop).
#pragma warning(disable : 4018 4244 4267)

#include "CoverageDataDeserializer.hpp"

#include <fstream>

#include "CoverageData.pb.hpp"

#include "Plugin/Exporter/CoverageData.hpp"
#include "Plugin/Exporter/ModuleCoverage.hpp"
#include "Plugin/Exporter/FileCoverage.hpp"

#include "../ExporterException.hpp"

#include "Tools/Tool.hpp"

#include "CoverageDataSerializer.hpp"
#include "ColumnarCoverage.hpp"
#include "ProtoBuff.hpp"

namespace covpb = ProtoBuff;

namespace Exporter
{
	namespace
	{
		//---------------------------------------------------------------------
		void ReadMessage(
			google::protobuf::io::CodedInputStream& input,
			google::protobuf::MessageLite& message)
		{
			unsigned int size = 0;

			if (!input.ReadVarint32(&size))
				THROW(L"Cannot read message size.");
			auto limit = input.PushLimit(size);

			if (!message.ParseFromCodedStream(&input))
				THROW(L"Cannot parse message.");

			input.PopLimit(limit);
		}

		//---------------------------------------------------------------------
		void InitCoverageDataFrom(
			google::protobuf::io::CodedInputStream&  input,
			const covpb::CoverageData& coverageDataProtoBuff,
			Plugin::CoverageData& coverageData)
		{
			auto moduleCount = coverageDataProtoBuff.modulecount();

			for (size_t i = 0; i < moduleCount; ++i)
			{
				covpb::ModuleCoverage moduleProtoBuff;

				ReadMessage(input, moduleProtoBuff);				
				auto& module = coverageData.AddModule(Tools::Utf8ToWString(moduleProtoBuff.path()));

				for (const auto& fileProtoBuff : moduleProtoBuff.files())
				{
					auto& file = module.AddFile(Tools::Utf8ToWString(fileProtoBuff.path()));

					for (const auto& line : fileProtoBuff.lines())
					{
						file.AddLine(line.linenumber(), line.hasbeenexecuted());

						for (const auto& branch : line.branches())
						{
							file.AddBranch(
							    line.linenumber(),
							    Plugin::BranchCoverage{
							        branch.branchindex(),
							        branch.hasbeentaken(),
							        branch.hasbeenfallenthrough()});
						}
					}
				}
			}
		}		

		//-------------------------------------------------------------------------
		Plugin::CoverageData DeserializeFromStream(
			std::istream& istr,
			const std::string& errorIfNotCorrectFormat)
		{
			google::protobuf::io::IstreamInputStream outputStream(&istr);
			google::protobuf::io::CodedInputStream  codedInputStream(&outputStream);

			unsigned int fileTypeId;
			if (!codedInputStream.ReadVarint32(&fileTypeId) || fileTypeId != CoverageDataSerializer::FileTypeId)
				throw std::runtime_error(errorIfNotCorrectFormat);

			covpb::CoverageData coverageDataProtoBuff;

			ReadMessage(codedInputStream, coverageDataProtoBuff);

			Plugin::CoverageData coverageData{
				Tools::Utf8ToWString(coverageDataProtoBuff.name()),
				coverageDataProtoBuff.exitcode() };

			InitCoverageDataFrom(codedInputStream, coverageDataProtoBuff, coverageData);

			return coverageData;
		}
	}
		
	//-------------------------------------------------------------------------
	Plugin::CoverageData CoverageDataDeserializer::Deserialize(
		const std::filesystem::path& path, 
		const std::string& errorIfNotCorrectFormat) const
	{
		// New files use the columnar v2 format; older files are protobuf v1.
		// The leading magic tells them apart.
		if (ColumnarBinary::IsColumnarFile(path))
			return ColumnarBinary::Read(path);

		std::ifstream ifs(path.string(), std::ios::binary);

		if (!ifs)
			THROW(L"Cannot open file " + path.wstring());
		return DeserializeFromStream(ifs, errorIfNotCorrectFormat);
	}
}
