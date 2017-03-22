// dllmain.cpp: определяет точку входа для приложения DLL.
#include "stdafx.h"
#include "SDK.h"
#include "Skins.h"
#include "Proxies.h"
#include "Functions.h"

#include "FrameStageNotify.h"
#include "FireEventClientSide.h"

void Activate()
{
	CreateInterfaceFn fnClientFactory = (CreateInterfaceFn)GetProcAddress(GetModuleHandleA("client.dll"), "CreateInterface");
	CreateInterfaceFn fnEngineFactory = (CreateInterfaceFn)GetProcAddress(GetModuleHandleA("engine.dll"), "CreateInterface");

	g_BaseClient = (IBaseClientDLL*)fnClientFactory("VClient018", NULL);
	g_EntityList = (IClientEntityList*)fnClientFactory("VClientEntityList003", NULL);
	g_EngineClient = (IVEngineClient*)fnEngineFactory("VEngineClient014", NULL);
	g_ModelInfo = (IVModelInfoClient*)fnEngineFactory("VModelInfoClient004", NULL);
	g_GameEventMgr = (IGameEventManager2*)fnEngineFactory("GAMEEVENTSMANAGER002", NULL);

	PDWORD* pClientDLLVMT = (PDWORD*)g_BaseClient;
	PDWORD* pGameEventMgrVMT = (PDWORD*)g_GameEventMgr;

	PDWORD pOriginalClientDLLVMT = *pClientDLLVMT;
	PDWORD pOriginalGameEventMgrVMT = *pGameEventMgrVMT;

	//Calculate VTable size
	size_t dwClientDLLVMTSize = 0;
	size_t dwGameEventMgrVMTSize = 0;

	while ((PDWORD)(*pClientDLLVMT)[dwClientDLLVMTSize])
		dwClientDLLVMTSize++;

	while ((PDWORD)(*pGameEventMgrVMT)[dwGameEventMgrVMTSize])
		dwGameEventMgrVMTSize++;
	
	// Create the replacement tables.
	PDWORD pNewClientDLLVMT = new DWORD[dwClientDLLVMTSize];
	PDWORD pNewGameEventMgrVMT = new DWORD[dwGameEventMgrVMTSize];

	// Copy the original table into the replacement table.
	CopyMemory(pNewClientDLLVMT, pOriginalClientDLLVMT, (sizeof(DWORD) * dwClientDLLVMTSize));
	CopyMemory(pNewGameEventMgrVMT, pOriginalGameEventMgrVMT, (sizeof(DWORD) * dwGameEventMgrVMTSize));

	// Change the FrameStageNotify function in the new table to point to our function.
	pNewClientDLLVMT[36] = (DWORD)FrameStageNotifyThink;

	// Change the FireEventClientSide function in the new table to point to our function.
	pNewGameEventMgrVMT[9] = (DWORD)FireEventClientSideThink;

	// Backup the original function from the untouched tables.
	fnOriginalFrameStageNotify = (FrameStageNotify)pOriginalClientDLLVMT[36];
	fnOriginalFireEventClientSide = (FireEventClientSide)pOriginalGameEventMgrVMT[9];

	// Write the virtual method tables.
	*pClientDLLVMT = pNewClientDLLVMT;
	*pGameEventMgrVMT = pNewGameEventMgrVMT;

	// Import skins to use.
	SetSkinConfig();

	// Import replacement kill icons.
	SetKillIconCfg();

	// Search for the 'CBaseViewModel' class.
	for (ClientClass* pClass = g_BaseClient->GetAllClasses(); pClass; pClass = pClass->m_pNext) {
		if (!strcmp(pClass->m_pNetworkName, "CBaseViewModel")) {
			// Search for the 'm_nModelIndex' property.
			RecvTable* pClassTable = pClass->m_pRecvTable;

			for (int nIndex = 0; nIndex < pClassTable->m_nProps; nIndex++) {
				RecvProp* pProp = &pClassTable->m_pProps[nIndex];

				if (!pProp || strcmp(pProp->m_pVarName, "m_nSequence"))
					continue;

				// Store the original proxy function.
				fnSequenceProxyFn = pProp->m_ProxyFn;

				// Replace the proxy function with our sequence changer.
				pProp->m_ProxyFn = (RecvVarProxyFn)SetViewModelSequence;

				break;
			}

			break;
		}
	}
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
		Activate();

	return TRUE;
}

