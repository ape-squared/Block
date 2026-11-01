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

	std::string dump_hint;
	{
		const HANDLE flag = CreateFileW(pending.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (flag != INVALID_HANDLE_VALUE)
		{
			char buf[1024] = {};
			DWORD read = 0;
			if (ReadFile(flag, buf, sizeof(buf) - 1, &read, nullptr) && read > 0)
				dump_hint.assign(buf, read);
			CloseHandle(flag);
		}
	}

	std::error_code ec;
	std::filesystem::remove(pending, ec);

	WCHAR viewer[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, viewer, MAX_PATH);
	const std::filesystem::path viewer_path(viewer);
	const std::string viewer_name = viewer_path.filename().u8string();

	const int choice = MessageBoxW(
		nullptr,
		L"Block detected a crash from a previous session.\n\n"
		L"A dump was saved under %LOCALAPPDATA%\\Block\\crashes\\.\n\n"
		L"Open the crash folder and a prefilled GitHub issue (version, viewer, dump path)?",
		L"Block — crash report",
		MB_YESNO | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST);

	if (choice == IDYES)
	{
		ShellExecuteW(nullptr, L"open", crash_dir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);

		auto url_encode = [](const std::string &s) {
			std::string out;
			out.reserve(s.size() * 3);
			for (unsigned char c : s)
			{
				if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
					out.push_back(static_cast<char>(c));
				else if (c == ' ')
					out += "%20";
				else if (c == '\n')
					out += "%0A";
				else
				{
					char hex[4];
					sprintf_s(hex, "%%%02X", c);
					out += hex;
				}
			}
			return out;
		};

		const std::string title = "Crash report — Block " BLOCK_VERSION_STRING;
		std::string body =
			"### Environment\n"
			"- Block version: " BLOCK_VERSION_STRING "\n"
			"- Viewer: " + viewer_name + "\n"
			"- Dump: " + (dump_hint.empty() ? std::string("%LOCALAPPDATA%\\Block\\crashes\\") : dump_hint) + "\n\n"
			"### Steps to reproduce\n"
			"1. ...\n\n"
			"### Notes\n"
			"Attach the matching `.dmp` + `.txt` from the crashes folder.\n";

		char url[4096];
		sprintf_s(url,
			"https://github.com/ape-squared/Block/issues/new?title=%s&body=%s",
			url_encode(title).c_str(),
			url_encode(body).c_str());
		ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
	}
}
