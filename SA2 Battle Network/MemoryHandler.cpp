#include <iostream>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "LazyMemory.h"

#include "Common.h"
#include "CommonEnums.h"
#include "AddressList.h"
#include "ActionBlacklist.h"

#include "MemoryManagement.h"
#include "PlayerObject.h"
#include "InputStruct.h"
#include "MemoryStruct.h"

#include "Networking.h"
#include "QuickSock.h"
#include "PacketHandler.h"

#include "MemoryHandler.h"

using namespace std;

/*
//	Memory Handler Class
*/

MemoryHandler::MemoryHandler(PacketHandler* packetHandler, bool isserver)
{
	this->packetHandler = packetHandler;
	this->isServer = isserver;

	firstMenuEntry = false;
	wroteP2Start = false;
	splitToggled = false;
	Teleported = false;
	writePlayer = false;

	InitPlayers();
	InitInput();

	cout << "[MemoryHandler] Initializing Memory Structures" << endl;

	memset(&local, 0x00, sizeof(MemStruct));
	//memset(&remote, 0x00, sizeof(MemStruct));

	memset(&recvPlayer, 0x00, sizeof(AbstractPlayer));
	memset(&recvInput, 0x00, sizeof(abstractInput));

	memset(&sendPlayer, 0x00, sizeof(AbstractPlayer));
	memset(&sendInput, 0x00, sizeof(abstractInput));

	thisFrame = 0;
	lastFrame = 0;

	return;
}
MemoryHandler::~MemoryHandler()
{
	packetHandler = nullptr;
	cout << "<> [MemoryHandler::~MemoryHandler] Deinitializing Player Objects and Input Structures" << endl;
	DeinitPlayers();
	DeinitInput();
}

void MemoryHandler::InitPlayers()
{
	if (player1 == nullptr)
		player1 = new PlayerObject(ADDR_PLAYER1);
	else
		cout << "<> [MemoryHandler::InitPlayers] Player 1 has already been initialized." << endl;

	if (player2 == nullptr)
		player2 = new PlayerObject(ADDR_PLAYER2);
	else
		cout << "<> [MemoryHandler::InitPlayers] Player 2 has already been initialized." << endl;

	return;
}
void MemoryHandler::DeinitPlayers()
{
	if (player1 != nullptr)
	{
		delete player1;
		player1 = nullptr;
	}
	if (player2 != nullptr)
	{
		delete player2;
		player2 = nullptr;
	}

	return;
}

void MemoryHandler::InitInput()
{
	if (p1Input == nullptr)
		p1Input = new InputStruct(ADDR_P1INPUT);
	else
		cout << "<> [MemoryHandler::InitInput] P1 Input has already been initialized." << endl;

	if (p2Input == nullptr)
		p2Input = new InputStruct(ADDR_P2INPUT);
	else
		cout << "<> [MemoryHandler::InitInput] P2 Input has already been initialized." << endl;

	return;
}
void MemoryHandler::DeinitInput()
{
	if (p1Input != nullptr)
	{
		delete p1Input;
		p1Input = nullptr;
	}
	if (p2Input != nullptr)
	{
		delete p2Input;
		p2Input = nullptr;
	}

	return;
}

void MemoryHandler::SendSystem(QSocket* Socket)
{
	if (GameState >= GameState::LOAD_FINISHED && TwoPlayerMode > 0)
	{
		if (local.system.GameState != GameState && GameState > GameState::LOAD_FINISHED)
		{
			cout << "<< Sending gamestate [" << (ushort)GameState << "]" << endl;

			packetHandler->WriteReliable(); Socket->writeByte(1);
			Socket->writeByte(MSG_S_GAMESTATE);
			Socket->writeByte(GameState);

			packetHandler->SendMsg(true);
			local.system.GameState = GameState;
		}

		if (local.system.PauseSelection != PauseSelection)
		{
			packetHandler->WriteReliable(); Socket->writeByte(1);
			Socket->writeByte(MSG_S_PAUSESEL);
			Socket->writeByte(PauseSelection);

			packetHandler->SendMsg(true);
			local.system.PauseSelection = PauseSelection;
		}

		if (local.game.TimerSeconds != TimerSeconds && isServer)
		{
			//cout << "<< Sending time [" << (ushort)local.game.TimerMinutes << ":" << (ushort)local.game.TimerSeconds << "]" << endl;
			Socket->writeByte(MSG_NULL); Socket->writeByte(2);
			Socket->writeByte(MSG_S_TIME);
			Socket->writeByte(MSG_KEEPALIVE);

			Socket->writeByte(TimerMinutes);
			Socket->writeByte(TimerSeconds);
			Socket->writeByte(TimerFrames);

			packetHandler->SendMsg();
			memcpy(&local.game.TimerMinutes, &TimerMinutes, sizeof(char) * 3);
			packetHandler->setSendKeepalive();
		}

		if (local.game.TimeStopMode != TimeStopMode)
		{
			cout << "<< Sending time stop [";
			packetHandler->WriteReliable(); Socket->writeByte(1);
			Socket->writeByte(MSG_S_TIMESTOP);

			// Swap the value since player 1 is relative to the client

			switch (TimeStopMode)
			{
			default:
			case 0:
				cout << 0 << "]" << endl;
				Socket->writeByte(0);
				break;

			case 1:
				cout << 2 << "]" << endl;
				Socket->writeByte(2);
				break;

			case 2:
				cout << 1 << "]" << endl;
				Socket->writeByte(1);
				break;
			}

			packetHandler->SendMsg(true);
			local.game.TimeStopMode = TimeStopMode;
		}
	}
	if (GameState != GameState::INGAME || TimeStopMode > 0 || !isServer)
	{
		if (Duration(packetHandler->getSendKeepalive()) >= 1000)
		{
			Socket->writeByte(MSG_NULL); Socket->writeByte(1);
			Socket->writeByte(MSG_KEEPALIVE);

			packetHandler->SendMsg();
			packetHandler->setSendKeepalive();
		}
	}
}
void MemoryHandler::SendInput(QSocket* Socket, uint sendTimer)
{
	if (CurrentMenu[0] == 16 || TwoPlayerMode > 0 && GameState > GameState::INACTIVE)
	{
		p1Input->read();

		if (!CheckFrame())
			ToggleSplitscreen();

		if (sendInput.buttons.Held != p1Input->buttons.Held && GameState != GameState::MEMCARD)
		{
			//cout << "<< Sending buttons!" << endl;
			packetHandler->WriteReliable(); Socket->writeByte(1);

			Socket->writeByte(MSG_I_BUTTONS);
			Socket->writeInt(p1Input->buttons.Held);

			packetHandler->SendMsg(true);

			sendInput.buttons.Held = p1Input->buttons.Held;

		}

		if (sendInput.analog.x != p1Input->analog.x || sendInput.analog.y != p1Input->analog.y)
		{
			if (GameState == GameState::INGAME)
			{
				if (Duration(analogTimer) >= 125)
				{
					if (p1Input->analog.x == 0 && p1Input->analog.y == 0)
					{
						//cout << "<< Analog 0'd out; sending reliably." << endl;
						packetHandler->WriteReliable(); Socket->writeByte(1);
						Socket->writeByte(MSG_I_ANALOG);
						Socket->writeShort(p1Input->analog.x);
						Socket->writeShort(p1Input->analog.y);

						packetHandler->SendMsg(true);

						sendInput.analog.x = p1Input->analog.x;
						sendInput.analog.y = p1Input->analog.y;
					}
					else
					{
						//cout << "<< Sending analog!" << endl;
						Socket->writeByte(MSG_NULL); Socket->writeByte(1);
						Socket->writeByte(MSG_I_ANALOG);
						Socket->writeShort(p1Input->analog.x);
						Socket->writeShort(p1Input->analog.y);

						packetHandler->SendMsg();

						sendInput.analog.x = p1Input->analog.x;
						sendInput.analog.y = p1Input->analog.y;
					}

					analogTimer = millisecs();
				}
			}
			else if (sendInput.analog.y != 0 || sendInput.analog.x != 0)
			{
				cout << "<< Resetting analog" << endl;
				packetHandler->WriteReliable(); Socket->writeByte(1);
				Socket->writeByte(MSG_I_ANALOG);

				sendInput.analog.y = 0;
				sendInput.analog.x = 0;

				Socket->writeShort(sendInput.analog.x);
				Socket->writeShort(sendInput.analog.y);

				packetHandler->SendMsg(true);
			}
		}
	}
	else
		return;
}
void MemoryHandler::SendPlayer(QSocket* Socket)
{
	player1->read();

	// If the game has finished loading...
	if (GameState >= GameState::LOAD_FINISHED && TwoPlayerMode > 0)
	{
		// Check if the stage has changed so we can re->valuate the player.
		if (local.game.CurrentLevel != CurrentLevel)
		{
			cout << "<> Stage has changed to " << (ushort)CurrentLevel /*<< "; re->valuating players."*/ << endl;

			//player1->pointerEval();
			//player2->pointerEval();
			updateAbstractPlayer(&sendPlayer, player1);

			// Reset the ringcounts so they don't get sent.
			local.game.RingCount[0] = 0;
			RingCount[0] = 0;
			local.game.RingCount[1] = 0;

			// Reset specials
			for (int i = 0; i < 3; i++)
				local.game.P2SpecialAttacks[i] = 0;

			// And finally, update the stage so this doesn't loop.
			local.game.CurrentLevel = CurrentLevel;
		}

		if (CheckTeleport())
		{
			// Send a teleport message

			packetHandler->WriteReliable(); Socket->writeByte(2);
			Socket->writeByte(MSG_P_POSITION); Socket->writeByte(MSG_P_SPEED);

			for (int i = 0; i < 3; i++)
			{
				Socket->writeFloat(player1->Position[i]);
				sendPlayer.Position[i] = player1->Position[i];
			}

			Socket->writeFloat(player1->Speed[0]);
			sendPlayer.Speed[0] = player1->Speed[0];

			Socket->writeFloat(player1->Speed[1]);
			sendPlayer.Speed[1] = player1->Speed[1];

			packetHandler->SendMsg(true);
		}

		if (memcmp(&local.game.P1SpecialAttacks[0], &P1SpecialAttacks[0], sizeof(char) * 3) != 0)
		{
			cout << "<< Sending specials!" << endl;
			packetHandler->WriteReliable(); Socket->writeByte(1);
			Socket->writeByte(MSG_2PSPECIALS);
			Socket->writeBytes(&P1SpecialAttacks[0], 3);

			memcpy(&local.game.P1SpecialAttacks[0], &P1SpecialAttacks[0], sizeof(char) * 3);

			packetHandler->SendMsg(true);
		}
		if (player1->CharID[0] == 6 || player1->CharID[0] == 7)
		{
			if (sendPlayer.MechHP != player1->MechHP)
			{
				cout << "<< Sending HP [" << player1->MechHP << "]" << endl;
				packetHandler->WriteReliable(); Socket->writeByte(1);
				Socket->writeByte(MSG_P_HP);
				Socket->writeFloat(player1->MechHP);

				packetHandler->SendMsg(true);
			}
		}

		if (sendPlayer.Action != player1->Action || sendPlayer.Status != player1->Status)
		{
			cout << "<< Sending action...";// << " [S " << sendPlayer.Status << " != " << player1->Status << "]";

			bool sendSpinTimer = ((player1->characterID() == CharacterID::SonicAmy || player1->characterID() == CharacterID::ShadowMetal)
				/*&& (player1->characterID2() == CharacterID2::Sonic || player1->characterID2() == CharacterID2::Shadow)*/);

			if (!isHoldAction(player1->Action))
			{
				cout << endl;
				packetHandler->WriteReliable(); Socket->writeByte((sendSpinTimer) ? 5 : 4);

				Socket->writeByte(MSG_P_POSITION);
				Socket->writeByte(MSG_P_ACTION);
				Socket->writeByte(MSG_P_STATUS);
				Socket->writeByte(MSG_P_ANIMATION);

				if (sendSpinTimer)
					Socket->writeByte(MSG_P_SPINTIMER);

				for (int i = 0; i < 3; i++)
					Socket->writeFloat(player1->Position[i]);
				Socket->writeByte(player1->Action);
				Socket->writeShort(player1->Status);
				Socket->writeShort(player1->Animation[0]);

				if (sendSpinTimer)
					Socket->writeShort(player1->SpinTimer);

				packetHandler->SendMsg(true);
			}
			else
			{
				cout << "without status bitfield. SCIENCE!" << endl;
				packetHandler->WriteReliable(); Socket->writeByte(2);

				Socket->writeByte(MSG_P_POSITION);
				Socket->writeByte(MSG_P_ACTION);
				//Socket->writeByte(MSG_P_ANIMATION);

				for (int i = 0; i < 3; i++)
					Socket->writeFloat(player1->Position[i]);
				Socket->writeByte(player1->Action);

				//Socket->writeShort(player1->Animation[0]);

				packetHandler->SendMsg(true);
			}
		}

		if (local.game.RingCount[0] != RingCount[0])
		{
			local.game.RingCount[0] = RingCount[0];
			cout << "<< Sending rings (" << local.game.RingCount[0] << ")" << endl;
			Socket->writeByte(MSG_NULL); Socket->writeByte(1);
			Socket->writeByte(MSG_P_RINGS);
			Socket->writeShort(local.game.RingCount[0]);
			packetHandler->SendMsg();
		}

		if (memcmp(&sendPlayer.Angle[0], &player1->Angle[0], sizeof(float) * 3) != 0 || memcmp(&sendPlayer.Speed[0], &player1->Speed[0], sizeof(float) * 2) != 0)
		{
			Socket->writeByte(MSG_NULL); Socket->writeByte(3);

			Socket->writeByte(MSG_P_ROTATION);
			Socket->writeByte(MSG_P_POSITION);
			Socket->writeByte(MSG_P_SPEED);
			for (int i = 0; i < 3; i++)
				Socket->writeInt(player1->Angle[i]);
			for (int i = 0; i < 3; i++)
				Socket->writeFloat(player1->Position[i]);
			Socket->writeFloat(player1->Speed[0]);
			Socket->writeFloat(player1->Speed[1]);
			Socket->writeFloat(player1->baseSpeed);

			packetHandler->SendMsg();
		}

		if (sendPlayer.Powerups != player1->Powerups)
		{
			cout << "<< Sending powerups" << endl;
			packetHandler->WriteReliable(); Socket->writeByte(1);
			Socket->writeByte(MSG_P_POWERUPS);
			Socket->writeShort(player1->Powerups);

			packetHandler->SendMsg(true);
		}
		if (sendPlayer.Upgrades != player1->Upgrades)
		{
			cout << "<< Sending upgrades" << endl;
			packetHandler->WriteReliable(); Socket->writeByte(1);
			Socket->writeByte(MSG_P_UPGRADES);
			Socket->writeInt(player1->Upgrades);

			packetHandler->SendMsg(true);
		}

		updateAbstractPlayer(&sendPlayer, player1);
	}

}
void MemoryHandler::SendMenu(QSocket* Socket)
{
	if (GameState == GameState::INACTIVE)
	{
		// Menu analog failsafe
		if (sendInput.analog.x != 0 || sendInput.analog.y != 0)
		{
			cout << "<>\tAnalog failsafe!" << endl;
			p1Input->analog.x = 0;
			p1Input->analog.y = 0;
			sendInput.analog.x = 0;
			sendInput.analog.y = 0;
			p1Input->writeAnalog(&sendInput, 0);
		}

		// ...and we're on the 2P menu...
		if (CurrentMenu[0] == Menu::BATTLE)
		{
			firstMenuEntry = (local.menu.sub != CurrentMenu[1]);

			if (memcmp(local.menu.BattleOptions, BattleOptions, sizeof(char) * 4) != 0)
			{
				cout << "<< Sending battle options..." << endl;
				memcpy(&local.menu.BattleOptions, &BattleOptions, sizeof(char) * 4);
				local.menu.BattleOptionsSelection = BattleOptionsSelection;
				local.menu.BattleOptionsBackSelected = BattleOptionsBackSelected;

				packetHandler->WriteReliable(); Socket->writeByte(2);
				Socket->writeByte(MSG_S_BATTLEOPT); Socket->writeByte(MSG_M_BATTLEOPTSEL);

				Socket->writeBytes(&local.menu.BattleOptions[0], sizeof(char) * 4);
				Socket->writeByte(local.menu.BattleOptionsSelection);
				Socket->writeByte(local.menu.BattleOptionsBackSelected);

				packetHandler->SendMsg(true);
			}

			else if (CurrentMenu[1] == SubMenu2P::S_BATTLEOPT)
			{
				if (local.menu.BattleOptionsSelection != BattleOptionsSelection || local.menu.BattleOptionsBackSelected != BattleOptionsBackSelected || firstMenuEntry && isServer)
				{
					local.menu.BattleOptionsSelection = BattleOptionsSelection;
					local.menu.BattleOptionsBackSelected = BattleOptionsBackSelected;

					packetHandler->WriteReliable(); Socket->writeByte(1);
					Socket->writeByte(MSG_M_BATTLEOPTSEL);

					Socket->writeByte(local.menu.BattleOptionsSelection);
					Socket->writeByte(local.menu.BattleOptionsBackSelected);

					packetHandler->SendMsg(true);
				}
			}


			// ...and we haven't pressed start
			else if (local.menu.atMenu[0] = (CurrentMenu[1] == SubMenu2P::S_START && P2Start == 0))
			{
				if (local.menu.atMenu[0] && local.menu.atMenu[1] && !wroteP2Start)
				{
					P2Start = 2;
					wroteP2Start = true;
				}
			}
			// ...and we HAVE pressed start
			else if (CurrentMenu[1] == SubMenu2P::S_READY || CurrentMenu[1] == SubMenu2P::O_READY)
			{
				if (local.menu.PlayerReady[0] != PlayerReady[0])
				{
					packetHandler->WriteReliable(); Socket->writeByte(1);
					Socket->writeByte(MSG_2PREADY);
					Socket->writeByte(PlayerReady[0]);
					packetHandler->SendMsg(true);

					local.menu.PlayerReady[0] = PlayerReady[0];
				}
			}
			else if (CurrentMenu[1] == SubMenu2P::S_BATTLEMODE || firstMenuEntry && isServer)
			{
				if (local.menu.BattleSelection != BattleSelection)
				{
					packetHandler->WriteReliable(); Socket->writeByte(1);
					Socket->writeByte(MSG_M_BATTLEMODESEL);
					Socket->writeByte(BattleSelection);
					packetHandler->SendMsg(true);

					local.menu.BattleSelection = BattleSelection;
				}
			}
			// Character Selection
			else if (CurrentMenu[1] == SubMenu2P::S_CHARSEL || CurrentMenu[1] == SubMenu2P::O_CHARSEL)
			{
				if (CharacterSelected[0] && CharacterSelected[1] && CurrentMenu[1] == SubMenu2P::S_CHARSEL)
				{
					cout << "<> Resetting character selections" << endl;
					CharacterSelectTimer = 0;
					CurrentMenu[1] = SubMenu2P::O_CHARSEL;
				}


				if (local.menu.CharacterSelection[0] != CharacterSelection[0] || firstMenuEntry)
				{
					cout << "<< Sending character selection" << endl;
					packetHandler->WriteReliable(); Socket->writeByte(1);
					Socket->writeByte(MSG_M_CHARSEL);
					Socket->writeByte(CharacterSelection[0]);
					packetHandler->SendMsg(true);

					local.menu.CharacterSelection[0] = CharacterSelection[0];
				}
				if (local.menu.CharacterSelected[0] != CharacterSelected[0])
				{
					packetHandler->WriteReliable(); Socket->writeByte(1);
					Socket->writeByte(MSG_M_CHARCHOSEN);
					Socket->writeByte(CharacterSelected[0]);
					packetHandler->SendMsg(true);

					local.menu.CharacterSelected[0] = CharacterSelected[0];
				}
				if (firstMenuEntry ||
					((local.menu.AltCharacterSonic != AltCharacterSonic) ||
					(local.menu.AltCharacterShadow != AltCharacterShadow) ||
					(local.menu.AltCharacterTails != AltCharacterTails) ||
					(local.menu.AltCharacterEggman != AltCharacterEggman) ||
					(local.menu.AltCharacterKnuckles != AltCharacterKnuckles) ||
					(local.menu.AltCharacterRouge != AltCharacterRouge))
					)
				{
					local.menu.AltCharacterSonic = AltCharacterSonic;
					local.menu.AltCharacterShadow = AltCharacterShadow;
					local.menu.AltCharacterTails = AltCharacterTails;
					local.menu.AltCharacterEggman = AltCharacterEggman;
					local.menu.AltCharacterKnuckles = AltCharacterKnuckles;
					local.menu.AltCharacterRouge = AltCharacterRouge;

					packetHandler->WriteReliable(); Socket->writeByte(1);
					Socket->writeByte(MSG_M_ALTCHAR);

					Socket->writeBytes(&local.menu.AltCharacterSonic, sizeof(char) * 6);

					packetHandler->SendMsg(true);
				}
			}
			else if (CurrentMenu[1] == SubMenu2P::I_STAGESEL || CurrentMenu[1] == SubMenu2P::S_STAGESEL)
			{
				if ((memcmp(&local.menu.StageSelection2P[0], &StageSelection2P[0], (sizeof(int) * 2)) != 0 || local.menu.BattleOptionsButton != BattleOptionsButton)
					|| firstMenuEntry)
				{
					//cout << "<< Sending Stage Selection" << endl;
					packetHandler->WriteReliable(); Socket->writeByte(1);
					Socket->writeByte(MSG_M_STAGESEL);
					Socket->writeInt(StageSelection2P[0]); Socket->writeInt(StageSelection2P[1]);
					Socket->writeByte(BattleOptionsButton);
					packetHandler->SendMsg(true);

					local.menu.StageSelection2P[0] = StageSelection2P[0];
					local.menu.StageSelection2P[1] = StageSelection2P[1];
					local.menu.BattleOptionsButton = BattleOptionsButton;
				}
			}
		}
		else
		{
			wroteP2Start = false;
			if (local.menu.atMenu[0])
				local.menu.atMenu[0] = false;
		}

		if (local.menu.atMenu[0] != remote.menu.atMenu[0])
		{
			cout << "<< Sending \"On 2P Menu\" state" << endl;
			packetHandler->WriteReliable(); Socket->writeByte(1);
			Socket->writeByte(MSG_M_ATMENU);
			Socket->writeByte(local.menu.atMenu[0]);
			packetHandler->SendMsg(true);

			remote.menu.atMenu[0] = local.menu.atMenu[0];
		}

		local.menu.sub = CurrentMenu[1];
	}
}

inline void MemoryHandler::writeP2Memory()
{
	if (GameState >= GameState::INGAME)
		player2->write(&recvPlayer);
}
inline void MemoryHandler::writeRings() { RingCount[1] = local.game.RingCount[1]; }
inline void MemoryHandler::writeSpecials() { memcpy(P2SpecialAttacks, &local.game.P2SpecialAttacks, sizeof(char) * 3); }
inline void MemoryHandler::writeTimeStop() { TimeStopMode = local.game.TimeStopMode; }

void MemoryHandler::updateAbstractPlayer(AbstractPlayer* recvr, PlayerObject* player)
{
	// Mech synchronize hack
	player2->MechHP = recvPlayer.MechHP;
	player2->Powerups = recvPlayer.Powerups;
	player2->Upgrades = recvPlayer.Upgrades;


	memcpy(recvr, &player->Action, sizeof(AbstractPlayer));
}

void MemoryHandler::ToggleSplitscreen()
{
	if (GameState == GameState::INGAME && TwoPlayerMode > 0)
	{
		if ((sendInput.buttons.Held & (1 << 16)) && (sendInput.buttons.Held & (2 << 16)))
		{
			if (!splitToggled)
			{
				if (SplitscreenMode == 1)
					SplitscreenMode = 2;
				else if (SplitscreenMode == 2)
					SplitscreenMode = 1;
				else
					return;
				splitToggled = true;
			}
		}

		else if (splitToggled)
			splitToggled = false;
	}

	return;
}
bool MemoryHandler::CheckTeleport()
{
	if (GameState == GameState::INGAME && TwoPlayerMode > 0)
	{
		if ((sendInput.buttons.Held & (1 << 5)) && (sendInput.buttons.Held & (1 << 9)))
		{
			if (!Teleported)
			{
				// Teleport to recvPlayer
				cout << "<> Teleporting to other player..." << endl;;
				player1->Teleport(&recvPlayer);

				return Teleported = true;
			}
		}
		else if (Teleported)
			return Teleported = false;
	}

	return false;
}

void MemoryHandler::ReceiveInput(QSocket* Socket, uchar type)
{
	if (CurrentMenu[0] == 16 || TwoPlayerMode > 0 && GameState > GameState::INACTIVE)
	{
		switch (type)
		{
		default:
			return;

		case MSG_I_BUTTONS:
			recvInput.buttons.Held = Socket->readInt();
			if (CheckFrame())
				MemManage::waitFrame(1, thisFrame);
			p2Input->writeButtons(&recvInput);

			return;

		case MSG_I_ANALOG:
			//cout << ">> Received analog!" << endl;
			recvInput.analog.x = Socket->readShort();
			recvInput.analog.y = Socket->readShort();

			p2Input->writeAnalog(&recvInput, GameState);

			return;
		}
	}
	else
		return;
}

void MemoryHandler::ReceiveSystem(QSocket* Socket, uchar type)
{
	if (GameState >= GameState::LOAD_FINISHED)
	{
		switch (type)
		{
		default:
			return;

		case MSG_S_TIME:
			TimerMinutes = local.game.TimerMinutes = Socket->readByte();
			TimerSeconds = local.game.TimerSeconds = Socket->readByte();
			TimerFrames = local.game.TimerFrames = Socket->readByte();
			return;

		case MSG_S_GAMESTATE:
		{
			uchar recvGameState = (char)Socket->readByte();

			if (GameState >= GameState::INGAME && recvGameState > GameState::LOAD_FINISHED)
				MemManage::changeGameState(recvGameState, &local);

			return;
		}

		case MSG_S_PAUSESEL:
			PauseSelection = local.system.PauseSelection = (uchar)Socket->readByte();
			return;

		case MSG_S_TIMESTOP:
			local.game.TimeStopMode = (char)Socket->readByte();
			writeTimeStop();
			return;

		case MSG_2PSPECIALS:
			for (int i = 0; i < 3; i++)
				local.game.P2SpecialAttacks[i] = (char)Socket->readByte();

			writeSpecials();
			return;

		case MSG_P_RINGS:
			local.game.RingCount[1] = Socket->readShort();
			writeRings();

			cout << ">> Ring Count Change: " << local.game.RingCount[1] << endl;
			return;
		}
	}
}

void MemoryHandler::ReceivePlayer(QSocket* Socket, uchar type)
{
	if (GameState >= GameState::LOAD_FINISHED)
	{
		writePlayer = false;
		switch (type)
		{
		default:
			return;

		case MSG_P_HP:
			recvPlayer.MechHP = Socket->readFloat();
			cout << ">> Received HP update. (" << recvPlayer.MechHP << ")" << endl;
			writePlayer = true;
			break;

		case MSG_P_ACTION:
			recvPlayer.Action = Socket->readChar();
			writePlayer = true;
			break;

		case MSG_P_STATUS:
			recvPlayer.Status = Socket->readShort();
			writePlayer = true;
			break;

		case MSG_P_SPINTIMER:
			recvPlayer.SpinTimer = Socket->readShort();
			writePlayer = true;
			break;

		case MSG_P_ANIMATION:
			recvPlayer.Animation[0] = Socket->readShort();
			writePlayer = true;
			break;

		case MSG_P_POSITION:
			for (ushort i = 0; i < 3; i++)
				recvPlayer.Position[i] = Socket->readFloat();
			writePlayer = true;
			break;

		case MSG_P_ROTATION:
			for (ushort i = 0; i < 3; i++)
				recvPlayer.Angle[i] = Socket->readInt();
			writePlayer = true;
			break;

		case MSG_P_SPEED:
			recvPlayer.Speed[0] = Socket->readFloat();
			recvPlayer.Speed[1] = Socket->readFloat();
			recvPlayer.baseSpeed = Socket->readFloat();

			writePlayer = true;
			break;

		case MSG_P_POWERUPS:
			recvPlayer.Powerups = Socket->readShort();
			writePlayer = true;
			break;

		case MSG_P_UPGRADES:
			recvPlayer.Upgrades = Socket->readInt();
			writePlayer = true;
			break;
		}

		if (writePlayer)
			writeP2Memory();

		return;
	}
	else
		return;
}
void MemoryHandler::ReceiveMenu(QSocket* Socket, uchar type)
{
	if (GameState == GameState::INACTIVE)
	{
		switch (type)
		{
		default:
			return;

		case MSG_M_ATMENU:
			local.menu.atMenu[1] = Socket->readOct();
			if (local.menu.atMenu[1])
				cout << ">> Player 2 is ready on the 2P menu!" << endl;
			else
				cout << ">> Player 2 is no longer on the 2P menu." << endl;
			return;

		case MSG_2PREADY:
			PlayerReady[1] = local.menu.PlayerReady[1] = (uchar)Socket->readByte();

			cout << ">> Player 2 ready state changed." << endl;
			return;

		case MSG_M_CHARSEL:
			CharacterSelection[1] = local.menu.CharacterSelection[1] = (uchar)Socket->readByte();

			return;

		case MSG_M_CHARCHOSEN:
			CharacterSelected[1] = local.menu.CharacterSelected[1] = Socket->readByte();
			return;

		case MSG_M_ALTCHAR:
			AltCharacterSonic = local.menu.AltCharacterSonic = Socket->readByte();
			AltCharacterShadow = local.menu.AltCharacterShadow = Socket->readByte();

			AltCharacterTails = local.menu.AltCharacterTails = Socket->readByte();
			AltCharacterEggman = local.menu.AltCharacterEggman = Socket->readByte();

			AltCharacterKnuckles = local.menu.AltCharacterKnuckles = Socket->readByte();
			AltCharacterRouge = local.menu.AltCharacterRouge = Socket->readByte();

			return;

		case MSG_S_BATTLEOPT:
			for (int i = 0; i < 4; i++)
				BattleOptions[i] = (char)Socket->readByte();

			return;

		case MSG_M_BATTLEOPTSEL:
			BattleOptionsSelection = local.menu.BattleOptionsSelection = (char)Socket->readByte();
			BattleOptionsBackSelected = local.menu.BattleOptionsBackSelected = (char)Socket->readByte();

			return;

		case MSG_M_STAGESEL:
			StageSelection2P[0] = local.menu.StageSelection2P[0] = Socket->readInt();
			StageSelection2P[1] = local.menu.StageSelection2P[1] = Socket->readInt();
			BattleOptionsButton = local.menu.BattleOptionsButton = (char)Socket->readByte();

			return;

		case MSG_M_BATTLEMODESEL:
			BattleSelection = local.menu.BattleSelection = (char)Socket->readByte();

			return;
		}
	}
	else
		return;
}

void MemoryHandler::PreReceive()
{
	player2->read();
	updateAbstractPlayer(&recvPlayer, player2);

	p2Input->read();

	writeRings();
	writeSpecials();

	return;
}

void MemoryHandler::PostReceive()
{
	player2->read();
	updateAbstractPlayer(&recvPlayer, player2);
	writeP2Memory();

	p2Input->read();

	writeRings();
	writeSpecials();

	return;
}