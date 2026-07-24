// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#pragma once

namespace TestHelper
{
	//-------------------------------------------------------------------------
	template <typename Value, typename CloseAction>
	class AutoClose
	{
	public:
		//---------------------------------------------------------------------
		AutoClose(Value value, Value invalidValue, CloseAction closeAction)
			: value_{ value }
			, closeAction_{ closeAction }
		{
			if (value_ == invalidValue)
				throw std::runtime_error("The value is invalid");
		}

		//---------------------------------------------------------------------
		~AutoClose()
		{
			closeAction_(value_);
		}

		//---------------------------------------------------------------------
		Value operator*()
		{
			return value_;
		}

	private:
		Value value_;
		CloseAction closeAction_;
	};

	//-------------------------------------------------------------------------
	template <typename Value, typename InvalidValue, typename CloseAction>
	AutoClose<Value, CloseAction> MakeAutoClose(
		Value value, 
		InvalidValue invalidValue,
		CloseAction closeAction)
	{
		return AutoClose<Value, CloseAction>(value, invalidValue, closeAction);
	}
}