// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include "ScopedAction.hpp"

#include <optional>

#include "Tool.hpp"

#include "Log.hpp"

namespace Tools
{
	//-------------------------------------------------------------------------
	ScopedAction::ScopedAction(std::function<void()> action)
		: action_(action)
	{
	}

	//-------------------------------------------------------------------------
	ScopedAction::~ScopedAction()
	{
		auto error = Try([&]
		{
			action_();
		});

		if (error)
			LOG_ERROR("{}", *error);
	}

}
