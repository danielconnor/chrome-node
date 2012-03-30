#include "write_req.h"


CREATE_CLASS(WriteReq);

WriteReq::WriteReq(NPP npp) : ScriptableObject(npp) 
{
	write_req.data = this;

	oncomplete_func = NPN_GetStringIdentifier("oncomplete");
	cb_func = NPN_GetStringIdentifier("cb");

	oncomplete_callback = new NPObject();
	cb_callback = new NPObject();

	retainBuffer = false;
}
void WriteReq::init(TCPWrap* socket, NPVariant on_complete, NPVariant cb)
{
	SetProperty(oncomplete_func,&on_complete);
	SetProperty(cb_func,&cb);
	this->socket = socket;
}

bool WriteReq::HasProperty(NPIdentifier name)
{
	return name == oncomplete_func ||
			name == cb_func;
}
bool WriteReq::HasMethod(NPIdentifier name)
{
	return name == oncomplete_func ||
			name == cb_func;
}

bool WriteReq::GetProperty(NPIdentifier name, NPVariant *result)
{
	result->type = NPVariantType_Object;
	if(name == cb_func && cb_callback->_class != NULL) 
	{
		result->value.objectValue = cb_callback;
		return true;
	}
	else if(name == oncomplete_func && oncomplete_callback->_class != NULL) 
	{
		result->value.objectValue = oncomplete_callback;
		return true;
	}
	else 
	{
		result->type = NPVariantType_Void;
		return true;
	}
	return false;
}
bool WriteReq::SetProperty(NPIdentifier name, const NPVariant *value)
{
	if(value->type == NPVariantType_Object) 
	{
		if(name == cb_func) 
		{
			NPN_ReleaseObject(cb_callback);
			cb_callback = value->value.objectValue;
			NPN_RetainObject(cb_callback);
		}
		if(name == oncomplete_func) 
		{
			NPN_ReleaseObject(oncomplete_callback);
			oncomplete_callback = value->value.objectValue;
			NPN_RetainObject(oncomplete_callback);
		}
	}
	return true;
}
bool WriteReq::Invoke(NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	if(name == cb_func) 
	{
		NPN_InvokeDefault(m_Instance,cb_callback,args,argCount,result);
	}
	if(name == oncomplete_func) 
	{	
		NPN_InvokeDefault(m_Instance,oncomplete_callback,args,argCount,result);
	}
	return true;
}

NPObject *WriteReq::Allocate(NPP npp, NPClass *aClass) 
{
	return (NPObject*)new WriteReq(npp);
}