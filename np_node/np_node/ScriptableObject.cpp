#include <iostream>
#include <string>
#include <vector>
#include <stdio.h>
#include <sys/stat.h>

#include "buffer_wrap.h"
#include "plugin.h"

ScriptableObject::ScriptableObject (NPP instance) 
{
	this->m_Instance = instance;
}

void ExecuteCallback(void* args) 
{	
	CallbackParams* params = (CallbackParams*)args;

	for(int i = 0; i < params->arg_len;i++) 
	{
		if(params->args[i].type == NPVariantType_String) 
		{
			NPString s = params->args[i].value.stringValue;

			BufferWrap* b = (BufferWrap*)NPN_CreateObject(params->npp,&BufferWrap::_npclass);
			b->init(s.UTF8Length);

			memcpy(b->data,s.UTF8Characters,s.UTF8Length);
			delete[] params->args[i].value.stringValue.UTF8Characters;
			params->args[i].type = NPVariantType_Object;
			params->args[i].value.objectValue = (NPObject*)b;
		}
	}

	NPVariant result;

	//calls Invoke directly instead of NPN_Invoke
	//doesn't need to call NPN_HasMethod along the way because we know
	//it exists already
	((ScriptableObject*)params->object)->Invoke(NPN_GetStringIdentifier(params->name),params->args,params->arg_len,&result);

	delete[] params->args;
	delete params;
}

void ScriptableObject::fireCallback(const char* name,NPVariant args[],int arg_len)
{
	fireCallback(name,args,arg_len,true);
}
void ScriptableObject::fireCallback(const char* name,NPVariant args[],int arg_len,bool async)
{
	CallbackParams* params = new CallbackParams;
	params->arg_len = arg_len;
	params->args = args;
	params->npp = m_Instance;
	params->object = (NPObject*)this;
	params->name = name;
	if(async) 
	{
		NPN_PluginThreadAsyncCall(m_Instance, ExecuteCallback, params);
	}
	else 
	{
		NPVariant result;
		NPN_Invoke(m_Instance,(NPObject*)this,NPN_GetStringIdentifier(name),args,arg_len,&result);
	}
}
void ScriptableObject::apply(NPObject* function,const NPVariant* args, int arg_len, NPVariant* result)
{
	NPVariant* c_args = new NPVariant[arg_len + 1];
	c_args[0].type = NPVariantType_Object;
	c_args[0].value.objectValue = this;
	
	for(int i = 0; i < arg_len; i++) 
	{
		c_args[i + 1] = args[i];
	}

	NPN_Invoke(m_Instance,function,NPN_GetStringIdentifier("call"),c_args,arg_len + 1,result);
	delete[] c_args;
}

void ScriptableObject::Deallocate()
{

}

void ScriptableObject::Invalidate()
{
}

bool ScriptableObject::HasMethod(NPIdentifier name)
{
	return false;
}
bool ScriptableObject::Invoke(NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  return false;
}

bool ScriptableObject::InvokeDefault(const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	return false;
}

bool ScriptableObject::HasProperty(NPIdentifier name)
{
	return false;
}

bool ScriptableObject::GetProperty(NPIdentifier name, NPVariant *result)
{
	return false;
}

bool ScriptableObject::SetProperty(NPIdentifier name, const NPVariant *value)
{
	return false;
}

bool ScriptableObject::RemoveProperty(NPIdentifier name)
{
	return false;
}

bool ScriptableObject::Enumerate(NPIdentifier **identifier, uint32_t *count)
{
	return false;
}

bool ScriptableObject::Construct(const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	return true;
}

void ScriptableObject::Detatch (void)
{
	m_Instance = NULL;
}

void ScriptableObject::_Deallocate(NPObject *npobj)
{
	ScriptableObject *pObj = ((ScriptableObject *) npobj);
	pObj->Deallocate ();
	delete pObj;
}

void ScriptableObject::_Invalidate(NPObject *npobj)
{
	((ScriptableObject*)npobj)->Invalidate();
}

bool ScriptableObject::_HasMethod(NPObject *npobj, NPIdentifier name)
{
	return ((ScriptableObject*)npobj)->HasMethod (name);
}

bool ScriptableObject::_Invoke(NPObject *npobj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	return ((ScriptableObject*)npobj)->Invoke (name, args, argCount, result);
}

bool ScriptableObject::_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	return ((ScriptableObject*)npobj)->InvokeDefault (args, argCount, result);
}

bool ScriptableObject::_HasProperty(NPObject * npobj, NPIdentifier name)
{
	return ((ScriptableObject*)npobj)->HasProperty (name);
}

bool ScriptableObject::_GetProperty(NPObject *npobj, NPIdentifier name, NPVariant *result)
{
	return ((ScriptableObject*)npobj)->GetProperty (name, result);
}

bool ScriptableObject::_SetProperty(NPObject *npobj, NPIdentifier name, const NPVariant *value)
{
	return ((ScriptableObject*)npobj)->SetProperty (name, value);
}

bool ScriptableObject::_RemoveProperty(NPObject *npobj, NPIdentifier name)
{
	return ((ScriptableObject*)npobj)->RemoveProperty (name);
}

bool ScriptableObject::_Enumerate(NPObject *npobj, NPIdentifier **identifier, uint32_t *count)
{
	return ((ScriptableObject*)npobj)->Enumerate (identifier, count);
}

bool ScriptableObject::_Construct(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	return ((ScriptableObject*)npobj)->Construct (args, argCount, result);
}