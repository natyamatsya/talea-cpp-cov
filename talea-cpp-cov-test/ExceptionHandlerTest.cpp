// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>

#include "talea-cpp-cov/ExceptionHandler.hpp"
#include "talea-cpp-cov/Debugger.hpp"
#include "talea-cpp-cov/StartInfo.hpp"
#include "talea-cpp-cov/IDebugEventsHandler.hpp"

#include "test-coverage-console/TestCoverageConsole.hpp"

namespace cov = TaleaCppCov;

namespace TaleaCppCovTest
{

	namespace
	{
		//---------------------------------------------------------------------
		class ExceptionHandlerTest : public ::testing::Test, private TaleaCppCov::IDebugEventsHandler
		{
		public:
			//---------------------------------------------------------------------
			void Run(const std::wstring& commandLineArgument)
			{
				cov::StartInfo startInfo{ TestCoverageConsole::GetOutputBinaryPath() };
				cov::Debugger debugger{ false, false, false };

				startInfo.AddArgument(commandLineArgument);
				[[maybe_unused]] auto exitCode =
				    Tools::ValueOrThrow(debugger.Debug(startInfo, *this));
			}

			//---------------------------------------------------------------------
			virtual ExceptionType OnException(HANDLE hProcess, HANDLE hThread, const EXCEPTION_DEBUG_INFO& exceptionDebugInfo) override
			{							
				std::wostringstream ostr;

				exceptionHandlerStatus_ = handler_.HandleException(hProcess, exceptionDebugInfo, ostr);
				message_ = ostr.str();

				return IDebugEventsHandler::ExceptionType::NotHandled;;
			}
			
			//-----------------------------------------------------------------
			std::wstring HandleExceptionTwice(std::int32_t errorCode, bool firstChange)
			{
				auto exceptionDebugInfo = CreateExceptionDebugInfo(errorCode, firstChange);

				handler_.HandleException(nullptr, exceptionDebugInfo, ostr_); // skip first exception
				handler_.HandleException(nullptr, exceptionDebugInfo, ostr_);

				return ostr_.str();
			};

			//-----------------------------------------------------------------------------
			EXCEPTION_DEBUG_INFO CreateExceptionDebugInfo(
				std::int32_t errorCode = cov::ExceptionHandler::ExceptionEmulationX86ErrorCode,
				bool firstChange = true)
			{
				EXCEPTION_DEBUG_INFO exceptionDebugInfo{};

				exceptionDebugInfo.dwFirstChance = firstChange ? 1 : 0;
				exceptionDebugInfo.ExceptionRecord.ExceptionCode = errorCode;

				return exceptionDebugInfo;
			}

			TaleaCppCov::ExceptionHandlerStatus exceptionHandlerStatus_;
			std::wstring message_;
			cov::ExceptionHandler handler_;
			std::wostringstream ostr_;
		};
	}

	//-----------------------------------------------------------------------------
	TEST_F(ExceptionHandlerTest, TestUnHandleCppException)
	{
		Run(TestCoverageConsole::TestThrowUnHandledCppException);
		ASSERT_EQ(cov::ExceptionHandlerStatus::CppError, exceptionHandlerStatus_);
		ASSERT_NE(std::string::npos, message_.find(cov::ExceptionHandler::ExceptionCpp));
	}

	//-----------------------------------------------------------------------------
	TEST_F(ExceptionHandlerTest, TestUnHandleSEHException)
	{
		Run(TestCoverageConsole::TestThrowUnHandledSEHException);	
		ASSERT_EQ(TaleaCppCov::ExceptionHandlerStatus::Error, exceptionHandlerStatus_);
		ASSERT_NE(std::string::npos, message_.find(cov::ExceptionHandler::ExceptionAccesViolation));
	}
	
	//-----------------------------------------------------------------------------
	TEST_F(ExceptionHandlerTest, TestFormatMessage)
	{
		auto message = HandleExceptionTwice(cov::ExceptionHandler::ExceptionEmulationX86ErrorCode, false);
		ASSERT_NE(std::string::npos, message.find(L"Exception status code used by Win32 x86 emulation subsystem"));
	}

	//-----------------------------------------------------------------------------
	TEST_F(ExceptionHandlerTest, TestUnknown)
	{
		auto message = HandleExceptionTwice(42, false);
		ASSERT_NE(std::string::npos, message.find(cov::ExceptionHandler::ExceptionUnknown));
	}

	//-----------------------------------------------------------------------------
	TEST_F(ExceptionHandlerTest, ChildProcess)
	{
		auto exceptionDebugInfo = CreateExceptionDebugInfo();

		ASSERT_EQ(cov::ExceptionHandlerStatus::FirstChanceException,
			handler_.HandleException(nullptr, exceptionDebugInfo, ostr_));
		ASSERT_EQ(cov::ExceptionHandlerStatus::BreakPoint,
			handler_.HandleException(nullptr, exceptionDebugInfo, ostr_));
		ASSERT_EQ(cov::ExceptionHandlerStatus::FirstChanceException,
			handler_.HandleException(reinterpret_cast<HANDLE>(42), exceptionDebugInfo, ostr_));
	}

	//-----------------------------------------------------------------------------
	TEST_F(ExceptionHandlerTest, ExitProcess)
	{
		auto exceptionDebugInfo = CreateExceptionDebugInfo();
		const HANDLE handle = nullptr;
		const auto handle2 = reinterpret_cast<HANDLE>(42);
		ASSERT_EQ(cov::ExceptionHandlerStatus::FirstChanceException,
			handler_.HandleException(handle, exceptionDebugInfo, ostr_));
		ASSERT_EQ(cov::ExceptionHandlerStatus::FirstChanceException,
			handler_.HandleException(handle2, exceptionDebugInfo, ostr_));

		handler_.OnExitProcess(handle2);
		ASSERT_EQ(cov::ExceptionHandlerStatus::FirstChanceException,
			handler_.HandleException(handle2, exceptionDebugInfo, ostr_));

		handler_.OnExitProcess(handle);
		ASSERT_EQ(cov::ExceptionHandlerStatus::FirstChanceException,
			handler_.HandleException(handle, exceptionDebugInfo, ostr_));
	}
}