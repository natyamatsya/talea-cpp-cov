// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include "StartInfo.hpp"

#include <filesystem>

#include "tools/Tool.hpp"
#include "TaleaCppCovException.hpp"

namespace fs = std::filesystem;

namespace TaleaCppCov
{	
	namespace
	{
		//-------------------------------------------------------------------------
		void CheckPathExists(const std::string& context, const fs::path& path)
		{
			if (!Tools::FileExists(path))
				throw std::runtime_error(context + " \"" + path.string() + "\" does not exist");
		}
	}

	//-------------------------------------------------------------------------
	StartInfo::StartInfo(const fs::path& path)
		: path_(path)
	{
		AddArgument(path.wstring());
	}

	//-------------------------------------------------------------------------
	StartInfo::StartInfo(StartInfo&& startInfo)
		: path_{ std::move(startInfo.path_) }
		, arguments_( std::move(startInfo.arguments_) )
		, workingDirectory_{ std::move(startInfo.workingDirectory_) }
	{
	}

	//-------------------------------------------------------------------------
	void StartInfo::SetWorkingDirectory(const fs::path& workingDirectory)
	{
		CheckPathExists("Working directory", workingDirectory);
		workingDirectory_ = workingDirectory;					
	}

	//-------------------------------------------------------------------------
	void StartInfo::AddArgument(const std::wstring& argument)
	{
		arguments_.push_back(argument);
	}

	//-------------------------------------------------------------------------
	const std::filesystem::path& StartInfo::GetPath() const
	{
		return path_;
	}

	//-------------------------------------------------------------------------
	const std::vector<std::wstring>& StartInfo::GetArguments() const
	{
		return arguments_;
	}

	//-------------------------------------------------------------------------
	const fs::path* StartInfo::GetWorkingDirectory() const
	{
		if (workingDirectory_)
			return &*workingDirectory_;
		return nullptr;
	}

	//-------------------------------------------------------------------------
	std::wostream& operator<<(std::wostream& ostr, const StartInfo& startInfo)
	{
		ostr << L"Path:" << startInfo.path_ << std::endl;
		ostr << L"Arguments:";

		const auto& arguments = startInfo.arguments_;

		for (size_t i = 1; i < arguments.size(); ++i)
			ostr << arguments[i] << " ";
		ostr << std::endl;
		ostr << L"Working directory: ";
		
		if (startInfo.workingDirectory_)
			ostr << *startInfo.workingDirectory_;
		else
			ostr << L"not set.";
		return ostr;
	}
}
