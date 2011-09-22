#pragma once

#ifndef _WRITE_REQ
#define _WRITE_REQ

#include "tcp_wrap.h"

class WriteReq : public ScriptableObject {

public:
	TCPWrap* socket;
	uv_write_t* write_req;
	WriteReq(NPP instance);

	void init(TCPWrap* socket);

	// callbacks
	NPObject* oncomplete_callback;
	NPObject* cb_callback;

	char* data;

	bool retainBuffer;

	bool SetProperty(NPIdentifier name, const NPVariant *value);
	bool GetProperty(NPIdentifier name, NPVariant *result);
	bool Invoke(NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);


	static NPObject *Allocate(NPP npp, NPClass *aClass);
	static NPClass _npclass;
};



#endif