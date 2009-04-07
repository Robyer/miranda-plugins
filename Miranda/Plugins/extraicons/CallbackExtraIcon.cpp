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

CallbackExtraIcon::CallbackExtraIcon(const char *name, void(*RebuildIcons)(), void(*ApplyIcon)(HANDLE hContact,
		int slot), const char *description, const char *descIcon) :
	ExtraIcon(name, description, descIcon), RebuildIcons(RebuildIcons), ApplyIcon(ApplyIcon), needToRebuild(true)
{
}

CallbackExtraIcon::~CallbackExtraIcon()
{
}

int CallbackExtraIcon::getType() const
{
	return EXTRAICON_TYPE_CALLBACK;
}

bool CallbackExtraIcon::needToRebuildIcons()
{
	return needToRebuild;
}

void CallbackExtraIcon::rebuildIcons()
{
	if (!isEnabled())
	{
		needToRebuild = true;
		return;
	}
	needToRebuild = false;

	RebuildIcons();
}

void CallbackExtraIcon::applyIcon(HANDLE hContact)
{
	if (!isEnabled() || hContact == NULL)
		return;

	ApplyIcon(hContact, ConvertToClistSlot(slot));
}

int CallbackExtraIcon::setIcon(HANDLE hContact, void *icon)
{
	if (!isEnabled() || hContact == NULL)
		return -1;

	return Clist_SetExtraIcon(hContact, slot, (HANDLE) icon);
}
