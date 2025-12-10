#include <windows.h>
#include <winternl.h>  // Contains partial TEB/PEB definitions

#include <format>
#include <string_view> // For the 'sv' suffix (std::wstring_view)

#pragma comment(lib, "ntdll.lib")

using namespace std::literals;  // Для 's' и 'sv' суффиксов

extern "C" NTSTATUS WINAPI RtlGetVersion(PRTL_OSVERSIONINFOW);

static auto get_os_version()
{
	OSVERSIONINFOEX Info{ sizeof(Info) };
	
	if (RtlGetVersion(reinterpret_cast<PRTL_OSVERSIONINFOW>(&Info)) == 0)
	{
		return Info;
	}

	// Fallback: чтение напрямую из PEB
	struct peb_version
	{
		ULONG OSMajorVersion;
		ULONG OSMinorVersion;
		USHORT OSBuildNumber;
		USHORT OSCSDVersion;
		ULONG OSPlatformId;
	};

	constexpr auto VersionOffset =
#ifdef _WIN64
		0x0118
#else
		0x00A4
#endif
		;

	const auto Teb = NtCurrentTeb();

	const auto& PebVersion = static_cast<peb_version>(Teb->ProcessEnvironmentBlock, VersionOffset);

	Info.dwMajorVersion = PebVersion.OSMajorVersion;
	Info.dwMinorVersion = PebVersion.OSMinorVersion;
	Info.dwBuildNumber = PebVersion.OSBuildNumber;
	Info.dwPlatformId = PebVersion.OSPlatformId;

	return Info;
}