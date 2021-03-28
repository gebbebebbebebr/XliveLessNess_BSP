#pragma once
#include <stdio.h>
#include <stdint.h>

uint16_t GetSockAddrPort(const SOCKADDR_STORAGE *sockAddrStorage);
bool SetSockAddrPort(SOCKADDR_STORAGE *sockAddrStorage, uint16_t portHBO);
char* GetSockAddrInfo(const SOCKADDR_STORAGE *sockAddrStorage);
bool SockAddrsMatch(const SOCKADDR_STORAGE *sockAddr1, const SOCKADDR_STORAGE *sockAddr2);
