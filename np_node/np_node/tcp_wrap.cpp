#include "tcp_wrap.h"
#include "write_req.h"
#include "buffer_wrap.h"


	CREATE_CLASS(TCPWrap);

uv_async_t TCPWrap::async_handle;

DWORD WINAPI TCPWrap::run_uv(LPVOID lpParam) 
{
	uv_run();
	return 0;
}

TCPWrap::TCPWrap(NPP instance) : ScriptableObject(instance)
{
}
void TCPWrap::init()
{
	init(new uv_tcp_t);
}
void TCPWrap::init(uv_tcp_t* stream)
{
	bind_func = NPN_GetStringIdentifier("bind");

	listen_func = NPN_GetStringIdentifier("listen");

	readstart_func = NPN_GetStringIdentifier("readStart");

	readstop_func = NPN_GetStringIdentifier("readStop");

	write_func = NPN_GetStringIdentifier("write");

	close_func = NPN_GetStringIdentifier("close");

	connect_func = NPN_GetStringIdentifier("connect");

	onconnection_cb = NPN_GetStringIdentifier("onconnection");

	onread_cb = NPN_GetStringIdentifier("onread");

	socket_prop = NPN_GetStringIdentifier("socket");

	uv_tcp_init(stream);

	onconnection_callback = new NPObject();
	onread_callback = new NPObject();
	socket_pointer = new NPObject();

	this->stream = stream;
	this->stream->data = (void *)this;
}

bool TCPWrap::HasMethod(NPIdentifier name)
{
	return name == onread_cb ||
			name == onconnection_cb ||
			name == connect_func ||
			name == close_func ||
			name == write_func ||
			name == listen_func ||
			name == readstart_func ||
			name == readstop_func ||
			name == bind_func ||
			name == listen_func;
}
bool TCPWrap::HasProperty(NPIdentifier name)
{
	return name == onconnection_cb ||
			name == onread_cb ||
			name == socket_prop;
}
bool TCPWrap::GetProperty(NPIdentifier name, NPVariant *result)
{
	result->type = NPVariantType_Object;
	if(name == onread_cb) 
	{
		result->value.objectValue = onconnection_callback;
		return true;
	}
	if(name == onconnection_cb) 
	{
		result->value.objectValue = onread_callback;
		return true;
	}
	if(name == socket_prop)
	{
		result->value.objectValue = socket_pointer;
		return true;
	}
	return false;
}
bool TCPWrap::SetProperty(NPIdentifier name, const NPVariant *value)
{
	if(name == onread_cb) 
	{
		NPN_ReleaseObject(onread_callback);
		onread_callback = value->value.objectValue;
		NPN_RetainObject(onread_callback);
	}
	if(name == onconnection_cb) 
	{
		NPN_ReleaseObject(onconnection_callback);
		onconnection_callback = value->value.objectValue;
		NPN_RetainObject(onconnection_callback);
	}
	if(name == socket_prop)
	{
		NPN_ReleaseObject(socket_pointer);
		socket_pointer = value->value.objectValue;
		NPN_RetainObject(socket_pointer);
	}
	return true;
}



TCPWrap::~TCPWrap()
{
	delete stream;
}

NPObject *TCPWrap::Allocate(NPP npp, NPClass *aClass) 
{
	return (NPObject*)new TCPWrap(npp);
}

bool TCPWrap::Invoke(NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	if(name == onconnection_cb) 
	{
		apply(onconnection_callback,args,argCount,result);
		return true;
	}
	if(name == onread_cb) 
	{
		apply(onread_callback,args,argCount,result);
		return true;
	}

	InvokeParams* params = new InvokeParams;
	params->name = name;
	params->object = this;
	params->next = NULL;


	if(name == write_func) 
	{
		params->args = new NPVariant[argCount + 1];
		params->argCount = argCount + 1;
	}
	else
	{
		params->args = new NPVariant[argCount];
		params->argCount = argCount;
	}


	if(currentParamWrite == NULL) 
	{
		currentParamRead = new InvokeParams;
		currentParamRead->next = currentParamWrite = params;
	}
	else
	{
		currentParamWrite->next = params;
		currentParamWrite = params;
	}

	
	for(uint32_t q = 0; q < argCount; q++) 
	{
		params->args[q] = *CopyNPVariant(&args[q]);
	}
	if(name == write_func) 
	{
		argCount++;
		WriteReq* w = (WriteReq*)NPN_CreateObject(m_Instance,&WriteReq::_npclass);


		// when we get a buffer we have to find the SlowBuffer._handle to access its data
		if(params->args[0].type == NPVariantType_Object)
		{
			// we dont want to delete the buffer because it's a SlowBuffer and might be accessed later
			// via a Buffer
			w->retainBuffer = true;
			NPVariant offset,length,parent,handle;

			// buffer.offset
			NPN_GetProperty(m_Instance,params->args[0].value.objectValue,NPN_GetStringIdentifier("offset"),&offset);
			// buffer.length
			NPN_GetProperty(m_Instance,params->args[0].value.objectValue,NPN_GetStringIdentifier("length"),&length);

			// buffer.parent
			NPN_GetProperty(m_Instance,params->args[0].value.objectValue,NPN_GetStringIdentifier("parent"),&parent);
			// buffer.parent._handle
			NPN_GetProperty(m_Instance,parent.value.objectValue,NPN_GetStringIdentifier("_handle"),&handle);

			params->args[0].type = NPVariantType_String;
			params->args[0].value.stringValue.UTF8Length = static_cast<uint32_t>(length.value.doubleValue);
			params->args[0].value.stringValue.UTF8Characters = 
				((BufferWrap*)handle.value.objectValue)->data + (uint16_t)offset.value.doubleValue;
		}
		
		w->init(this,args[1],args[2]);
		NPN_RetainObject((NPObject*)w);

		params->args[argCount - 1].type = NPVariantType_Object;
		params->args[argCount - 1].value.objectValue = (NPObject*)w;

		//return the write request
		*result = params->args[argCount - 1];
	}

	uv_async_send(&TCPWrap::async_handle);
	return true;
}


void TCPWrap::invoke_worker_thread(uv_async_t* handle)
{
	InvokeParams* params;

	// if there's a build up of invoke requests handle them all
	// while you're at it
	// does same thing as if most of the time
	while(currentParamRead->next != NULL) {
		params = currentParamRead->next;
		delete currentParamRead;
		currentParamRead = params;
		
		NPVariant* args = params->args;
		uint32_t argCount = params->argCount;
		NPIdentifier name = params->name;
		TCPWrap* s = params->object;

		if(name == s->bind_func) {
			s->bind(args[0],args[1]);
		}
		if(name == s->listen_func) {
			s->listen(args[0]);
		}
		if(name == s->readstart_func) {
			s->readStart();
		}
		if(name == s->readstop_func) {
			s->readStop();
		}
		if(name == s->close_func) {
			s->close();
		}
		if(name == s->write_func) {
			s->write(args[0],args[params->argCount - 1].value.objectValue);
		}
		delete[] params->args;
	}
}

bool TCPWrap::bind(NPVariant address, NPVariant port) 
{
	int c_port = static_cast<int>(port.value.doubleValue);
	char* c_address = (char*)realloc((void*)address.value.stringValue.UTF8Characters,address.value.stringValue.UTF8Length);

	// the address must be null terminated
	c_address[address.value.stringValue.UTF8Length] = '\0';

    struct sockaddr_in s_address = uv_ip4_addr(c_address, c_port);
    int r = uv_tcp_bind(this->stream, s_address);

	return true;
}

DWORD WINAPI run_uv(LPVOID lpParam) 
{
	uv_run();
	return 0;
}

bool TCPWrap::listen(NPVariant backlog)
{
	int c_backlog = static_cast<int> (backlog.value.doubleValue);

    int r = uv_listen((uv_stream_t*)this->stream, c_backlog, OnConnection);

	return true;
}
bool TCPWrap::readStart()
{
	int r = uv_read_start((uv_stream_t*)this->stream, OnAlloc, OnRead);

	return true;
}
bool TCPWrap::readStop()
{
	int r = uv_read_stop((uv_stream_t*)this->stream);

	return true;
}
bool TCPWrap::write(NPVariant data,NPObject* req_obj)
{
	WriteReq* req = (WriteReq*)req_obj;
	req->data = (char*)data.value.stringValue.UTF8Characters;

	int r = uv_write(req->write_req, 
					(uv_stream_t*)this->stream,
					&uv_buf_init((char*)data.value.stringValue.UTF8Characters, data.value.stringValue.UTF8Length), 
					1, 
					OnWrite);

	return true;
}
bool TCPWrap::close() 
{
	uv_close((uv_handle_t*)stream,(uv_close_cb)OnClose);

	return true;
}


void OnConnection(uv_stream_t* server, int status) 
{
	uv_tcp_t* stream = new uv_tcp_t;

	TCPWrap* tcpWrap = (TCPWrap*)server->data;

	TCPWrap* newTcpWrap = (TCPWrap*)NPN_CreateObject(tcpWrap->m_Instance,&TCPWrap::_npclass);
	newTcpWrap->init(stream);
	NPN_RetainObject((NPObject*)newTcpWrap);

	int r = uv_accept(server, (uv_stream_t*)stream);

	NPVariant* args = new NPVariant[1];

	args[0].type = NPVariantType_Object;
	args[0].value.objectValue = (NPObject*)newTcpWrap;

	tcpWrap->fireCallback("onconnection",args,1);
}

uv_buf_t OnAlloc(uv_handle_t* handle, size_t suggested_size) 
{
	// really should be using buffers here but because it's running in a different
	// thread so I don't know whether it's worth it
	return uv_buf_init(new char[suggested_size], suggested_size);
}

void OnRead(uv_stream_t* stream, ssize_t nread, uv_buf_t buf)
{
	if(nread >= 0) {
		NPVariant* args = new NPVariant[3];

		//buffer
		args[0].type = NPVariantType_String;
		args[0].value.stringValue.UTF8Characters = buf.base;
		args[0].value.stringValue.UTF8Length = nread;

		//offset
		args[1].type = NPVariantType_Double;
		args[1].value.doubleValue = 0;

		//length
		args[2].type = NPVariantType_Double;
		args[2].value.doubleValue = nread;

		((TCPWrap*)stream->data)->fireCallback("onread",args,3);
	}
}

void OnWrite(uv_write_t* write_req, int status) 
{
	WriteReq* req = (WriteReq*)write_req->data;

	NPVariant* args = new NPVariant[3];
	args[0].type = NPVariantType_Double;
	args[0].value.doubleValue = status;

	args[1].type = NPVariantType_Object;
	args[1].value.objectValue = (NPObject*)req->socket;

	args[2].type = NPVariantType_Object;
	args[2].value.objectValue = (NPObject*)req;

	req->fireCallback("oncomplete",args,3);
	if(!req->retainBuffer) {
		//delete[] req->data;
	}
}
void OnClose(uv_stream_t* stream)
{
	TCPWrap* tcpWrap = (TCPWrap*)stream->data;
	delete tcpWrap;
}