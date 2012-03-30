#pragma once

#ifndef _WRITE_REQ
#define _WRITE_REQ

#include "tcp_wrap.h"

class WriteReq : public ScriptableObject {

public:
	// holds the socket that the request is being executed on
	TCPWrap* socket;
	//holds the uv write request
	uv_write_t write_req;
	WriteReq(NPP instance);

	void init(TCPWrap* socket,NPVariant on_complete, NPVariant cb);

	// callbacks
	NPObject* oncomplete_callback;
	NPObject* cb_callback;

	// holds a pointer to the data to be written
	char* data;

	//if a buffer is being written this is set to true
	//because it shouldnt be deleted unlike if it is a string
	bool retainBuffer;

	bool HasProperty(NPIdentifier name);
	bool SetProperty(NPIdentifier name, const NPVariant *value);
	bool GetProperty(NPIdentifier name, NPVariant *result);

	bool HasMethod(NPIdentifier name);
	bool Invoke(NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);

	NPIdentifier oncomplete_func;
	NPIdentifier cb_func;

	static NPObject *Allocate(NPP npp, NPClass *aClass);
	static NPClass _npclass;
};



#endif