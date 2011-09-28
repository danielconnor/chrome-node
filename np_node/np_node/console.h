#ifndef _CONSOLE
#define _CONSOLE

static NPObject* sWindowObj;

class console {
public:
	static void log(NPP npp,char* string)
	{
		NPVariant s;
		s.type = NPVariantType_String;
		s.value.stringValue.UTF8Characters = string;
		s.value.stringValue.UTF8Length = strlen(string);

		NPVariant args[] = {
			s
		};
		NPVariant console;
		NPN_GetProperty(npp,sWindowObj,NPN_GetStringIdentifier("console"),&console);

		NPVariant result;
		NPN_Invoke(npp,console.value.objectValue,NPN_GetStringIdentifier("log"),args,1,&result);
	}
	static void time(NPP npp,char* string)
	{
		NPVariant s;
		s.type = NPVariantType_String;
		s.value.stringValue.UTF8Characters = string;
		s.value.stringValue.UTF8Length = strlen(string);

		NPVariant args[] = {
			s
		};
		NPVariant console;
		NPN_GetProperty(npp,sWindowObj,NPN_GetStringIdentifier("console"),&console);

		NPVariant result;
		NPN_Invoke(npp,console.value.objectValue,NPN_GetStringIdentifier("time"),args,1,&result);
	}
	static void timeEnd(NPP npp,char* string)
	{
		NPVariant s;
		s.type = NPVariantType_String;
		s.value.stringValue.UTF8Characters = string;
		s.value.stringValue.UTF8Length = strlen(string);

		NPVariant args[] = {
			s
		};
		NPVariant console;
		NPN_GetProperty(npp,sWindowObj,NPN_GetStringIdentifier("console"),&console);

		NPVariant result;
		NPN_Invoke(npp,console.value.objectValue,NPN_GetStringIdentifier("timeEnd"),args,1,&result);
	}
};

#endif