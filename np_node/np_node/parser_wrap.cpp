#include "parser_wrap.h"
#include "buffer_wrap.h"

#include "console.h"

CREATE_CLASS(ParserWrap);

ParserWrap::ParserWrap(NPP instance): ScriptableObject(instance)
{
	addMethod("execute");
	addMethod("reinitialize");

	addCallback("onMessageBegin");
	addCallback("onURL");
	addCallback("onHeaderField");
	addCallback("onHeaderValue");
	addCallback("onHeadersComplete");
	addCallback("onBody");
	addCallback("onMessageComplete");

	addProperty("createInfo");

	onmessagebegin_callback = new NPObject;
	onurl_callback = new NPObject;
	onheaderfield_callback = new NPObject;
	onheadervalue_callback = new NPObject;
	onheaderscomplete_callback = new NPObject;
	onbody_callback = new NPObject;
	onmessagecomplete_callback = new NPObject;
	create_info = new NPObject;

	parser = new http_parser;
	parser->data = this;
}
ParserWrap::~ParserWrap()
{
	delete onmessagebegin_callback;
	delete onurl_callback;
	delete onheaderfield_callback;
	delete onheadervalue_callback;
	delete onheaderscomplete_callback;
	delete onbody_callback;
	delete onmessagecomplete_callback;
	delete create_info;
}

bool ParserWrap::Invoke(NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	if(name == methods["execute"]) 
	{
		return execute(args[0],args[1],args[2]);
	}
	if(name == methods["reinitialize"]) 
	{
		return init(args[0]);
	}
	return false;
}

bool ParserWrap::init(NPVariant type) 
{
	http_parser_type t = HTTP_RESPONSE;
	int len = type.value.stringValue.UTF8Length;
	char* type_char = new char[len + 1];
	memcpy(type_char,type.value.stringValue.UTF8Characters,len);

	type_char[len] = '\0';
	if(strcmp(type_char,"request") == 0) 
	{
		t = HTTP_REQUEST;
	}
	http_parser_init(this->parser,t);
	return true;
}
bool ParserWrap::execute(NPVariant buffer,NPVariant start,NPVariant end) 
{
	size_t c_start = start.value.doubleValue;
	size_t c_end = end.value.doubleValue;
	
	NPVariant result;
	NPN_GetProperty(m_Instance,buffer.value.objectValue,NPN_GetStringIdentifier("_handle"),&result);

	BufferWrap* b = (BufferWrap*)result.value.objectValue;

	http_parser_execute(parser,&http_parser_callbacks,b->data + c_start,c_end - c_start);

	return true;
}

bool ParserWrap::SetProperty(NPIdentifier name, const NPVariant *value)
{
	NPObject** callback;

	if(name == properties["onMessageBegin"])
	{
		callback = &onmessagebegin_callback;
	}
	if(name == properties["onURL"])
	{
		callback = &onurl_callback;
	}
	if(name == properties["onHeaderField"])
	{
		callback = &onheaderfield_callback;
	}
	if(name == properties["onHeaderValue"])
	{
		callback = &onheadervalue_callback;
	}
	if(name == properties["onHeadersComplete"])
	{
		callback = &onheaderscomplete_callback;
	}
	if(name == properties["onBody"])
	{
		callback = &onbody_callback;
	}
	if(name == properties["onMessageComplete"])
	{
		callback = &onmessagecomplete_callback;
	}
	if(name == properties["createInfo"]) 
	{
		callback = &create_info;
	}
	if(callback != NULL) 
	{
		NPN_ReleaseObject(*callback);
		*callback = value->value.objectValue;
		NPN_RetainObject(*callback);
	}
	return true;
}

int OnMessageBegin(http_parser* parser)
{
	ParserWrap* wrap = (ParserWrap*)parser->data;
	NPVariant result;
	wrap->apply(wrap->onmessagebegin_callback,NULL,0,&result);
	return 0;
}
int OnUrl(http_parser* parser, const char *at, size_t length)
{
	NPString str = { at, length};
	NPVariant buffer;
	buffer.type = NPVariantType_String;
	buffer.value.stringValue = str;

	NPVariant args[] = {buffer};

	ParserWrap* wrap = (ParserWrap*)parser->data;

	NPVariant result;
	wrap->apply(wrap->onurl_callback,args,1,&result);

	return 0;
}
int OnHeaderField(http_parser* parser, const char *at, size_t length)
{
	NPString str = { at, length};
	NPVariant buffer;
	buffer.type = NPVariantType_String;
	buffer.value.stringValue = str;

	NPVariant args[] = {buffer};

	ParserWrap* wrap = (ParserWrap*)parser->data;

	NPVariant result;
	wrap->apply(wrap->onheaderfield_callback,args,1,&result);
	return 0;
}
int OnHeaderValue(http_parser* parser, const char *at, size_t length)
{
	NPString str = { at, length};
	NPVariant buffer;
	buffer.type = NPVariantType_String;
	buffer.value.stringValue = str;

	NPVariant args[] = {buffer};

	ParserWrap* wrap = (ParserWrap*)parser->data;

	NPVariant result;
	wrap->apply(wrap->onheadervalue_callback,args,1,&result);
	return 0;
}
int OnHeadersComplete(http_parser* parser)
{
	ParserWrap* wrap = (ParserWrap*)parser->data;
	NPP npp = wrap->m_Instance;


	// this is a bit hacky
	// we have to create an empty javascript object
	// so we use a function we created and passed to
	// the plugin
	NPVariant info;
	wrap->apply(wrap->create_info,NULL,0,&info);
	
	NPVariant major;
	major.type = NPVariantType_Double;
	major.value.doubleValue = parser->http_major;
	NPN_SetProperty(npp,info.value.objectValue,NPN_GetStringIdentifier("versionMajor"),&major);

	NPVariant minor;
	minor.type = NPVariantType_Double;
	minor.value.doubleValue = parser->http_major;
	NPN_SetProperty(npp,info.value.objectValue,NPN_GetStringIdentifier("versionMinor"),&minor);

	NPVariant method;
	//method.type = NPVariantType_String;
	//NPString method_str = {(NPUTF8 *)parser->method,strlen((char*)parser->method)};
	//method.value.stringValue = method;
	//NPN_SetProperty(npp,info.value.objectValue,NPN_GetStringIdentifier("method"),&method);

	NPVariant status;
	status.type = NPVariantType_Double;
	status.value.doubleValue = parser->status_code;
	NPN_SetProperty(npp,info.value.objectValue,NPN_GetStringIdentifier("statusCode"),&status);

	NPVariant upgrade;
	upgrade.type = NPVariantType_Bool;
	upgrade.value.boolValue = parser->upgrade;
	NPN_SetProperty(npp,info.value.objectValue,NPN_GetStringIdentifier("upgrade"),&upgrade);

	NPVariant keepAlive;
	keepAlive.type = NPVariantType_Bool;
	keepAlive.value.boolValue = http_should_keep_alive(parser);
	NPN_SetProperty(npp,info.value.objectValue,NPN_GetStringIdentifier("shouldKeepAlive"),&keepAlive);

	NPVariant* args = new NPVariant;
	args->type = NPVariantType_Object;
	args->value.objectValue = info.value.objectValue;
	NPN_RetainObject(info.value.objectValue);

	NPVariant result;
	wrap->apply(wrap->onheaderscomplete_callback,args,1,&result);
	return 0;
}
int OnBody(http_parser* parser, const char *at, size_t length)
{
	NPString str = { at, length};
	NPVariant buffer;
	buffer.type = NPVariantType_String;
	buffer.value.stringValue = str;

	NPVariant args[] = {buffer};

	ParserWrap* wrap = (ParserWrap*)parser->data;
	NPVariant result;
	wrap->apply(wrap->onbody_callback,args,1,&result);
	return 0;
}
int OnMessageComplete(http_parser* parser)
{
	ParserWrap* wrap = (ParserWrap*)parser->data;
	NPVariant result;
	wrap->apply(wrap->onmessagecomplete_callback,NULL,0,&result);
	return 0;
}

NPObject *ParserWrap::Allocate(NPP npp, NPClass *aClass)
{
	NPObject *pObj = (NPObject *)new ParserWrap(npp);
	return pObj;
}
