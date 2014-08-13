// Fixes PROCESS_ALL_ACCESS for Windows XP
#define _WIN32_WINNT 0x501
#define WIN32_LEAN_AND_MEAN

#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include <Windows.h>
#include <ShellAPI.h>		// for CommandLinetoArgvW

#include "Common.h"			// for LazyTypedefs, SleepFor, Globals
#include "Networking.h"		// for MSG_COUNT
#include "PacketHandler.h"	// for RemoteAddress
#include "Application.h"	// for Application

#include "Initialize.h"

// Prototypes
void hprint(std::string text, std::chrono::milliseconds sleep);

// Namespaces
using namespace std;
using namespace chrono;

// Globals
int argc = 0;
wchar_t** argv = nullptr;

void __cdecl Init_t(const char *path)
{
	argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	thread mainThread(MainThread, argc, argv);
	mainThread.detach();
	return;
}

void MainThread(int argc, wchar_t** argv)
{
	bool isServer = false;
	uint timeout = 15000;

	PacketHandler::RemoteAddress Address;
	Application::Program* Program = nullptr;
	Application::Settings Settings = {};
	Application::ExitCode ExitCode;

#pragma region Command line arguments
	if (argc > 2)
	{
		for (int i = 1; i < argc; i++)
		{
			// Connection
			if ((wcscmp(argv[i], L"--host") == 0 || wcscmp(argv[i], L"-h") == 0) && (i + 1) < argc)
			{
				isServer = true;
				//netMode = "server";
				Address.port = _wtoi(argv[++i]);
			}
			else if ((wcscmp(argv[i], L"--connect") == 0 || wcscmp(argv[i], L"-c") == 0) && (i + 2) < argc)
			{
				isServer = false;
				
				wstring wstr = argv[++i];
				string sstr(wstr.begin(), wstr.end());

				Address.ip = sstr;
				cout << Address.ip << endl;
				Address.port = _wtoi(argv[++i]);
			}
			else if ((wcscmp(argv[i], L"--timeout") == 0 || wcscmp(argv[i], L"-t") == 0) && (i + 1) < argc)
			{
				if ((timeout = _wtoi(argv[++i])) < 1000)
					timeout = 1000;
			}

			// Configuration
			else if (wcscmp(argv[i], L"--no-specials") == 0)
				Settings.noSpecials = true;
			else if (wcscmp(argv[i], L"--local") == 0 || wcscmp(argv[i], L"-l") == 0)
				Settings.isLocal = true;
			else if (wcscmp(argv[i], L"--keep-active") == 0)
				Settings.KeepWindowActive = true;
		}
	}
	else
	{
		return;
	}
#pragma endregion


	PacketEx::SetMessageTypeCount(MSG_COUNT);
	sa2bn::Globals::ProcessID = GetCurrentProcess();
	sa2bn::Globals::Networking = new PacketHandler();
	Program = new Application::Program(Settings, isServer, Address);

	while (true)
	{

		if (Program->Connect() != Application::ExitCode::NotReady)
		{
			Program->ApplySettings();
			// This will run indefinitely unless something stops it from
			// the inside. Therefore, we delete immediately after.
			ExitCode = Program->RunLoop();

			bool result = Program->OnEnd();

			if (!result)
				break;
			else
				cout << "<> Reinitializing..." << endl;
		}

		SleepFor((milliseconds)250);
	}

	delete Program;
	delete sa2bn::Globals::Networking;
}

// Unnecessary haxy print
void hprint(std::string text, std::chrono::milliseconds sleep)
{
	for (unsigned int i = 0; i < text.length(); i++)
	{
		cout << text[i];
		SleepFor(sleep);
	}
}
