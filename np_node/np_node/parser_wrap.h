#include "http_parser.h"
#include "ScriptableObject.h"

int OnMessageBegin(http_parser*);
int OnUrl(http_parser*, const char *at, size_t length);
int OnHeaderField(http_parser*, const char *at, size_t length);
int OnHeaderValue(http_parser*, const char *at, size_t length);
int OnHeadersComplete(http_parser*);
int OnBody(http_parser*, const char *at, size_t length);
int OnMessageComplete(http_parser*);

static http_parser_settings http_parser_callbacks = {
		(http_cb)OnMessageBegin,
		(http_data_cb)OnUrl,
		(http_data_cb)OnHeaderField,
		(http_data_cb)OnHeaderValue,
		(http_cb)OnHeadersComplete,
		(http_data_cb)OnBody,
		(http_cb)OnMessageComplete
};


class ParserWrap : public ScriptableObject {
public:
	http_parser* parser;
	ParserWrap(NPP npp);
	~ParserWrap();

	bool init(NPVariant type);
	bool execute(NPVariant buffer,NPVariant start,NPVariant end);
	

	bool HasMethod(NPIdentifier name);
	bool Invoke(NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
    bool HasProperty(NPIdentifier name);
    bool SetProperty(NPIdentifier name, const NPVariant *value);

	// callbacks
	NPObject *onmessagebegin_callback;
	NPObject *onurl_callback;
	NPObject *onheaderfield_callback;
	NPObject *onheadervalue_callback;
	NPObject *onheaderscomplete_callback;
	NPObject *onbody_callback;
	NPObject *onmessagecomplete_callback;

	// holds a javascript function which creates a blank object
	NPObject *create_info;

	NPIdentifier createinfo_prop;
	
	NPIdentifier onmessagebegin_func;
	NPIdentifier onurl_func;
	NPIdentifier onheaderfield_func;
	NPIdentifier onheadervalue_func;
	NPIdentifier onheaderscomplete_func;
	NPIdentifier onbody_func;
	NPIdentifier onmessagecomplete_func;

	NPIdentifier reinitialise_func;
	NPIdentifier execute_func;

	static NPClass _npclass;
	static NPObject *Allocate(NPP npp, NPClass *aClass);
};


