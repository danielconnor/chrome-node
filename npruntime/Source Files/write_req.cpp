#include "write_req.h"


NPClass WriteReq::_npclass = {                              
  NP_CLASS_STRUCT_VERSION,
  WriteReq::Allocate,
  WriteReq::_Deallocate,
  WriteReq::_Invalidate,
  WriteReq::_HasMethod,
  WriteReq::_Invoke,
  WriteReq::_InvokeDefault,
  WriteReq::_HasProperty,
  WriteReq::_GetProperty,
  WriteReq::_SetProperty,
  WriteReq::_RemoveProperty,
  WriteReq::_Enumerate,
  WriteReq::_Construct
};

WriteReq::WriteReq(NPP npp) : ScriptableObject(npp) 
{
	this->write_req = new uv_write_t;
	write_req->data = this;
	addProperty("oncomplete");
	addMethod("oncomplete");
	addProperty("cb");
	addMethod("cb");

	oncomplete_callback = new NPObject();
	cb_callback = new NPObject();
	retainBuffer = false;
}
void WriteReq::init(TCPWrap* socket)
{
	this->socket = socket;
}

bool WriteReq::GetProperty(NPIdentifier name, NPVariant *result)
{
	result->type = NPVariantType_Object;
	if(name == properties["cb"] && cb_callback->_class != NULL) 
	{
		result->value.objectValue = cb_callback;
		return true;
	}
	else if(name == properties["oncomplete"] && oncomplete_callback->_class != NULL) 
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
		if(name == properties["cb"]) 
		{
			NPN_ReleaseObject(cb_callback);
			cb_callback = value->value.objectValue;
			NPN_RetainObject(cb_callback);
		}
		if(name == properties["oncomplete"]) 
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
	if(name == methods["cb"]) 
	{
		NPN_InvokeDefault(m_Instance,cb_callback,args,argCount,result);
	}
	if(name == methods["oncomplete"]) 
	{	
		NPN_InvokeDefault(m_Instance,oncomplete_callback,args,argCount,result);
	}
	return true;
}

NPObject *WriteReq::Allocate(NPP npp, NPClass *aClass) 
{
	return (NPObject*)new WriteReq(npp);
}