#include "stdafx.h"

#include <SFML/Network.hpp>
#include "Networking.h"
#include "typedefs.h"

#include "PacketExtensions.h"

ushort PacketEx::sequence = 0;

PacketEx::PacketEx(const bool safe) : sf::Packet(), isSafe(safe), MessageTypes(nullptr)
{
	Initialize();
}
PacketEx::~PacketEx()
{
	delete[] MessageTypes;
}

void PacketEx::Initialize()
{
	if (!isSafe)
	{
		sequence %= USHRT_MAX;
		*this << ++sequence;
	}

	empty = true;
	messageCount = 0;

	if (MessageTypes != nullptr)
		delete[] MessageTypes;

	MessageTypes = new bool[nethax::Message::Count]();
}

bool PacketEx::isInPacket(const nethax::Message::_Message type) const
{
	return MessageTypes[type];
}

bool PacketEx::addType(const nethax::Message::_Message type)
{
	if (isInPacket(type))
		return false;

	empty = false;
	*this << type;
	messageCount++;
	return MessageTypes[type] = true;
}

sf::Packet& operator <<(sf::Packet& packet, const char& data)
{
	return packet << (signed char)data;
}
sf::Packet& operator >>(sf::Packet& packet, char& data)
{
	return packet >> (signed char&)data;
}

sf::Packet& operator<<(sf::Packet& packet, const nethax::Message::_Message& data)
{
	return packet << (uint8)data;
}

sf::Packet& operator>>(sf::Packet& packet, nethax::Message::_Message& data)
{
	uint8 d;
	packet >> d; data = (nethax::Message::_Message)d;
	return packet;
}