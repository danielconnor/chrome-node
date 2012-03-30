#pragma once

#ifndef _SHUTDOWN_REQ
#define _SHUTDOWN_REQ

#include "tcp_wrap.h"

class ShutdownReq : public ScriptableObject {

public:
	// holds the socket that the request is being executed on
	TCPWrap* socket;
	//holds the uv write request
	uv_shutdown_t shutdown_req;
	ShutdownReq(NPP);

	void init(TCPWrap*, NPVariant);

	// callbacks
	NPObject* oncomplete_callback;

	bool HasProperty(NPIdentifier);
	bool SetProperty(NPIdentifier, const NPVariant *);
	bool GetProperty(NPIdentifier, NPVariant *);

	bool HasMethod(NPIdentifier );
	bool Invoke(NPIdentifier, const NPVariant *, uint32_t , NPVariant *);

	NPIdentifier oncomplete_func;
	NPIdentifier cb_func;

	static NPObject *Allocate(NPP, NPClass *);
	static NPClass _npclass;
};



#endif