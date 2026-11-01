/*
 * Block crash dumps and next-launch report prompt.
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "block_crash_handler.hpp"
#include "block_version.h"
#include "dll_log.hpp"

#include <Windows.h>
#include <Shellapi.h>
#include <DbgHelp.h>
#include <ShlObj.h>
#include <filesystem>
#include <string>

namespace
{
	PVOID g_veh = nullptr;
	std::filesystem::path g_crash_dir;
	volatile LONG g_handling = 0;

	std::filesystem::path crash_dir()
	{
		if (!g_crash_dir.empty())
			return g_crash_dir;

		WCHAR local[MAX_PATH] = {};
		if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, local)))
			return g_crash_dir = L".";

		g_crash_dir = std::filesystem::path(local) / L"Block" / L"crashes";
		std::error_code ec;
		std::filesystem::create_directories(g_crash_dir, ec);
		return g_crash_dir;
	}

	bool should_ignore(DWORD code)
	{
		return code == CONTROL_C_EXIT ||
			code == 0x406D1388 /* SetThreadName */ ||
			code == DBG_PRINTEXCEPTION_C ||
			code == DBG_PRINTEXCEPTION_WIDE_C ||
			code == STATUS_BREAKPOINT ||
			code == 0xE0434352 /* CLR */ ||
			code == 0xE06D7363 /* MSVC C++ */ ||
			((code ^ 0xE24C4A00) <= 0xFF) /* LuaJIT */;
	}

	void write_report_sidecar(const std::filesystem::path &dump_path, DWORD code, void *address)
	{
		const auto txt = dump_path;
		std::filesystem::path report = dump_path;
		report.replace_extension(L".txt");

		char buf[2048];
		SYSTEMTIME st = {};
		GetLocalTime(&st);
		sprintf_s(buf,
			"Block crash report\r\n"
			"==================\r\n"
			"Version: %s\r\n"
			"Time: %04u-%02u-%02u %02u:%02u:%02u\r\n"
			"Exception: 0x%08lX at %p\r\n"
			"Dump: %s\r\n"
			"\r\n"
			"Attach this .txt and the .dmp when filing an issue:\r\n"
			"https://github.com/ape-squared/Block/issues/new\r\n",
			BLOCK_VERSION_STRING,
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
			static_cast<unsigned long>(code),
			address,
			dump_path.u8string().c_str());

		const HANDLE file = CreateFileW(report.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE)
			return;
		DWORD written = 0;
		WriteFile(file, buf, static_cast<DWORD>(strlen(buf)), &written, nullptr);
		CloseHandle(file);

		// Flag for next launch prompt
		const auto pending = crash_dir() / L"pending_report";
		const HANDLE flag = CreateFileW(pending.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (flag != INVALID_HANDLE_VALUE)
		{
			const std::string name = report.u8string();
			WriteFile(flag, name.data(), static_cast<DWORD>(name.size()), &written, nullptr);
			CloseHandle(flag);
		}
	}

	LONG WINAPI block_exception_filter(PEXCEPTION_POINTERS ex)
	{
		if (ex == nullptr || ex->ExceptionRecord == nullptr)
			return EXCEPTION_CONTINUE_SEARCH;

		const DWORD code = ex->ExceptionRecord->ExceptionCode;
		if (should_ignore(code))
			return EXCEPTION_CONTINUE_SEARCH;

		if (InterlockedCompareExchange(&g_handling, 1, 0) != 0)
			return EXCEPTION_CONTINUE_SEARCH;

		const auto dbghelp = LoadLibraryW(L"dbghelp.dll");
		if (dbghelp == nullptr)
		{
			InterlockedExchange(&g_handling, 0);
			return EXCEPTION_CONTINUE_SEARCH;
		}

		using MiniDumpWriteDump_t = BOOL(WINAPI *)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
			PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
		const auto write_dump = reinterpret_cast<MiniDumpWriteDump_t>(GetProcAddress(dbghelp, "MiniDumpWriteDump"));
		if (write_dump == nullptr)
		{
			InterlockedExchange(&g_handling, 0);
			return EXCEPTION_CONTINUE_SEARCH;
		}

		SYSTEMTIME st = {};
		GetLocalTime(&st);
		wchar_t name[64];
		swprintf_s(name, L"block-%04u%02u%02u-%02u%02u%02u.dmp",
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

		const std::filesystem::path dump_path = crash_dir() / name;
		const HANDLE file = CreateFileW(dump_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file != INVALID_HANDLE_VALUE)
		{
			MINIDUMP_EXCEPTION_INFORMATION info = {};
			info.ThreadId = GetCurrentThreadId();
			info.ExceptionPointers = ex;
			info.ClientPointers = FALSE;

			if (write_dump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal, &info, nullptr, nullptr))
			{
				reshade::log::message(reshade::log::level::error,
					"Block wrote crash dump to '%s' (exception 0x%08lX).",
					dump_path.u8string().c_str(),
					static_cast<unsigned long>(code));
				write_report_sidecar(dump_path, code, ex->ExceptionRecord->ExceptionAddress);
			}
			CloseHandle(file);
		}

		InterlockedExchange(&g_handling, 0);
		return EXCEPTION_CONTINUE_SEARCH;
	}
}

void block::install_crash_handler()
{
	crash_dir(); // ensure folder exists
	if (g_veh == nullptr)
		g_veh = AddVectoredExceptionHandler(1, block_exception_filter);
	SetUnhandledExceptionFilter([](PEXCEPTION_POINTERS ex) -> LONG {
		block_exception_filter(ex);
		return EXCEPTION_CONTINUE_SEARCH;
	});
}

void block::uninstall_crash_handler()
{
	if (g_veh != nullptr)
	{
		RemoveVectoredExceptionHandler(g_veh);
		g_veh = nullptr;
	}
}

void block::prompt_pending_crash_reports()
{
	const auto pending = crash_dir() / L"pending_report";
	if (!std::filesystem::exists(pending))
		return;

	std::error_code ec;
	std::filesystem::remove(pending, ec);

	const int choice = MessageBoxW(
		nullptr,
		L"Block detected a crash from a previous session.\n\n"
		L"A dump was saved under %LOCALAPPDATA%\\Block\\crashes\\.\n\n"
		L"Open the crash folder and GitHub issues so you can attach the dump?",
		L"Block — crash report",
		MB_YESNO | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST);

	if (choice == IDYES)
	{
		ShellExecuteW(nullptr, L"open", crash_dir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);

		char url[512];
		sprintf_s(url,
			"https://github.com/ape-squared/Block/issues/new?title=Crash%%20report&body=Block%%20version%%3A%%20%s%%0A%%0ASteps%%3A%%0A1.%%20...%%0A%%0AAttach%%20the%%20.dmp%%20%%2B%%20.txt%%20from%%20%%25LOCALAPPDATA%%25%%5CBlock%%5Ccrashes%%5C",
			BLOCK_VERSION_STRING);
		ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
	}
}
