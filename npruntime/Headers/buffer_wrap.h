#pragma once

#ifndef _BUFFER_WRAP
#define _BUFFER_WRAP

#include "ScriptableObject.h"

class BufferWrap : public ScriptableObject
{
public:
    static NPClass _npclass;
 
	BufferWrap(NPP instance);
	~BufferWrap();

	void init(size_t size);
	char* data;

	size_t size;

    bool GetProperty(NPIdentifier name, NPVariant *result);
	
	bool utf8Slice(NPVariant start, NPVariant end,NPVariant* string); 
	bool asciiSlice(NPVariant start, NPVariant end,NPVariant* string);
	
	bool utf8Write(NPVariant string, NPVariant start,NPVariant length,NPVariant* written);
	bool asciiWrite(NPVariant string, NPVariant start,NPVariant length,NPVariant* written);

	bool copy(NPVariant target,NPVariant target_start,NPVariant start, NPVariant end);
	bool get(NPVariant src,NPVariant* result);
	bool set(NPVariant dest,NPVariant val);

	// methods & properties
	NPIdentifier utf8slice_func;
	NPIdentifier asciislice_func;
	NPIdentifier utf8write_func;
	NPIdentifier asciiwrite_func;
	NPIdentifier copy_func;
	NPIdentifier get_func;
	NPIdentifier set_func;
	NPIdentifier destroy_func;

	NPIdentifier size_prop;


	static NPObject* Allocate(NPP npp, NPClass *aClass);

	bool HasMethod(NPIdentifier name);
	bool HasProperty(NPIdentifier name);
    bool Invoke(NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);

};





#endif