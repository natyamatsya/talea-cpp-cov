// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2019 OpenCppCoverage

#pragma once

#include "tools/Log.hpp"
#include "tools/Tool.hpp"

#include "tools/Error.hpp"
#include <optional>

namespace TaleaCppCov
{
	template <typename T_Handle, typename T_Releaser>
	class Handle
	{
	  public:
		//---------------------------------------------------------------------
		// A valid handle and releaser are an invariant of the type, so it is
		// built through a factory that reports the violation as an error rather
		// than a throwing constructor.
		static Tools::Expected<Handle> Create(T_Handle handle,
		                                       T_Releaser releaser)
		{
			if (!handle)
				return std::unexpected(MAKE_ERROR(L"Handle is not valid"));
			if (!releaser)
				return std::unexpected(MAKE_ERROR(L"Releaser is not valid"));
			return Handle{handle, releaser};
		}

		//---------------------------------------------------------------------
		// Moving transfers ownership: the source is emptied so only the
		// destination releases the handle. A defaulted move would leave the
		// source's handle_ set and double-release - harmless when CreateHandle
		// returned a Handle by value (guaranteed elision, never moved), but the
		// factory now returns it inside a Tools::Expected, which really moves.
		Handle(Handle&& other) noexcept
		    : handle_(other.handle_), releaser_(other.releaser_)
		{
			other.handle_ = T_Handle{};
		}

		//---------------------------------------------------------------------
		~Handle()
		{
			Tools::Try([&] {
				if (handle_ && !releaser_(handle_))
				{
					LOG_ERROR("Cannot release handler");
				}
			});
		}

		//---------------------------------------------------------------------
		const T_Handle& GetValue() const
		{
			return handle_;
		}

	  private:
		//---------------------------------------------------------------------
		Handle(T_Handle handle, T_Releaser releaser)
		    : handle_(handle), releaser_(releaser)
		{
		}

		Handle(const Handle&) = delete;
		Handle& operator=(const Handle&) = delete;
		Handle& operator=(Handle&&) = delete;

	  private:
		T_Handle handle_;
		T_Releaser releaser_;
	};

	//-------------------------------------------------------------------------
	template <typename T_Handle, typename T_Releaser>
	Tools::Expected<Handle<T_Handle, T_Releaser>>
	CreateHandle(T_Handle handle, T_Releaser releaser)
	{
		return Handle<T_Handle, T_Releaser>::Create(handle, releaser);
	}
}
