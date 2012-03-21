/* 
Copyright (C) 2006-2009 Ricardo Pescuma Domenecci

This is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this file; see the file license.txt.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  
*/


#include "commons.h"


// Prototypes ///////////////////////////////////////////////////////////////////////////

// Service called by the main menu
#define MS_LISTENINGTO_MAINMENU				"ListeningTo/MainMenu"

// Service called by toptoolbar
#define MS_LISTENINGTO_TTB					"ListeningTo/TopToolBar"

// Services called by hotkeys
#define MS_LISTENINGTO_HOTKEYS_ENABLE		"ListeningTo/HotkeysEnable"
#define MS_LISTENINGTO_HOTKEYS_DISABLE		"ListeningTo/HotkeysDisable"
#define MS_LISTENINGTO_HOTKEYS_TOGGLE		"ListeningTo/HotkeysToggle"

#define ICON_NAME "LISTENING_TO_ICON"


PLUGININFOEX pluginInfo={
	sizeof(PLUGININFOEX),
#ifdef UNICODE
	"ListeningTo (Unicode)",
#else
	"ListeningTo",
#endif
	PLUGIN_MAKE_VERSION(0,3,0,0),
	"Handle listening information to/for contacts",
	"Ricardo Pescuma Domenecci",
	"",
	"� 2006-2009 Ricardo Pescuma Domenecci",
	"http://pescuma.org/miranda/listeningto",
	UNICODE_AWARE,
	0,		//doesn't replace anything built-in
#ifdef UNICODE
	{ 0xf981f3f5, 0x35a, 0x444f, { 0x98, 0x92, 0xca, 0x72, 0x2c, 0x19, 0x5a, 0xda } } // {F981F3F5-035A-444f-9892-CA722C195ADA}
#else
	{ 0xa4a8ff7a, 0xc48a, 0x4d2a, { 0xb5, 0xa9, 0x46, 0x46, 0x84, 0x43, 0x26, 0x3d } } // {A4A8FF7A-C48A-4d2a-B5A9-46468443263D}
#endif
};


HINSTANCE hInst;
PLUGINLINK *pluginLink;
struct MM_INTERFACE mmi;
struct UTF8_INTERFACE utfi;

static std::vector<HANDLE> hHooks;
static std::vector<HANDLE> hServices;
static HANDLE hEnableStateChangedEvent = NULL;
HANDLE hExtraIcon = NULL;
static HANDLE hMainMenuGroup = NULL;

static HANDLE hTTB = NULL;
static char *metacontacts_proto = NULL;
BOOL loaded = FALSE;
static UINT hTimer = 0;
static HANDLE hExtraImage = NULL;
static DWORD lastInfoSetTime = 0;

std::vector<ProtocolInfo> proto_itens;


int ModulesLoaded(WPARAM wParam, LPARAM lParam);
int PreShutdown(WPARAM wParam, LPARAM lParam);
int PreBuildContactMenu(WPARAM wParam,LPARAM lParam);
int TopToolBarLoaded(WPARAM wParam, LPARAM lParam);
int ClistExtraListRebuild(WPARAM wParam, LPARAM lParam);
int SettingChanged(WPARAM wParam,LPARAM lParam);

int MainMenuClicked(WPARAM wParam, LPARAM lParam);
BOOL ListeningToEnabled(char *proto, BOOL ignoreGlobal = FALSE);
int ListeningToEnabled(WPARAM wParam, LPARAM lParam);
int EnableListeningTo(WPARAM wParam,LPARAM lParam);
int GetTextFormat(WPARAM wParam,LPARAM lParam);
int GetParsedFormat(WPARAM wParam,LPARAM lParam);
int GetOverrideContactOption(WPARAM wParam,LPARAM lParam);
int GetUnknownText(WPARAM wParam,LPARAM lParam);
int SetNewSong(WPARAM wParam,LPARAM lParam);
void SetExtraIcon(HANDLE hContact, BOOL set);
void SetListeningInfos(LISTENINGTOINFO *lti);
int HotkeysEnable(WPARAM wParam,LPARAM lParam);
int HotkeysDisable(WPARAM wParam,LPARAM lParam);
int HotkeysToggle(WPARAM wParam,LPARAM lParam);

TCHAR* VariablesParseInfo(ARGUMENTSINFO *ai);
TCHAR* VariablesParseType(ARGUMENTSINFO *ai);
TCHAR* VariablesParseArtist(ARGUMENTSINFO *ai);
TCHAR* VariablesParseAlbum(ARGUMENTSINFO *ai);
TCHAR* VariablesParseTitle(ARGUMENTSINFO *ai);
TCHAR* VariablesParseTrack(ARGUMENTSINFO *ai);
TCHAR* VariablesParseYear(ARGUMENTSINFO *ai);
TCHAR* VariablesParseGenre(ARGUMENTSINFO *ai);
TCHAR* VariablesParseLength(ARGUMENTSINFO *ai);
TCHAR* VariablesParsePlayer(ARGUMENTSINFO *ai);


#define UNKNOWN(_X_) ( _X_ == NULL || _X_[0] == _T('\0') ? opts.unknown : _X_ )



// Functions ////////////////////////////////////////////////////////////////////////////



extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) 
{
	hInst = hinstDLL;
	return TRUE;
}


extern "C" __declspec(dllexport) PLUGININFO* MirandaPluginInfo(DWORD mirandaVersion) 
{
	pluginInfo.cbSize = sizeof(PLUGININFO);
	return (PLUGININFO*) &pluginInfo;
}


extern "C" __declspec(dllexport) PLUGININFOEX* MirandaPluginInfoEx(DWORD mirandaVersion)
{
	pluginInfo.cbSize = sizeof(PLUGININFOEX);
	return &pluginInfo;
}


static const MUUID interfaces[] = { MIID_LISTENINGTO, MIID_LAST };
extern "C" __declspec(dllexport) const MUUID* MirandaPluginInterfaces(void)
{
	return interfaces;
}

/*
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
	// Find the windows
	char class_name[1024];
	if (GetClassNameA(hwnd, class_name, sizeof(class_name)))
	{
		class_name[sizeof(class_name)-1] = '\0';
OutputDebugStringA(class_name);
OutputDebugStringA(" -> ");
		GetWindowTextA(hwnd, class_name, 1024);
OutputDebugStringA(class_name);
OutputDebugStringA("\n");
	}

	return TRUE;
}
*/

extern "C" int __declspec(dllexport) Load(PLUGINLINK *link) 
{
//	EnumWindows(EnumWindowsProc, 0);

	pluginLink = link;

	mir_getMMI(&mmi);
	mir_getUTFI(&utfi);

	CHECK_VERSION("Listening To")

	CoInitialize(NULL);

	// Services
	hServices.push_back( CreateServiceFunction(MS_LISTENINGTO_ENABLED, ListeningToEnabled) );
	hServices.push_back( CreateServiceFunction(MS_LISTENINGTO_ENABLE, EnableListeningTo) );
	hServices.push_back( CreateServiceFunction(MS_LISTENINGTO_GETTEXTFORMAT, GetTextFormat) );
	hServices.push_back( CreateServiceFunction(MS_LISTENINGTO_GETPARSEDTEXT, GetParsedFormat) );
	hServices.push_back( CreateServiceFunction(MS_LISTENINGTO_OVERRIDECONTACTOPTION, GetOverrideContactOption) );
	hServices.push_back( CreateServiceFunction(MS_LISTENINGTO_GETUNKNOWNTEXT, GetUnknownText) );
	hServices.push_back( CreateServiceFunction(MS_LISTENINGTO_MAINMENU, MainMenuClicked) );
	hServices.push_back( CreateServiceFunction(MS_LISTENINGTO_SET_NEW_SONG, SetNewSong) );
	hServices.push_back( CreateServiceFunction(MS_LISTENINGTO_HOTKEYS_ENABLE, HotkeysEnable) );
	hServices.push_back( CreateServiceFunction(MS_LISTENINGTO_HOTKEYS_DISABLE, HotkeysDisable) );
	hServices.push_back( CreateServiceFunction(MS_LISTENINGTO_HOTKEYS_TOGGLE, HotkeysToggle) );
	
	// Hooks
	hHooks.push_back( HookEvent(ME_SYSTEM_MODULESLOADED, ModulesLoaded) );
	hHooks.push_back( HookEvent(ME_SYSTEM_PRESHUTDOWN, PreShutdown) );
	hHooks.push_back( HookEvent(ME_DB_CONTACT_SETTINGCHANGED, SettingChanged) );

	hEnableStateChangedEvent = CreateHookableEvent(ME_LISTENINGTO_ENABLE_STATE_CHANGED);

	InitMusic();
	InitOptions();

	return 0;
}

extern "C" int __declspec(dllexport) Unload(void) 
{
	CoUninitialize();

	return 0;
}


int ProtoServiceExists(const char *szModule, const char *szService)
{
	char str[MAXMODULELABELLENGTH];
	strcpy(str,szModule);
	strcat(str,szService);
	return ServiceExists(str);
}

void UpdateGlobalStatusMenus()
{
	BOOL enabled = ListeningToEnabled(NULL, TRUE);

	CLISTMENUITEM clmi = {0};
	clmi.cbSize = sizeof(clmi);
	clmi.flags = CMIM_FLAGS 
			| (enabled ? CMIF_CHECKED : 0)
			| (opts.enable_sending ? 0 : CMIF_GRAYED);
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) proto_itens[0].hMenu, (LPARAM) &clmi);

	if (hTTB != NULL)
		CallService(MS_TTB_SETBUTTONSTATE, (WPARAM) hTTB, (LPARAM) (enabled ? TTBST_PUSHED : TTBST_RELEASED));
}


bool CompareAccounts(const ProtocolInfo &one, const ProtocolInfo &two)
{
	return lstrcmp(one.account, two.account) < 0;
}


void RebuildMenu()
{
	std::sort(proto_itens.begin(), proto_itens.end(), CompareAccounts);

	for (unsigned int i = 1; i < proto_itens.size(); i++)
	{
		ProtocolInfo *info = &proto_itens[i];

		if (info->hMenu != NULL)
			CallService(MS_CLIST_REMOVEMAINMENUITEM, (WPARAM) info->hMenu, 0);

		TCHAR text[512];
		mir_sntprintf(text, MAX_REGS(text), TranslateT("Send to %s"), info->account);

		CLISTMENUITEM mi = {0};
		mi.cbSize = sizeof(mi);
		mi.position = 100000 + i;
		mi.pszPopupName = (char *) hMainMenuGroup;
		mi.popupPosition = 500080000 + i;
		mi.pszService = MS_LISTENINGTO_MAINMENU;
		mi.ptszName = text;
		mi.flags = CMIF_CHILDPOPUP | CMIF_TCHAR
				| (ListeningToEnabled(info->proto, TRUE) ? CMIF_CHECKED : 0)
				| (opts.enable_sending ? 0 : CMIF_GRAYED);

		info->hMenu = (HANDLE) CallService(MS_CLIST_ADDMAINMENUITEM, 0, (LPARAM)&mi);
	}

	UpdateGlobalStatusMenus();
}

void RegisterProtocol(char *proto, TCHAR *account)
{		
	if (!ProtoServiceExists(proto, PS_SET_LISTENINGTO) &&
		!ProtoServiceExists(proto, PS_ICQ_SETCUSTOMSTATUSEX))
		return;

	int id = proto_itens.size();
	proto_itens.resize(id+1);

	strncpy(proto_itens[id].proto, proto, MAX_REGS(proto_itens[id].proto));
	proto_itens[id].proto[MAX_REGS(proto_itens[id].proto)-1] = 0;

	lstrcpyn(proto_itens[id].account, account, MAX_REGS(proto_itens[id].account));

	proto_itens[id].hMenu = NULL;
	proto_itens[id].old_xstatus = 0;
	proto_itens[id].old_xstatus_name[0] = _T('\0');
	proto_itens[id].old_xstatus_message[0] = _T('\0');
}


int AccListChanged(WPARAM wParam, LPARAM lParam) 
{
	PROTOACCOUNT *proto = (PROTOACCOUNT *) lParam;
	if (proto == NULL || proto->type != PROTOTYPE_PROTOCOL)
		return 0;

	ProtocolInfo *info = GetProtoInfo(proto->szModuleName);
	if (info != NULL)
	{
		if (wParam == PRAC_UPGRADED || wParam == PRAC_CHANGED)
		{
			lstrcpyn(info->account, proto->tszAccountName, MAX_REGS(info->account));

			TCHAR text[512];
			mir_sntprintf(text, MAX_REGS(text), TranslateT("Send to %s"), info->account);

			CLISTMENUITEM clmi = {0};
			clmi.cbSize = sizeof(clmi);
			clmi.flags = CMIM_NAME | CMIF_TCHAR;
			clmi.ptszName = text;
			CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) info->hMenu, (LPARAM) &clmi);
		} 
		else if (wParam == PRAC_REMOVED || (wParam == PRAC_CHECKED && !proto->bIsEnabled))
		{
			CallService(MS_CLIST_REMOVEMAINMENUITEM, (WPARAM) info->hMenu, 0);

			for(std::vector<ProtocolInfo>::iterator it = proto_itens.begin(); it != proto_itens.end(); ++it)
			{
				if (&(*it) == info)
				{
					proto_itens.erase(it);
					break;
				}
			}

			RebuildMenu();
		}
	}
	else
	{
		if (wParam == PRAC_ADDED || (wParam == PRAC_CHECKED && proto->bIsEnabled))
		{
			RegisterProtocol(proto->szModuleName, proto->tszAccountName);
			RebuildMenu();
		}
	}

	return 0;
}


int GetMusicXStatusID(char *proto);

// Called when all the modules are loaded
int ModulesLoaded(WPARAM wParam, LPARAM lParam) 
{
	EnableDisablePlayers();

	if (ServiceExists(MS_MC_GETPROTOCOLNAME))
		metacontacts_proto = (char *) CallService(MS_MC_GETPROTOCOLNAME, 0, 0);

	// add our modules to the KnownModules list
	CallService("DBEditorpp/RegisterSingleModule", (WPARAM) MODULE_NAME, 0);

	IcoLib_Register(ICON_NAME, _T("Contact List"), _T("Listening to"), IDI_LISTENINGTO);

	// Extra icon support
	hExtraIcon = ExtraIcon_Register(MODULE_NAME, "Listening to music", ICON_NAME);
	if (hExtraIcon != NULL)
	{
		HANDLE hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
		while (hContact != NULL)
		{
			char *proto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
			if (proto != NULL)
			{
				DBVARIANT dbv = {0};
				if (!DBGetContactSettingTString(hContact, proto, "ListeningTo", &dbv))
				{
					if (dbv.ptszVal != NULL && dbv.ptszVal[0] != 0)
						SetExtraIcon(hContact, TRUE);
					
					DBFreeVariant(&dbv);
				}
			}

			hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM) hContact, 0);
		}
	}
	else if (hExtraIcon == NULL && ServiceExists(MS_CLIST_EXTRA_ADD_ICON))
	{
		hHooks.push_back( HookEvent(ME_CLIST_EXTRA_LIST_REBUILD, ClistExtraListRebuild) );
	}


    // updater plugin support
    if (ServiceExists(MS_UPDATE_REGISTER))
	{
		Update upd = {0};
		char szCurrentVersion[30];

		upd.cbSize = sizeof(upd);
		upd.szComponentName = pluginInfo.shortName;

		upd.szUpdateURL = UPDATER_AUTOREGISTER;

		upd.szBetaVersionURL = "http://pescuma.org/miranda/listeningto_version.txt";
		upd.szBetaChangelogURL = "http://pescuma.org/miranda/listeningto#Changelog";
		upd.pbBetaVersionPrefix = (BYTE *)"ListeningTo ";
		upd.cpbBetaVersionPrefix = strlen((char *)upd.pbBetaVersionPrefix);
#ifdef UNICODE
		upd.szBetaUpdateURL = "http://pescuma.org/miranda/listeningtoW.zip";
#else
		upd.szBetaUpdateURL = "http://pescuma.org/miranda/listeningto.zip";
#endif

		upd.pbVersion = (BYTE *)CreateVersionStringPlugin((PLUGININFO*) &pluginInfo, szCurrentVersion);
		upd.cpbVersion = strlen((char *)upd.pbVersion);

        CallService(MS_UPDATE_REGISTER, 0, (LPARAM)&upd);
	}

	{
		CLISTMENUITEM mi = {0};
		mi.cbSize = sizeof(mi);

		// Add main menu item
		mi.position = 500080000;
		mi.pszPopupName = (char*) -1;
		mi.pszName = "Listening to";
		mi.flags = CMIF_ROOTPOPUP;
		mi.hIcon = IcoLib_LoadIcon(ICON_NAME);

		hMainMenuGroup = (HANDLE) CallService(MS_CLIST_ADDMAINMENUITEM, 0, (LPARAM) &mi);

		IcoLib_ReleaseIcon(mi.hIcon);

		mi.pszPopupName = (char *) hMainMenuGroup;
		mi.popupPosition = 500080000;
		mi.position = 0;
		mi.pszService = MS_LISTENINGTO_MAINMENU;
		mi.hIcon = NULL;

		// Add all protos
		mi.pszName = Translate("Send to all protocols");
		mi.flags = CMIF_CHILDPOPUP 
				| (ListeningToEnabled(NULL, TRUE) ? CMIF_CHECKED : 0)
				| (opts.enable_sending ? 0 : CMIF_GRAYED);
		proto_itens.resize(1);
		proto_itens[0].hMenu = (HANDLE) CallService(MS_CLIST_ADDMAINMENUITEM, 0, (LPARAM)&mi);
		proto_itens[0].proto[0] = 0;
		proto_itens[0].account[0] = 0;
		proto_itens[0].old_xstatus = 0;
		proto_itens[0].old_xstatus_name[0] = _T('\0');
		proto_itens[0].old_xstatus_message[0] = _T('\0');
	}

	// Add each proto

	if (ServiceExists(MS_PROTO_ENUMACCOUNTS))
	{
		PROTOACCOUNT **protos;
		int count;
		CallService(MS_PROTO_ENUMACCOUNTS, (WPARAM)&count, (LPARAM)&protos);

		for (int i = 0; i < count; i++)
		{
			if (protos[i]->type != PROTOTYPE_PROTOCOL)
				continue;
			if (!protos[i]->bIsEnabled)
				continue;

			RegisterProtocol(protos[i]->szModuleName, protos[i]->tszAccountName);
		}

		hHooks.push_back( HookEvent(ME_PROTO_ACCLISTCHANGED, AccListChanged) );
	}
	else
	{
		PROTOCOLDESCRIPTOR **protos;
		int count;
		CallService(MS_PROTO_ENUMPROTOCOLS, (WPARAM)&count, (LPARAM)&protos);

		for (int i = 0; i < count; i++)
		{
			if (protos[i]->type != PROTOTYPE_PROTOCOL)
				continue;

			char name[128];
			CallProtoService(protos[i]->szName, PS_GETNAME, sizeof(name), (LPARAM)name);

			TCHAR *acc = mir_a2t(name);
			RegisterProtocol(protos[i]->szName, acc);
			mir_free(acc);
		}
	}

	RebuildMenu();

	hHooks.push_back( HookEvent(ME_TTB_MODULELOADED, TopToolBarLoaded) );

	// Variables support
	if (ServiceExists(MS_VARS_REGISTERTOKEN))
	{
		TOKENREGISTER tr = {0};
		tr.cbSize = sizeof(TOKENREGISTER);
		tr.memType = TR_MEM_MIRANDA;
		tr.flags = TRF_FREEMEM | TRF_PARSEFUNC | TRF_FIELD | TRF_TCHAR;

		tr.tszTokenString = _T("listening_info");
		tr.parseFunctionT = VariablesParseInfo;
		tr.szHelpText = "Listening info\tListening info as set in the options";
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM) &tr);

		tr.tszTokenString = _T("listening_type");
		tr.parseFunctionT = VariablesParseType;
		tr.szHelpText = "Listening info\tMedia type: Music, Video, etc";
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM) &tr);

		tr.tszTokenString = _T("listening_artist");
		tr.parseFunctionT = VariablesParseArtist;
		tr.szHelpText = "Listening info\tArtist name";
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM) &tr);

		tr.tszTokenString = _T("listening_album");
		tr.parseFunctionT = VariablesParseAlbum;
		tr.szHelpText = "Listening info\tAlbum name";
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM) &tr);

		tr.tszTokenString = _T("listening_title");
		tr.parseFunctionT = VariablesParseTitle;
		tr.szHelpText = "Listening info\tSong name";
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM) &tr);

		tr.tszTokenString = _T("listening_track");
		tr.parseFunctionT = VariablesParseTrack;
		tr.szHelpText = "Listening info\tTrack number";
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM) &tr);

		tr.tszTokenString = _T("listening_year");
		tr.parseFunctionT = VariablesParseYear;
		tr.szHelpText = "Listening info\tSong year";
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM) &tr);

		tr.tszTokenString = _T("listening_genre");
		tr.parseFunctionT = VariablesParseGenre;
		tr.szHelpText = "Listening info\tSong genre";
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM) &tr);

		tr.tszTokenString = _T("listening_length");
		tr.parseFunctionT = VariablesParseLength;
		tr.szHelpText = "Listening info\tSong length";
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM) &tr);

		tr.tszTokenString = _T("listening_player");
		tr.parseFunctionT = VariablesParsePlayer;
		tr.szHelpText = "Listening info\tPlayer name";
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM) &tr);
	}

	// Hotkeys support
	if (ServiceExists(MS_HOTKEY_REGISTER))
	{
		HOTKEYDESC hkd = {0};
		hkd.cbSize = sizeof(hkd);
		hkd.pszSection = Translate("Listening to");

		hkd.pszService = MS_LISTENINGTO_HOTKEYS_ENABLE;
		hkd.pszName = "ListeningTo/EnableAll";
		hkd.pszDescription = Translate("Send to all protocols");
		CallService(MS_HOTKEY_REGISTER, 0, (LPARAM)&hkd);

		hkd.pszService = MS_LISTENINGTO_HOTKEYS_DISABLE;
		hkd.pszName = "ListeningTo/DisableAll";
		hkd.pszDescription = Translate("Don't send to any protocols");
		CallService(MS_HOTKEY_REGISTER, 0, (LPARAM)&hkd);

		hkd.pszService = MS_LISTENINGTO_HOTKEYS_TOGGLE;
		hkd.pszName = "ListeningTo/ToggleAll";
		hkd.pszDescription = Translate("Toggle send to all protocols");
		CallService(MS_HOTKEY_REGISTER, 0, (LPARAM)&hkd);
	}

	SetListeningInfos(NULL);
	StartTimer();

	loaded = TRUE;

	return 0;
}


int PreShutdown(WPARAM wParam, LPARAM lParam)
{
	loaded = FALSE;

	if (hTimer != NULL)
	{
		KillTimer(NULL, hTimer);
		hTimer = NULL;
	}

	DeInitOptions();

	DestroyHookableEvent(hEnableStateChangedEvent);

	int i;
	for(i = 0; i < hHooks.size(); i++)
		UnhookEvent(hHooks[i]);

	for(i = 0; i < hServices.size(); i++)
		DestroyServiceFunction(hServices[i]);

	FreeMusic();

	return 0;
}


int TopToolBarClick(WPARAM wParam, LPARAM lParam)
{
	BOOL enabled = !ListeningToEnabled(NULL, TRUE);

	EnableListeningTo(NULL, enabled);

	return 0;
}


// Toptoolbar hook to put an icon in the toolbar
int TopToolBarLoaded(WPARAM wParam, LPARAM lParam) 
{
	BOOL enabled = ListeningToEnabled(NULL, TRUE);

	hServices.push_back( CreateServiceFunction(MS_LISTENINGTO_TTB, TopToolBarClick) );

	TTBButton ttb = {0};
	ttb.cbSize = sizeof(ttb);
	ttb.hbBitmapUp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_TTB_UP_DISABLED));
	ttb.hbBitmapDown = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_TTB_UP_ENABLED));
	ttb.pszServiceUp = MS_LISTENINGTO_TTB;
	ttb.pszServiceDown = MS_LISTENINGTO_TTB;
	ttb.dwFlags = TTBBF_VISIBLE | TTBBF_SHOWTOOLTIP | (enabled ? TTBBF_PUSHED : 0);
	ttb.name = Translate("Enable/Disable sending Listening To info (to all protocols)");
	
	hTTB = (HANDLE)CallService(MS_TTB_ADDBUTTON, (WPARAM)&ttb, 0);

	return 0;
}


int MainMenuClicked(WPARAM wParam, LPARAM lParam)
{
	if (!loaded)
		return -1;

	int pos = wParam == 0 ? 0 : wParam - 500080000;

	if (pos >= proto_itens.size() || pos < 0)
		return 0;

	EnableListeningTo((WPARAM) proto_itens[pos].proto, (LPARAM) !ListeningToEnabled(proto_itens[pos].proto, TRUE));
	return 0;
}


BOOL ListeningToEnabled(char *proto, BOOL ignoreGlobal) 
{
	if (!ignoreGlobal && !opts.enable_sending)
		return FALSE;

	if (proto == NULL || proto[0] == 0)
	{
		// Check all protocols
		BOOL enabled = TRUE;

		for (unsigned int i = 1; i < proto_itens.size(); ++i)
		{
			if (!ListeningToEnabled(proto_itens[i].proto, TRUE))
			{
				enabled = FALSE;
				break;
			}
		}

		return enabled;
	}
	else
	{
		char setting[256];
		mir_snprintf(setting, sizeof(setting), "%sEnabled", proto);
		return (BOOL) DBGetContactSettingByte(NULL, MODULE_NAME, setting, FALSE);
	}
}


int ListeningToEnabled(WPARAM wParam, LPARAM lParam) 
{
	if (!loaded)
		return -1;

	return ListeningToEnabled((char *)wParam) ;
}


ProtocolInfo *GetProtoInfo(char *proto)
{
	for (unsigned int i = 1; i < proto_itens.size(); i++)
		if (strcmp(proto, proto_itens[i].proto) == 0)
			return &proto_itens[i];

	return NULL;
}

BOOL IsMusicXStatus(char *proto, int id)
{
	TCHAR xstatus_name[1024];

	ICQ_CUSTOM_STATUS ics = {0};
	ics.cbSize = sizeof(ICQ_CUSTOM_STATUS);
	ics.flags = (CSSF_MASK_NAME | CSSF_DEFAULT_NAME | CSSF_TCHAR);
	ics.ptszName = xstatus_name;
	ics.wParam = (WPARAM *) &id;

	if (CallProtoService(proto, PS_ICQ_GETCUSTOMSTATUSEX, 0, (LPARAM) &ics))
		return FALSE;

	_tcslwr(xstatus_name);
	return _tcsstr(xstatus_name, _T("music")) != NULL;
}

int GetMusicXStatusID(char *proto)
{
	if (!ProtoServiceExists(proto, PS_ICQ_GETCUSTOMSTATUSEX))
		return -1;

	int defs[] = { 
		11, // ICQ default
		48, // MRA default
	};

	for (int i = 0; i < MAX_REGS(defs); ++i)
		if (IsMusicXStatus(proto, defs[i]))
			return defs[i];

	for (int i = 1; i < 100; ++i)
	{
		if (IsMusicXStatus(proto, i))
			return i;
	}

	return -1;
}

void SetListeningInfo(char *proto, LISTENINGTOINFO *lti)
{
//	m_log(_T("SetListeningInfo"), _T("proto=%S  lti=%d  title=%s"), 
//		proto, (int) lti, lti == NULL ? _T("") : lti->ptszTitle);
		
	if (proto == NULL)
		return;

	if (!ListeningToEnabled(proto))
	{
		lti = NULL;
//		m_log(_T("SetListeningInfo"), _T("DISABLED -> lti = NULL"));
	}


	if (ProtoServiceExists(proto, PS_SET_LISTENINGTO))
	{
		CallProtoService(proto, PS_SET_LISTENINGTO, 0, (LPARAM) lti);
	}
	else if (ProtoServiceExists(proto, PS_ICQ_SETCUSTOMSTATUSEX))
	{
		if (opts.xstatus_set == IGNORE_XSTATUS)
			return;

		int music_xstatus_id = GetMusicXStatusID(proto);
		if (music_xstatus_id <= 0)
			return;

		int status;
		ICQ_CUSTOM_STATUS ics = {0};
		ics.cbSize = sizeof(ICQ_CUSTOM_STATUS);
		ics.status = &status;

		// Set or reset?
		if (lti == NULL)
		{
			// Reset -> only if is still in music xstatus
			ics.flags = CSSF_MASK_STATUS;
			if (CallProtoService(proto, PS_ICQ_GETCUSTOMSTATUSEX, 0, (LPARAM) &ics) || status != music_xstatus_id)
			{
				if (opts.xstatus_set == SET_XSTATUS)
				{
					ProtocolInfo *pi = GetProtoInfo(proto);
					if (pi != NULL) 
					{
						pi->old_xstatus = 0;
						pi->old_xstatus_name[0] = _T('\0');
						pi->old_xstatus_message[0] = _T('\0');
					}
				}
				return;
			}

			if (opts.xstatus_set == CHECK_XSTATUS_MUSIC)
			{
				// Set text to nothing
				TCHAR *fr[] = { 
					_T("listening"), opts.nothing
				};

				Buffer<TCHAR> name;
				ReplaceTemplate(&name, NULL, opts.xstatus_name, fr, MAX_REGS(fr));
				Buffer<TCHAR> msg;
				ReplaceTemplate(&msg, NULL, opts.xstatus_message, fr, MAX_REGS(fr));

				ics.flags = CSSF_TCHAR | CSSF_MASK_STATUS |	CSSF_MASK_NAME | CSSF_MASK_MESSAGE;
				ics.ptszName = name.str;
				ics.ptszMessage = msg.str;

				CallProtoService(proto, PS_ICQ_SETCUSTOMSTATUSEX, 0, (LPARAM) &ics);
			}
			else if (opts.xstatus_set == CHECK_XSTATUS)
			{
				status = 0;
				ics.flags = CSSF_MASK_STATUS;

				CallProtoService(proto, PS_ICQ_SETCUSTOMSTATUSEX, 0, (LPARAM) &ics);
			}
			else
			{
				// Set to old text
				ProtocolInfo *pi = GetProtoInfo(proto);
				if (pi != NULL) 
				{
					ics.flags = CSSF_TCHAR | CSSF_MASK_STATUS |	CSSF_MASK_NAME | CSSF_MASK_MESSAGE;
					ics.status = &pi->old_xstatus;
					ics.ptszName = pi->old_xstatus_name;
					ics.ptszMessage = pi->old_xstatus_message;
				}
				else
				{
					status = 0;
					ics.flags = CSSF_MASK_STATUS;
				}

				CallProtoService(proto, PS_ICQ_SETCUSTOMSTATUSEX, 0, (LPARAM) &ics);

				if (pi != NULL) 
				{
					pi->old_xstatus = 0;
					pi->old_xstatus_name[0] = _T('\0');
					pi->old_xstatus_message[0] = _T('\0');
				}
			}
		}
		else
		{
			// Set it
			if (opts.xstatus_set == CHECK_XSTATUS_MUSIC)
			{
				ics.flags = CSSF_MASK_STATUS;
				if (CallProtoService(proto, PS_ICQ_GETCUSTOMSTATUSEX, 0, (LPARAM) &ics) || status != music_xstatus_id)
					return;
			}
			else if (opts.xstatus_set == CHECK_XSTATUS)
			{
				ics.flags = CSSF_MASK_STATUS;
				if (!CallProtoService(proto, PS_ICQ_GETCUSTOMSTATUSEX, 0, (LPARAM) &ics) && status != music_xstatus_id && status != 0)
					return;
			}
			else
			{
				// Store old data
				ics.flags = CSSF_MASK_STATUS;
				if (!CallProtoService(proto, PS_ICQ_GETCUSTOMSTATUSEX, 0, (LPARAM) &ics) && status != music_xstatus_id)
				{
					ProtocolInfo *pi = GetProtoInfo(proto);
					if (pi != NULL)
					{
						ics.flags = CSSF_TCHAR | CSSF_MASK_STATUS |	CSSF_MASK_NAME | CSSF_MASK_MESSAGE;
						ics.status = &pi->old_xstatus;
						ics.ptszName = pi->old_xstatus_name;
						ics.ptszMessage = pi->old_xstatus_message;

						CallProtoService(proto, PS_ICQ_GETCUSTOMSTATUSEX, 0, (LPARAM) &ics);
					}
				}
			}

			TCHAR *fr[] = { 
				_T("listening"), (TCHAR *) GetParsedFormat(0, (WPARAM) lti),
				_T("artist"), UNKNOWN(lti->ptszArtist),
				_T("album"), UNKNOWN(lti->ptszAlbum),
				_T("title"), UNKNOWN(lti->ptszTitle),
				_T("track"), UNKNOWN(lti->ptszTrack),
				_T("year"), UNKNOWN(lti->ptszYear),
				_T("genre"), UNKNOWN(lti->ptszGenre),
				_T("length"), UNKNOWN(lti->ptszLength),
				_T("player"), UNKNOWN(lti->ptszPlayer),
				_T("type"), UNKNOWN(lti->ptszType)
			};

			Buffer<TCHAR> name;
			ReplaceTemplate(&name, NULL, opts.xstatus_name, fr, MAX_REGS(fr));
			Buffer<TCHAR> msg;
			ReplaceTemplate(&msg, NULL, opts.xstatus_message, fr, MAX_REGS(fr));

			status = music_xstatus_id;
			ics.flags = CSSF_TCHAR | CSSF_MASK_STATUS |	CSSF_MASK_NAME | CSSF_MASK_MESSAGE;
			ics.status = &status;
			ics.ptszName = name.str;
			ics.ptszMessage = msg.str;

			CallProtoService(proto, PS_ICQ_SETCUSTOMSTATUSEX, 0, (LPARAM) &ics);

			mir_free(fr[1]);
		}
	}
}


int EnableListeningTo(WPARAM wParam,LPARAM lParam) 
{
	if (!loaded)
		return -1;

	char *proto = (char *)wParam;

	if (proto == NULL || proto[0] == 0)
	{
		// For all protocols
		for (unsigned int i = 1; i < proto_itens.size(); ++i)
		{
			EnableListeningTo((WPARAM) proto_itens[i].proto, lParam);
		}
	}
	else
	{
		if (!ProtoServiceExists(proto, PS_SET_LISTENINGTO) &&
			!ProtoServiceExists(proto, PS_ICQ_SETCUSTOMSTATUSEX))
			return 0;

		char setting[256];
		mir_snprintf(setting, sizeof(setting), "%sEnabled", proto);
		DBWriteContactSettingByte(NULL, MODULE_NAME, setting, (BOOL) lParam);

		// Modify menu info
		ProtocolInfo *info = GetProtoInfo(proto);
		if (info != NULL)
		{
			CLISTMENUITEM clmi = {0};
			clmi.cbSize = sizeof(clmi);
			clmi.flags = CMIM_FLAGS 
					| (lParam ? CMIF_CHECKED : 0)
					| (opts.enable_sending ? 0 : CMIF_GRAYED);
			CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) info->hMenu, (LPARAM) &clmi);

			if (!opts.enable_sending || !lParam)
				SetListeningInfo(proto, NULL);
			else
				SetListeningInfo(proto, GetListeningInfo());
		}

		// Set all protos info
		UpdateGlobalStatusMenus();
	}

	StartTimer();

	NotifyEventHooks(hEnableStateChangedEvent, wParam, lParam);

	return 0;
}


int HotkeysEnable(WPARAM wParam,LPARAM lParam) 
{
	return EnableListeningTo(lParam, TRUE);
}


int HotkeysDisable(WPARAM wParam,LPARAM lParam) 
{
	return EnableListeningTo(lParam, FALSE);
}


int HotkeysToggle(WPARAM wParam,LPARAM lParam) 
{
	return EnableListeningTo(lParam, !ListeningToEnabled((char *)lParam, TRUE));
}


int GetTextFormat(WPARAM wParam,LPARAM lParam) 
{
	if (!loaded)
		return NULL;

	return (int) mir_tstrdup(opts.templ);
}


int GetParsedFormat(WPARAM wParam,LPARAM lParam) 
{
	if (!loaded)
		return NULL;

	LISTENINGTOINFO *lti = (LISTENINGTOINFO *) lParam;

	if (lti == NULL)
		return NULL;

	TCHAR *fr[] = { 
		_T("artist"), UNKNOWN(lti->ptszArtist),
		_T("album"), UNKNOWN(lti->ptszAlbum),
		_T("title"), UNKNOWN(lti->ptszTitle),
		_T("track"), UNKNOWN(lti->ptszTrack),
		_T("year"), UNKNOWN(lti->ptszYear),
		_T("genre"), UNKNOWN(lti->ptszGenre),
		_T("length"), UNKNOWN(lti->ptszLength),
		_T("player"), UNKNOWN(lti->ptszPlayer),
		_T("type"), UNKNOWN(lti->ptszType)
	};

	Buffer<TCHAR> ret;
	ReplaceTemplate(&ret, NULL, opts.templ, fr, MAX_REGS(fr));
	return (int) ret.detach();
}


int GetOverrideContactOption(WPARAM wParam,LPARAM lParam) 
{
	return (int) opts.override_contact_template;
}


int GetUnknownText(WPARAM wParam,LPARAM lParam) 
{
	return (int) opts.unknown;
}


void SetListeningInfos(LISTENINGTOINFO *lti)
{
	for (unsigned int i = 1; i < proto_itens.size(); ++i)
	{
		SetListeningInfo(proto_itens[i].proto, lti);
	}
}

static void CALLBACK GetInfoTimer(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
	if (hTimer != NULL)
	{
		KillTimer(NULL, hTimer);
		hTimer = NULL;
	}

	// Check if we can set it now...
	DWORD now = GetTickCount();
	if (now < lastInfoSetTime + MIN_TIME_BEETWEEN_SETS)
	{
		hTimer = SetTimer(NULL, NULL, lastInfoSetTime + MIN_TIME_BEETWEEN_SETS - now, GetInfoTimer);
		return;
	}
	lastInfoSetTime = GetTickCount(); // TODO Move this to inside the if that really sets

	if (!opts.enable_sending)
	{
//		m_log(_T("GetInfoTimer"), _T("!opts.enable_sending"));
		SetListeningInfos(NULL);
		return;
	}

	// Set it
	int changed = ChangedListeningInfo();
	if (changed < 0)
	{
//		m_log(_T("GetInfoTimer"), _T("changed < 0"));
		SetListeningInfos(NULL);
	}
	else if (changed > 0)
	{
//		m_log(_T("GetInfoTimer"), _T("changed > 0"));
		SetListeningInfos(GetListeningInfo());
	}

	StartTimer();
}

void StartTimer()
{
	// See if any protocol want Listening info
	BOOL want = FALSE;

	if (opts.enable_sending)
	{
		if (!players[WATRACK]->enabled)
		{
			// See if any player needs it
			BOOL needPoll = FALSE;
			int i;
			for (i = FIRST_PLAYER; i < NUM_PLAYERS; i++)
			{
				if (players[i]->needPoll)
				{
					needPoll = TRUE;
					break;
				}
			}

			if (needPoll)
			{
				// Now see protocols
				for (unsigned int i = 1; i < proto_itens.size(); ++i)
				{
					if (ListeningToEnabled(proto_itens[i].proto))
					{
						want = TRUE;
						break;
					}
				}
			}
		}
	}

	if (want)
	{
		if (hTimer == NULL)
			hTimer = SetTimer(NULL, NULL, opts.time_to_pool * 1000, GetInfoTimer);
	}
	else
	{
		if (hTimer != NULL)
		{
			KillTimer(NULL, hTimer);
			hTimer = NULL;

			// To be sure that no one was left behind
//			m_log(_T("StartTimer"), _T("To be sure that no one was left behind"));
			SetListeningInfos(NULL);
		}
	}
}

void HasNewListeningInfo()
{
	if (hTimer != NULL)
	{
		KillTimer(NULL, hTimer);
		hTimer = NULL;
	}

	hTimer = SetTimer(NULL, NULL, 100, GetInfoTimer);
}


int ClistExtraListRebuild(WPARAM wParam, LPARAM lParam)
{
	HICON hIcon = IcoLib_LoadIcon(ICON_NAME);

	hExtraImage = (HANDLE) CallService(MS_CLIST_EXTRA_ADD_ICON, (WPARAM) hIcon, 0);

	IcoLib_ReleaseIcon(hIcon);

	return 0;
}

void SetExtraIcon(HANDLE hContact, BOOL set)
{
	if (hExtraIcon != NULL)
	{
		ExtraIcon_SetIcon(hExtraIcon, hContact, set ? ICON_NAME : NULL);
	}
	else if (opts.show_adv_icon && hExtraImage != NULL)
	{
		IconExtraColumn iec;
		iec.cbSize = sizeof(iec);
		iec.hImage = set ? hExtraImage : (HANDLE)-1;
		if (opts.adv_icon_slot < 2)
		{
			iec.ColumnType = opts.adv_icon_slot + EXTRA_ICON_ADV1;
		}
		else 
		{
			int first = CallService(MS_CLUI_GETCAPS, 0, CLUIF2_USEREXTRASTART);
			iec.ColumnType = opts.adv_icon_slot - 2 + first;
		}

		CallService(MS_CLIST_EXTRA_SET_ICON, (WPARAM)hContact, (LPARAM)&iec);
	}
}

int SettingChanged(WPARAM wParam,LPARAM lParam)
{
	HANDLE hContact = (HANDLE) wParam;
	if (hContact == NULL)
		return 0;

	DBCONTACTWRITESETTING *cws = (DBCONTACTWRITESETTING*)lParam;
	if (strcmp(cws->szSetting, "ListeningTo") != 0)
		return 0;

	char *proto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
	if (proto == NULL)
		return 0;

	if (strcmp(cws->szModule, proto) != 0)
		return 0;

	if (cws->value.type == DBVT_DELETED || cws->value.ptszVal == NULL || cws->value.ptszVal[0] == 0)
		SetExtraIcon(hContact, FALSE);
	else
		SetExtraIcon(hContact, TRUE);

	return 0;
}


int SetNewSong(WPARAM wParam,LPARAM lParam)
{
	if (lParam == NULL)
		return -1;

	if (lParam == LISTENINGTO_ANSI)
	{
		CharToWchar data((char *) wParam);
		((GenericPlayer *) players[GENERIC])->NewData(data, wcslen(data));
	}
	else
	{
		WCHAR *data = (WCHAR *) wParam;
		((GenericPlayer *) players[GENERIC])->NewData(data, wcslen(data));
	}


	return 0;
}


TCHAR* VariablesParseInfo(ARGUMENTSINFO *ai)
{
	if (ai->cbSize < sizeof(ARGUMENTSINFO))
		return NULL;

	LISTENINGTOINFO *lti = GetListeningInfo();
	if (lti == NULL)
	{
		ai->flags = AIF_FALSE;
		return mir_tstrdup(_T(""));
	}

	TCHAR *fr[] = { 
		_T("artist"), UNKNOWN(lti->ptszArtist),
		_T("album"), UNKNOWN(lti->ptszAlbum),
		_T("title"), UNKNOWN(lti->ptszTitle),
		_T("track"), UNKNOWN(lti->ptszTrack),
		_T("year"), UNKNOWN(lti->ptszYear),
		_T("genre"), UNKNOWN(lti->ptszGenre),
		_T("length"), UNKNOWN(lti->ptszLength),
		_T("player"), UNKNOWN(lti->ptszPlayer),
		_T("type"), UNKNOWN(lti->ptszType)
	};

	Buffer<TCHAR> ret;
	ReplaceTemplate(&ret, NULL, opts.templ, fr, MAX_REGS(fr));
	return ret.detach();
}

#define VARIABLES_PARSE_BODY(__field__) \
	if (ai->cbSize < sizeof(ARGUMENTSINFO)) \
		return NULL; \
	\
	LISTENINGTOINFO *lti = GetListeningInfo(); \
	if (lti == NULL) \
	{ \
		ai->flags = AIF_FALSE; \
		return mir_tstrdup(_T("")); \
	} \
	else if (IsEmpty(lti->__field__))  \
	{ \
		ai->flags = AIF_FALSE; \
		return mir_tstrdup(opts.unknown); \
	} \
	else \
	{ \
		ai->flags = AIF_DONTPARSE; \
		TCHAR *ret = mir_tstrdup(lti->__field__); \
		return ret; \
	}


TCHAR* VariablesParseType(ARGUMENTSINFO *ai)
{
	VARIABLES_PARSE_BODY(ptszType);
}

TCHAR* VariablesParseArtist(ARGUMENTSINFO *ai)
{
	VARIABLES_PARSE_BODY(ptszArtist);
}

TCHAR* VariablesParseAlbum(ARGUMENTSINFO *ai)
{
	VARIABLES_PARSE_BODY(ptszAlbum);
}

TCHAR* VariablesParseTitle(ARGUMENTSINFO *ai)
{
	VARIABLES_PARSE_BODY(ptszTitle);
}

TCHAR* VariablesParseTrack(ARGUMENTSINFO *ai)
{
	VARIABLES_PARSE_BODY(ptszTrack);
}

TCHAR* VariablesParseYear(ARGUMENTSINFO *ai)
{
	VARIABLES_PARSE_BODY(ptszYear);
}

TCHAR* VariablesParseGenre(ARGUMENTSINFO *ai)
{
	VARIABLES_PARSE_BODY(ptszGenre);
}

TCHAR* VariablesParseLength(ARGUMENTSINFO *ai)
{
	VARIABLES_PARSE_BODY(ptszLength);
}

TCHAR* VariablesParsePlayer(ARGUMENTSINFO *ai)
{
	VARIABLES_PARSE_BODY(ptszPlayer);
}

