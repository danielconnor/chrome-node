#ifndef _TCP_WRAP
#define _TCP_WRAP

#include "ScriptableObject.h"
#include "uv.h"

DWORD WINAPI run_uv(LPVOID lpParam);

//callbacks
void OnConnection(uv_stream_t* server, int status);
void OnRead(uv_stream_t* stream, ssize_t nread, uv_buf_t buf);
void OnWrite(uv_write_t* req, int status);
void OnClose(uv_stream_t* stream);
uv_buf_t OnAlloc(uv_handle_t* handle, size_t suggested_size); 

DWORD WINAPI run(LPVOID lpParam);


class TCPWrap : public ScriptableObject
{
public: 
	void init(uv_tcp_t* stream);
	void init();

	TCPWrap(NPP instance);
	~TCPWrap();

	uv_tcp_t* stream;

	bool bind(NPVariant address,NPVariant port);
	bool connect(NPVariant address,NPVariant port);
	bool listen(NPVariant backlog);
	bool write(NPVariant data, NPObject* req);
	bool readStart();
	bool readStop();
	bool close();

	bool HasMethod(NPIdentifier name);
	bool Invoke(NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
    bool GetProperty(NPIdentifier name, NPVariant *result);
    bool SetProperty(NPIdentifier name, const NPVariant *value);
	

	// methods & properties
	NPIdentifier bind_func;
	NPIdentifier listen_func;
	NPIdentifier readstart_func;
	NPIdentifier readstop_func;
	NPIdentifier write_func;
	NPIdentifier close_func;
	NPIdentifier connect_func;

	NPIdentifier onconnection_cb;
	NPIdentifier onread_cb;

	NPIdentifier socket_prop;
	


	// callbacks
	NPObject* onconnection_callback;
	NPObject* onread_callback;

	// properties
	NPObject* socket_pointer;

	static uv_async_t async_handle;

    static NPObject* Allocate(NPP npp, NPClass *aClass);
    static NPClass _npclass;

	static DWORD WINAPI run_uv(LPVOID lpParam);
	static void invoke_worker_thread(uv_async_t* handle);
};


struct InvokeParams {
	NPIdentifier name;
	NPVariant* args;
	int argCount;
	TCPWrap* object;
	InvokeParams* next;
};

static InvokeParams* currentParamWrite = NULL;
static InvokeParams* currentParamRead = NULL;

#endif

