#include <SA2ModLoader.h>

#include "Globals.h"			// for Globals :specialed:
#include "AddressList.h"		// for FrameCount, FrameIncrement

#include "OnFrame.h"

using namespace sa2bn;

void* caseDefault_ptr = (void*)0x004340CC;
void* case08_ptr = (void*)0x0043405D;
void* case09_ptr = (void*)0x0043407E;
void* case10_ptr = (void*)0x0043407E;

void __declspec(naked) OnFrame_MidJump()
{
	__asm
	{
		push eax
		call FrameHandler
		pop eax

		pop edi
		pop esi
		pop ebp
		pop ebx
		pop ecx
		retn
	}
}

void* OnFrame_Hook_ptr = (void*)0x004340E7;
void __declspec(naked) OnFrame_Hook()
{
	__asm
	{
		push eax
		call FrameHandler
		pop eax
		retn
	}
}

unsigned int lastFrame = 0;
void FrameHandler()
{
	if ((FrameCount - lastFrame) > FrameIncrement || FrameCount == lastFrame)
		PrintDebug("\a[FRAME DISCREPANCY]");
	
	lastFrame = FrameCount;

	if (!Globals::isConnected())
	{
		Globals::Program->Connect();
		return;
	}
	else if (!Globals::Program->CheckConnectOK())
	{
		Globals::Program->Disconnect(false, Program::ErrorCode::NotReady);
		return;
	}

	Globals::Memory->RecvLoop();

	Globals::Memory->SendSystem();
	Globals::Memory->SendPlayer();
	Globals::Memory->SendMenu();

	Globals::Memory->Finalize();
}

void InitOnFrame()
{
	WriteJump(case08_ptr, OnFrame_MidJump);
	WriteJump(case09_ptr, OnFrame_MidJump);
	WriteJump(case10_ptr, OnFrame_MidJump);

	// OnFrame caseDefault
	// Occurs if the current game mode isn't 8, 9 or 10, and byte_174AFF9 == 1
	WriteJump(caseDefault_ptr, OnFrame_MidJump);

	// OnFrame OnFrame_Hook
	// Occurs at the end of the function (effectively the "else" to the statement above)
	WriteJump(OnFrame_Hook_ptr, OnFrame_Hook);
}
