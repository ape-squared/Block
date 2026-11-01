#pragma once

// Block crash dumps + pending-report prompt (Release and Debug).

namespace block
{
	/// Install SEH/VEH handler that writes minidumps under %LOCALAPPDATA%\Block\crashes\.
	void install_crash_handler();

	/// Remove the handler (DLL unload).
	void uninstall_crash_handler();

	/// If a previous run left a crash dump, offer to open the folder + GitHub issue form.
	void prompt_pending_crash_reports();
}
