#include <SA2ModLoader.h>
#include <thread>			// for this_thread::yield
#include "Networking.h"		// for MSG
#include "Globals.h"
#include "OnStageChange.h"

void* SetCurrentLevel_ptr = (void*)0x0043D8A0;

void InitOnStageChange()
{
	WriteJump(SetCurrentLevel_ptr, SetCurrentLevel_asm);
}

int __declspec(naked) SetCurrentLevel_asm(int stage)
{
	__asm
	{
		push eax
			call SetCurrentLevel
			pop eax
			retn
	}
}

DataPointer(short, word_1934B84, 0x01934B84);
DataPointer(short, isFirstStageLoad, 0x01748B94);

void SetCurrentLevel(int stage)
{
	word_1934B84 = CurrentLevel;

	if (isFirstStageLoad)
		isFirstStageLoad = 0;

	CurrentLevel = stage;

	OnStageChange();
}

void OnStageChange()
{
	using namespace sa2bn::Globals;

	if (!isInitialized())
		return;

	if (!isConnected())
		return;

	if (Networking->isServer())
	{
		Broker->Request(MSG_S_STAGE, true);
		Broker->Finalize();
		Broker->WaitForPlayers(Broker->isClientReady);
	}
	else
	{
		PrintDebug("<> Waiting for stage number...");
		Broker->WaitForPlayers(Broker->stageReceived);
		Broker->Request(MSG_READY, true);
		Broker->Finalize();
	}

	PrintDebug(">> Stage received. Resuming game.");
}