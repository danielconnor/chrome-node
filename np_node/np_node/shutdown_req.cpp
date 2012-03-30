#include "shutdown_req.h"


CREATE_CLASS(ShutdownReq);

ShutdownReq::ShutdownReq(NPP npp) : ScriptableObject(npp) 
{
	shutdown_req.data = this;

	oncomplete_func = NPN_GetStringIdentifier("oncomplete");
	cb_func = NPN_GetStringIdentifier("cb");

	oncomplete_callback = new NPObject();
}
void ShutdownReq::init(TCPWrap* socket, NPVariant on_complete)
{
	SetProperty(oncomplete_func,&on_complete);
	this->socket = socket;
}

bool ShutdownReq::HasProperty(NPIdentifier name)
{
	return name == oncomplete_func;
}
bool ShutdownReq::HasMethod(NPIdentifier name)
{
	return name == oncomplete_func;
}

bool ShutdownReq::GetProperty(NPIdentifier name, NPVariant *result)
{
	result->type = NPVariantType_Object;
	if(name == oncomplete_func && oncomplete_callback->_class != NULL) 
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
bool ShutdownReq::SetProperty(NPIdentifier name, const NPVariant *value)
{
	if(value->type == NPVariantType_Object) 
	{
		if(name == oncomplete_func) 
		{
			NPN_ReleaseObject(oncomplete_callback);
			oncomplete_callback = value->value.objectValue;
			NPN_RetainObject(oncomplete_callback);
		}
	}
	return true;
}
bool ShutdownReq::Invoke(NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	if(name == oncomplete_func) 
	{	
		NPN_InvokeDefault(m_Instance,oncomplete_callback,args,argCount,result);
	}
	return true;
}

NPObject *ShutdownReq::Allocate(NPP npp, NPClass *aClass) 
{
	return (NPObject*)new ShutdownReq(npp);
}