#include "globals.h"
#include "BorderState_v8_wrapper.h"
#include <v8.h>
#include "BorderState.h"

using namespace v8;


#ifdef UNICODE
# define V8_TCHAR uint16_t
#else
# define V8_TCHAR char
#endif


static Handle<Value> Get_BorderState_left(Local<String> property, const AccessorInfo &info) 
{
	Local<Object> self = info.Holder();
	Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
	BorderState *tmp = (BorderState *) wrap->Value();
	return Int32::New(tmp->getLeft());
}

static void Set_BorderState_left(Local<String> property, Local<Value> value, const AccessorInfo& info) 
{
	Local<Object> self = info.Holder();
	Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
	BorderState *tmp = (BorderState *) wrap->Value();
	tmp->setLeft(value->Int32Value());
}


static Handle<Value> Get_BorderState_right(Local<String> property, const AccessorInfo &info) 
{
	Local<Object> self = info.Holder();
	Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
	BorderState *tmp = (BorderState *) wrap->Value();
	return Int32::New(tmp->getRight());
}

static void Set_BorderState_right(Local<String> property, Local<Value> value, const AccessorInfo& info) 
{
	Local<Object> self = info.Holder();
	Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
	BorderState *tmp = (BorderState *) wrap->Value();
	tmp->setRight(value->Int32Value());
}


static Handle<Value> Get_BorderState_top(Local<String> property, const AccessorInfo &info) 
{
	Local<Object> self = info.Holder();
	Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
	BorderState *tmp = (BorderState *) wrap->Value();
	return Int32::New(tmp->getTop());
}

static void Set_BorderState_top(Local<String> property, Local<Value> value, const AccessorInfo& info) 
{
	Local<Object> self = info.Holder();
	Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
	BorderState *tmp = (BorderState *) wrap->Value();
	tmp->setTop(value->Int32Value());
}


static Handle<Value> Get_BorderState_bottom(Local<String> property, const AccessorInfo &info) 
{
	Local<Object> self = info.Holder();
	Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
	BorderState *tmp = (BorderState *) wrap->Value();
	return Int32::New(tmp->getBottom());
}

static void Set_BorderState_bottom(Local<String> property, Local<Value> value, const AccessorInfo& info) 
{
	Local<Object> self = info.Holder();
	Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
	BorderState *tmp = (BorderState *) wrap->Value();
	tmp->setBottom(value->Int32Value());
}


void AddBorderStateAcessors(Handle<ObjectTemplate> &templ)
{
	templ->SetAccessor(String::New("left"), Get_BorderState_left, Set_BorderState_left);
	templ->SetAccessor(String::New("right"), Get_BorderState_right, Set_BorderState_right);
	templ->SetAccessor(String::New("top"), Get_BorderState_top, Set_BorderState_top);
	templ->SetAccessor(String::New("bottom"), Get_BorderState_bottom, Set_BorderState_bottom);
}
