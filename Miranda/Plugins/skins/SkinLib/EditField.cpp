#include "globals.h"
#include "EditField.h"
#include "EditFieldState.h"


EditField::EditField(const char *name, HWND hwnd) : ControlField(name, hwnd)
{
}

EditField::~EditField()
{
}

FieldType EditField::getType() const
{
	return CONTROL_EDIT;
}

FieldState * EditField::createState()
{
	return new EditFieldState(this);
}