// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"

#include "talea-cpp-cov/StartInfo.hpp"
#include "talea-cpp-cov/Debugger.hpp"
#include "test-coverage-console/TestCoverageConsole.hpp"

#include "DebugEventsMock.hpp"

namespace cov = TaleaCppCov;

namespace TaleaCppCovTest
{
	//-----------------------------------------------------------------------------
	TEST(DebugerTest, Debug)
	{		
		cov::StartInfo startInfo{ TestCoverageConsole::GetOutputBinaryPath() };
		cov::Debugger debugger{ false, false, false };
		DebugEventsHandlerMock debugEventsHandlerMock;

		EXPECT_CALL(debugEventsHandlerMock, OnCreateProcess(testing::_));
		EXPECT_CALL(debugEventsHandlerMock, OnExitProcess(testing::_, testing::_, testing::_));
		EXPECT_CALL(debugEventsHandlerMock, OnLoadDll(testing::_, testing::_, testing::_)).Times(testing::AnyNumber());
		EXPECT_CALL(debugEventsHandlerMock, OnUnloadDll(testing::_, testing::_, testing::_)).Times(testing::AnyNumber());
		EXPECT_CALL(debugEventsHandlerMock, OnException(testing::_, testing::_, testing::_))
			.WillRepeatedly(testing::Return(cov::IDebugEventsHandler::ExceptionType::NotHandled));

		[[maybe_unused]] auto exitCode =
		    Tools::ValueOrThrow(debugger.Debug(startInfo, debugEventsHandlerMock));
		ASSERT_EQ(0, debugger.GetRunningProcesses());
		ASSERT_EQ(0, debugger.GetRunningThreads());
	}	
}