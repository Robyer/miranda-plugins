/*
 Copyright (C) 2009 Ricardo Pescuma Domenecci

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

PLUGININFOEX pluginInfo = {
		sizeof(PLUGININFOEX),
		"Extra Icons Service",
		PLUGIN_MAKE_VERSION(0,1,0,0),
		"Extra Icons Service",
		"Ricardo Pescuma Domenecci",
		"",
		"� 2009 Ricardo Pescuma Domenecci",
		"http://pescuma.org/miranda/extraicons",
		0,
		0, //doesn't replace anything built-in
		{ 0x112f7d30, 0xcd19, 0x4c74, { 0xa0, 0x3b, 0xbf, 0xbb, 0x76, 0xb7, 0x5b, 0xc4 } } // {112F7D30-CD19-4c74-A03B-BFBB76B75BC4}
};

HINSTANCE hInst;
PLUGINLINK *pluginLink;
MM_INTERFACE mmi;
UTF8_INTERFACE utfi;

vector<HANDLE> hHooks;
vector<HANDLE> hServices;
vector<BaseExtraIcon*> registeredExtraIcons;
vector<BaseExtraIcon*> extraIconsByHandle;
vector<ExtraIcon*> extraIcons;

char *metacontacts_proto = NULL;
BOOL clistRebuildAlreadyCalled = FALSE;
BOOL clistApplyAlreadyCalled = FALSE;

int clistFirstSlot = 0;
int clistSlotCount = 0;

int ModulesLoaded(WPARAM wParam, LPARAM lParam);
int PreShutdown(WPARAM wParam, LPARAM lParam);
int IconsChanged(WPARAM wParam, LPARAM lParam);
int ClistExtraListRebuild(WPARAM wParam, LPARAM lParam);
int ClistExtraImageApply(WPARAM wParam, LPARAM lParam);
int ClistExtraClick(WPARAM wParam, LPARAM lParam);

int ExtraIcon_Register(WPARAM wParam, LPARAM lParam);
int ExtraIcon_SetIcon(WPARAM wParam, LPARAM lParam);

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

static const MUUID interfaces[] = { MIID_EXTRAICONSSERVICE, MIID_LAST };
extern "C" __declspec(dllexport) const MUUID* MirandaPluginInterfaces(void)
{
	return interfaces;
}

static void IcoLib_Register(char *name, char *section, char *description, int id)
{
	HICON hIcon = (HICON) CallService(MS_SKIN2_GETICON, 0, (LPARAM) name);
	if (hIcon != NULL)
		return;

	SKINICONDESC sid = { 0 };
	sid.cbSize = sizeof(SKINICONDESC);
	sid.flags = SIDF_TCHAR;
	sid.pszName = name;
	sid.pszSection = section;
	sid.pszDescription = description;

	int cx = GetSystemMetrics(SM_CXSMICON);
	sid.hDefaultIcon = (HICON) LoadImage(hInst, MAKEINTRESOURCE(id), IMAGE_ICON, cx, cx, LR_DEFAULTCOLOR | LR_SHARED);

	CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);

	if (sid.hDefaultIcon)
		DestroyIcon(sid.hDefaultIcon);
}

extern "C" int __declspec(dllexport) Load(PLUGINLINK *link)
{
	pluginLink = link;

	mir_getMMI(&mmi);
	mir_getUTFI(&utfi);

	DWORD ret = CallService(MS_CLUI_GETCAPS, CLUICAPS_FLAGS2, 0);
	clistFirstSlot = HIWORD(ret);
	clistSlotCount = LOWORD(ret);


	// Icons
	IcoLib_Register("AlwaysVis", "Contact List", "Always Visible", IDI_ALWAYSVIS);
	IcoLib_Register("NeverVis", "Contact List", "Never Visible", IDI_NEVERVIS);
	IcoLib_Register("ChatActivity", "Contact List", "Chat Activity", IDI_CHAT);
	IcoLib_Register("gender_male", "Contact List", "Male", IDI_MALE);
	IcoLib_Register("gender_female", "Contact List", "Female", IDI_FEMALE);


	// Hooks
	hHooks.push_back(HookEvent(ME_SYSTEM_MODULESLOADED, &ModulesLoaded));
	hHooks.push_back(HookEvent(ME_SYSTEM_PRESHUTDOWN, &PreShutdown));
	hHooks.push_back(HookEvent(ME_CLIST_EXTRA_LIST_REBUILD, &ClistExtraListRebuild));
	hHooks.push_back(HookEvent(ME_CLIST_EXTRA_IMAGE_APPLY, &ClistExtraImageApply));
	hHooks.push_back(HookEvent(ME_CLIST_EXTRA_CLICK, &ClistExtraClick));


	// Services
	hServices.push_back(CreateServiceFunction(MS_EXTRAICON_REGISTER, &ExtraIcon_Register));
	hServices.push_back(CreateServiceFunction(MS_EXTRAICON_SET_ICON, &ExtraIcon_SetIcon));

	DefaultExtraIcons_Load();

	return 0;
}

extern "C" int __declspec(dllexport) Unload(void)
{
	return 0;
}

// Called when all the modules are loaded
int ModulesLoaded(WPARAM wParam, LPARAM lParam)
{
	if (ServiceExists(MS_MC_GETPROTOCOLNAME))
		metacontacts_proto = (char *) CallService(MS_MC_GETPROTOCOLNAME, 0, 0);


	// add our modules to the KnownModules list
	CallService("DBEditorpp/RegisterSingleModule", (WPARAM) MODULE_NAME, 0);


	// updater plugin support
	if (ServiceExists(MS_UPDATE_REGISTER))
	{
		Update upd = { 0 };
		char szCurrentVersion[30];

		upd.cbSize = sizeof(upd);
		upd.szComponentName = pluginInfo.shortName;

		upd.szUpdateURL = UPDATER_AUTOREGISTER;

		upd.szBetaVersionURL = "http://pescuma.org/miranda/extraicons_version.txt";
		upd.szBetaChangelogURL = "http://pescuma.org/miranda/extraicons#Changelog";
		upd.pbBetaVersionPrefix = (BYTE *) "Extra Icons Service ";
		upd.cpbBetaVersionPrefix = strlen((char *) upd.pbBetaVersionPrefix);
		upd.szBetaUpdateURL = "http://pescuma.org/miranda/extraicons.zip";

		upd.pbVersion = (BYTE *) CreateVersionStringPlugin((PLUGININFO*) &pluginInfo, szCurrentVersion);
		upd.cpbVersion = strlen((char *) upd.pbVersion);

		CallService(MS_UPDATE_REGISTER, 0, (LPARAM)&upd);
	}

	hHooks.push_back(HookEvent(ME_SKIN2_ICONSCHANGED, &IconsChanged));

	InitOptions();

	return 0;
}

int IconsChanged(WPARAM wParam, LPARAM lParam)
{
	return 0;
}

int PreShutdown(WPARAM wParam, LPARAM lParam)
{
	DefaultExtraIcons_Unload();

	unsigned int i;
	for (i = 0; i < hServices.size(); i++)
		DestroyServiceFunction(hServices[i]);

	for (i = 0; i < hHooks.size(); i++)
		UnhookEvent(hHooks[i]);

	DeInitOptions();

	return 0;
}

int GetNumberOfSlots()
{
	return clistSlotCount;
}

int ConvertToClistSlot(int slot)
{
	if (slot < 0)
		return slot;

	return clistFirstSlot + slot;
}

int Clist_SetExtraIcon(HANDLE hContact, int slot, HANDLE hImage)
{
	IconExtraColumn iec = { 0 };
	iec.cbSize = sizeof(iec);
	iec.ColumnType = ConvertToClistSlot(slot);
	iec.hImage = (hImage == NULL ? (HANDLE) -1 : hImage);

	return CallService(MS_CLIST_EXTRA_SET_ICON, (WPARAM) hContact, (LPARAM) &iec);
}

ExtraIcon *GetExtraIcon(HANDLE id)
{
	unsigned int i = (int) id;

	if (i < 1 || i > extraIconsByHandle.size())
		return NULL;

	return extraIconsByHandle[i - 1];
}

ExtraIcon * GetExtraIconBySlot(int slot)
{
	for (unsigned int i = 0; i < extraIcons.size(); ++i)
	{
		ExtraIcon *extra = extraIcons[i];
		if (extra->getSlot() == slot)
			return extra;
	}
	return NULL;
}

struct compareFunc : std::binary_function<const ExtraIcon *, const ExtraIcon *, bool>
{
	bool operator()(const ExtraIcon * one, const ExtraIcon * two) const
	{
		return *one < *two;
	}
};

int ExtraIcon_Register(WPARAM wParam, LPARAM lParam)
{
	if (wParam == 0)
		return 0;

	EXTRAICON_INFO *ei = (EXTRAICON_INFO *) wParam;
	if (ei->cbSize < (int) sizeof(EXTRAICON_INFO))
		return 0;
	if (ei->type != EXTRAICON_TYPE_CALLBACK && ei->type != EXTRAICON_TYPE_ICOLIB)
		return 0;
	if (IsEmpty(ei->name) || IsEmpty(ei->description))
		return 0;
	if (ei->type == EXTRAICON_TYPE_CALLBACK && (ei->ApplyIcon == NULL || ei->RebuildIcons == NULL))
		return 0;

	const char *desc = Translate(ei->description);

	for (unsigned int i = 0; i < registeredExtraIcons.size(); ++i)
	{
		BaseExtraIcon *extra = registeredExtraIcons[i];
		if (strcmp(ei->name, extra->getName()) != 0)
			continue;

		if (ei->type != extra->getType())
			return 0;

		// Found one, now merge it

		if (stricmp(extra->getDescription(), desc))
		{
			// TODO Handle group
			string newDesc = extra->getDescription();
			newDesc += " / ";
			newDesc += desc;
			extra->setDescription(newDesc.c_str());
		}

		if (IsEmpty(extra->getDescIcon()) && !IsEmpty(ei->descIcon))
			extra->setDescIcon(ei->descIcon);

		return i + 1;
	}

	int id = registeredExtraIcons.size() + 1;

	BaseExtraIcon *extra;
	switch (ei->type)
	{
		case EXTRAICON_TYPE_CALLBACK:
			extra = new CallbackExtraIcon(id, ei->name, desc, ei->descIcon == NULL ? "" : ei->descIcon,
					ei->RebuildIcons, ei->ApplyIcon, ei->OnClick, ei->onClickParam);
			break;
		case EXTRAICON_TYPE_ICOLIB:
			extra = new IcolibExtraIcon(id, ei->name, desc, ei->descIcon == NULL ? "" : ei->descIcon, ei->OnClick,
					ei->onClickParam);
			break;
		default:
			return 0;
	}

	char setting[512];
	mir_snprintf(setting, MAX_REGS(setting), "Position_%s", ei->name);
	extra->setPosition(DBGetContactSettingWord(NULL, MODULE_NAME, setting, 1000));

	mir_snprintf(setting, MAX_REGS(setting), "Slot_%s", ei->name);
	int slot = DBGetContactSettingWord(NULL, MODULE_NAME, setting, 1);
	if (slot == (WORD) -1)
		slot = -1;
	extra->setSlot(slot);

	registeredExtraIcons.push_back(extra);
	extraIconsByHandle.push_back(extra);
	extraIcons.push_back(extra);

	if (slot >= 0)
	{
		vector<BaseExtraIcon *> tmp;
		tmp = registeredExtraIcons;
		std::sort(tmp.begin(), tmp.end(), compareFunc());

		if (clistRebuildAlreadyCalled)
			extra->rebuildIcons();

		slot = 0;
		for (unsigned int i = 0; i < tmp.size(); ++i)
		{
			ExtraIcon *ex = tmp[i];
			if (ex->getSlot() < 0)
				continue;

			int oldSlot = ex->getSlot();
			ex->setSlot(slot++);

			if (clistApplyAlreadyCalled && (ex == extra || oldSlot != slot))
				extra->applyIcons();
		}
	}

	return id;
}

int ExtraIcon_SetIcon(WPARAM wParam, LPARAM lParam)
{
	if (wParam == 0)
		return -1;

	EXTRAICON *ei = (EXTRAICON *) wParam;
	if (ei->cbSize < (int) sizeof(EXTRAICON))
		return -1;
	if (ei->hExtraIcon == NULL || ei->hContact == NULL)
		return -1;

	ExtraIcon *extra = GetExtraIcon(ei->hExtraIcon);
	if (extra == NULL)
		return -1;

	return extra->setIcon((int) ei->hExtraIcon, ei->hContact, ei->hImage);
}

int ClistExtraListRebuild(WPARAM wParam, LPARAM lParam)
{
	clistRebuildAlreadyCalled = TRUE;

	ResetIcons();

	for (unsigned int i = 0; i < registeredExtraIcons.size(); ++i)
		registeredExtraIcons[i]->rebuildIcons();

	return 0;
}

int ClistExtraImageApply(WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact = (HANDLE) wParam;
	if (hContact == NULL)
		return 0;

	clistApplyAlreadyCalled = TRUE;

	for (unsigned int i = 0; i < registeredExtraIcons.size(); ++i)
		registeredExtraIcons[i]->applyIcon(hContact);

	return 0;
}

int ClistExtraClick(WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact = (HANDLE) wParam;
	if (hContact == NULL)
		return 0;

	int extra = (int) lParam;

	for (unsigned int i = 0; i < registeredExtraIcons.size(); ++i)
	{
		if (ConvertToClistSlot(registeredExtraIcons[i]->getSlot()) == extra)
		{
			registeredExtraIcons[i]->onClick(hContact);
			break;
		}
	}

	return 0;
}
