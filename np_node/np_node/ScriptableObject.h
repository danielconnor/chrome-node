#pragma once

#include "npapi.h"
#include "npruntime.h"


#define CREATE_CLASS(_class)		\
NPClass _class::_npclass = {       \
  NP_CLASS_STRUCT_VERSION,			\
  _class::Allocate,					\
  _class::_Deallocate,				\
  _class::_Invalidate,				\
  _class::_HasMethod,				\
  _class::_Invoke,					\
  _class::_InvokeDefault,			\
  _class::_HasProperty,				\
  _class::_GetProperty,				\
  _class::_SetProperty,				\
  _class::_RemoveProperty,			\
  _class::_Enumerate,				\
  _class::_Construct				\
}


struct CallbackParams {
	NPP npp;
	NPObject* object;
	NPVariant* args;
	int arg_len;
	const char* name;
};

static NPVariant* CopyNPVariant(const NPVariant* from) {
  NPVariant* copy = new NPVariant;
  if (NULL == copy) {
    return NULL;
  }
  *copy = *from;
  if (NPVARIANT_IS_OBJECT(*copy)) {
    // Increase the ref count.
    NPN_RetainObject(NPVARIANT_TO_OBJECT(*copy));
  } else if (NPVARIANT_IS_STRING(*copy)) {
    // Copy the string.
    NPUTF8* str = new NPUTF8[NPVARIANT_TO_STRING(*from).UTF8Length];
    if (NULL == str) {
      delete copy;
    }
    memcpy(str,
           NPVARIANT_TO_STRING(*from).UTF8Characters,
           NPVARIANT_TO_STRING(*from).UTF8Length);
    NPVARIANT_TO_STRING(*copy).UTF8Characters = str;
  }
  return copy;
}



class ScriptableObject : public NPObject
{
public:
    virtual void Deallocate();
    virtual void Invalidate();
    virtual bool HasMethod(NPIdentifier name);
    virtual bool Invoke(NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
    virtual bool InvokeDefault(const NPVariant *args, uint32_t argCount, NPVariant *result);
    virtual bool HasProperty(NPIdentifier name);
    virtual bool GetProperty(NPIdentifier name, NPVariant *result);
    virtual bool SetProperty(NPIdentifier name, const NPVariant *value);
    virtual bool RemoveProperty(NPIdentifier name);
    virtual bool Enumerate(NPIdentifier **identifier, uint32_t *count);
    virtual bool Construct(const NPVariant *args, uint32_t argCount, NPVariant *result);

public:
	ScriptableObject (NPP instance);
	void Detatch (void);

	static void _Deallocate(NPObject *npobj);
    static void _Invalidate(NPObject *npobj);
    static bool _HasMethod(NPObject *npobj, NPIdentifier name);
    static bool _Invoke(NPObject *npobj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
    static bool _InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result);
    static bool _HasProperty(NPObject * npobj, NPIdentifier name);
    static bool _GetProperty(NPObject *npobj, NPIdentifier name, NPVariant *result);
    static bool _SetProperty(NPObject *npobj, NPIdentifier name, const NPVariant *value);
    static bool _RemoveProperty(NPObject *npobj, NPIdentifier name);
    static bool _Enumerate(NPObject *npobj, NPIdentifier **identifier, uint32_t *count);
    static bool _Construct(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result);
 
	void fireCallback(const char* name,NPVariant args[],int arg_len,bool async);
	void fireCallback(const char* name,NPVariant args[],int arg_len);
	void apply(NPObject* function,const NPVariant* args, int arg_len, NPVariant* result);

    NPP m_Instance;
};