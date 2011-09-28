#include <stdio.h>

#include "plugin.h"
#include "tcp_wrap.h"
#include "buffer_wrap.h"
#include "parser_wrap.h"
#include "console.h"
#include "npfunctions.h"


class ScriptablePluginObject : public ScriptableObject
{
public:
	ScriptablePluginObject(NPP npp) : ScriptableObject(npp)
	{
		new_socket_function = NPN_GetStringIdentifier("createSocket");
		new_parser_function = NPN_GetStringIdentifier("createParser");
		new_buffer_function = NPN_GetStringIdentifier("createBuffer");
	}

    static NPClass _npclass;

	virtual bool HasMethod(NPIdentifier name);
	virtual bool HasProperty(NPIdentifier name);
	virtual bool GetProperty(NPIdentifier name, NPVariant *result);
	virtual bool Invoke(NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
	virtual bool InvokeDefault(const NPVariant *args, uint32_t argCount, NPVariant *result);

	static NPObject* Allocate(NPP npp, NPClass *aClass);

	NPIdentifier new_socket_function;
	NPIdentifier new_parser_function;
	NPIdentifier new_buffer_function;
};

CREATE_CLASS(ScriptablePluginObject);


NPObject* ScriptablePluginObject::Allocate(NPP npp, NPClass *aClass)
{
	return new ScriptablePluginObject(npp);
}


bool ScriptablePluginObject::HasMethod(NPIdentifier name)
{
	return name == new_socket_function ||
		 name == new_parser_function ||
		 name == new_buffer_function;
}

bool ScriptablePluginObject::HasProperty(NPIdentifier name)
{
	return false;
}

bool ScriptablePluginObject::GetProperty(NPIdentifier name, NPVariant *result)
{
	return true;
}

bool ScriptablePluginObject::Invoke(NPIdentifier name, const NPVariant *args,
                               uint32_t argCount, NPVariant *result)
{
	result->type = NPVariantType_Object;
	if(name == new_socket_function) 
	{
		TCPWrap* w = (TCPWrap*)NPN_CreateObject(m_Instance,&TCPWrap::_npclass);
		w->init();
		result->value.objectValue = (NPObject*)w;
	}
	if(name == new_parser_function)
	{
		ParserWrap* p = (ParserWrap*)NPN_CreateObject(m_Instance,&ParserWrap::_npclass);
		p->init(args[0]);
		result->value.objectValue = (NPObject*)p;
	}
	if(name == new_buffer_function) 
	{
		BufferWrap* b = (BufferWrap*)NPN_CreateObject(m_Instance,&BufferWrap::_npclass);
		b->init(args[0].value.doubleValue);
		result->value.objectValue = (NPObject*)b;
	}
	NPN_RetainObject(result->value.objectValue);

	return true;
}

bool ScriptablePluginObject::InvokeDefault(const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	return true;
}

CPlugin::CPlugin(NPP pNPInstance) :
  m_pNPInstance(pNPInstance),
  m_pNPStream(NULL),
  m_bInitialized(false),
  m_pScriptableObject(NULL)
{
	NPN_GetValue(m_pNPInstance, NPNVWindowNPObject, &sWindowObj);

	// initialise uv and set up inter-thread communication
	uv_init();
	uv_async_init(&TCPWrap::async_handle, (uv_async_cb)TCPWrap::invoke_worker_thread);
	CreateThread(NULL,0,TCPWrap::run_uv,NULL,0,NULL);
}

CPlugin::~CPlugin()
{
	if (sWindowObj)
		NPN_ReleaseObject(sWindowObj);

	if (m_pScriptableObject)
		NPN_ReleaseObject(m_pScriptableObject);
}


void CPlugin::shut()
{
	m_bInitialized = false;
}

NPBool CPlugin::isInitialized()
{
	return m_bInitialized;
}

int16_t CPlugin::handleEvent(void* event)
{
	return 0;
}

void CPlugin::getVersion(char* *aVersion)
{
	const char *ua = NPN_UserAgent(m_pNPInstance);
	char*& version = *aVersion;
	version = (char*)NPN_MemAlloc(1 + strlen(ua));
	if (version)
		strcpy(version, ua);
}

NPObject *CPlugin::GetScriptableObject()
{
	if (!m_pScriptableObject) {
		m_pScriptableObject = NPN_CreateObject(m_pNPInstance, &ScriptablePluginObject::_npclass);
	}

	if (m_pScriptableObject) {
		NPN_RetainObject(m_pScriptableObject);
	}

	return m_pScriptableObject;
}