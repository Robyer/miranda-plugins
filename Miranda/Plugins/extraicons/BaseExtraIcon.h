#ifndef __BASEEXTRAICON_H__
#define __BASEEXTRAICON_H__

#include "ExtraIcon.h"

class BaseExtraIcon : public ExtraIcon
{
public:
	BaseExtraIcon(int id, const char *name, const char *description, const char *descIcon, MIRANDAHOOKPARAM OnClick,
			LPARAM param);
	virtual ~BaseExtraIcon();

	virtual const char *getDescription() const;
	virtual void setDescription(const char *desc);
	virtual const char *getDescIcon() const;
	virtual void setDescIcon(const char *icon);
	virtual int getType() const =0;

	virtual void onClick(HANDLE hContact);


protected:
	std::string description;
	std::string descIcon;
	MIRANDAHOOKPARAM OnClick;
	LPARAM onClickParam;
};

#endif // __BASEEXTRAICON_H__