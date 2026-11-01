/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Block: checks ape-squared/block latest.json (GitHub Pages) for product updates.
 */

#include "runtime.hpp"
#include <block_version.h>
#include <Windows.h>
#include <WinInet.h>
#include <cstring>
#include <string>

struct scoped_internet_handle
{
	scoped_internet_handle(HINTERNET handle) : handle(handle) {}
	~scoped_internet_handle() { InternetCloseHandle(handle); }

	operator HINTERNET() const { return handle; }

private:
	HINTERNET handle;
};

unsigned int reshade::runtime::s_latest_version[3] = {};
char reshade::runtime::s_update_url[512] = {};
bool reshade::runtime::s_update_available = false;

static bool parse_json_string_field(const char *json, const char *key, char *out, size_t out_len)
{
	if (json == nullptr || key == nullptr || out == nullptr || out_len == 0)
		return false;
	out[0] = '\0';

	std::string needle = "\"";
	needle += key;
	needle += "\"";
	const char *p = std::strstr(json, needle.c_str());
	if (p == nullptr)
		return false;
	p = std::strchr(p + needle.size(), ':');
	if (p == nullptr)
		return false;
	p++;
	while (*p == ' ' || *p == '\t')
		p++;
	if (*p != '"')
		return false;
	p++;
	size_t i = 0;
	while (*p != '\0' && *p != '"' && i + 1 < out_len)
	{
		if (*p == '\\' && p[1] != '\0')
			p++;
		out[i++] = *p++;
	}
	out[i] = '\0';
	return i > 0;
}

static bool parse_semver(const char *text, unsigned int out[3])
{
	out[0] = out[1] = out[2] = 0;
	if (text == nullptr || *text == '\0')
		return false;
	if (*text == 'v' || *text == 'V')
		text++;
	char *end = nullptr;
	out[0] = static_cast<unsigned int>(std::strtoul(text, &end, 10));
	if (end == nullptr || *end != '.')
		return false;
	out[1] = static_cast<unsigned int>(std::strtoul(end + 1, &end, 10));
	if (end == nullptr || *end != '.')
		return out[0] != 0 || out[1] != 0;
	out[2] = static_cast<unsigned int>(std::strtoul(end + 1, &end, 10));
	return true;
}

static int compare_semver(const unsigned int a[3], const unsigned int b[3])
{
	for (int i = 0; i < 3; ++i)
	{
		if (a[i] < b[i]) return -1;
		if (a[i] > b[i]) return 1;
	}
	return 0;
}

void reshade::runtime::check_for_update()
{
#if defined(NDEBUG) && !defined(RESHADE_TEST_APPLICATION)
	if (s_latest_version[0] != 0 || s_latest_version[1] != 0 || s_latest_version[2] != 0)
		return;

	const scoped_internet_handle handle = InternetOpen(TEXT("Block/" BLOCK_VERSION_STRING), INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
	if (handle == nullptr)
		return;

	const scoped_internet_handle request = InternetOpenUrl(handle, BLOCK_UPDATE_URL, nullptr, 0,
		INTERNET_FLAG_RELOAD | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE, 0);
	if (request == nullptr)
		return;

	DWORD timeout = 2000;
	InternetSetOption(request, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
	InternetSetOption(request, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

	char response_data[2048];
	DWORD total = 0;
	while (total + 1 < sizeof(response_data))
	{
		DWORD len = 0;
		if (!InternetReadFile(request, response_data + total, static_cast<DWORD>(sizeof(response_data) - 1 - total), &len) || len == 0)
			break;
		total += len;
	}
	if (total == 0)
		return;
	response_data[total] = '\0';

	char version_text[64] = {};
	if (!parse_json_string_field(response_data, "version", version_text, sizeof(version_text)))
		return;

	unsigned int remote[3] = {};
	if (!parse_semver(version_text, remote))
		return;

	s_latest_version[0] = remote[0];
	s_latest_version[1] = remote[1];
	s_latest_version[2] = remote[2];

	char url[sizeof(s_update_url)] = {};
	if (!parse_json_string_field(response_data, "url", url, sizeof(url)))
		std::strncpy(url, BLOCK_RELEASES_URL, sizeof(url) - 1);
	std::strncpy(s_update_url, url, sizeof(s_update_url) - 1);

	const unsigned int local[3] = { BLOCK_VERSION_MAJOR, BLOCK_VERSION_MINOR, BLOCK_VERSION_PATCH };
	s_update_available = compare_semver(local, remote) < 0;
#endif
}
