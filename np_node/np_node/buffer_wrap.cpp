#include "buffer_wrap.h"


CREATE_CLASS(BufferWrap);

NPObject *BufferWrap::Allocate(NPP npp, NPClass *aClass)
{
	NPObject *pObj = (NPObject *)new BufferWrap(npp);
	return pObj;
}
bool BufferWrap::HasMethod(NPIdentifier name)
{
	return name == set_func ||
		   name == get_func ||
		   name == copy_func ||
		   name == destroy_func ||
		   name == asciislice_func ||
		   name == asciiwrite_func ||
		   name == utf8slice_func ||
		   name == utf8write_func;
}
bool BufferWrap::HasProperty(NPIdentifier name)
{
	return name == size_prop;
}
BufferWrap::BufferWrap (NPP instance): ScriptableObject(instance)
{
	get_func = NPN_GetStringIdentifier("get");
	set_func = NPN_GetStringIdentifier("set");
	asciislice_func = NPN_GetStringIdentifier("asciiSlice");
	asciiwrite_func = NPN_GetStringIdentifier("asciiWrite");
	utf8slice_func =  NPN_GetStringIdentifier("utf8Slice");
	utf8write_func =  NPN_GetStringIdentifier("utf8Write");
	copy_func =  NPN_GetStringIdentifier("copy");
	destroy_func =  NPN_GetStringIdentifier("destroy");
	size_prop =  NPN_GetStringIdentifier("length");
}
void BufferWrap::init(size_t length)
{
	data = new char[length];
	size = length;
}

BufferWrap::~BufferWrap()
{
	delete[] data;
}
bool BufferWrap::GetProperty(NPIdentifier name, NPVariant *result)
{
	if(name == size_prop)
	{
		result->type = NPVariantType_Double;
		result->value.doubleValue = static_cast<double>(size);
	}
	return true;
}

bool BufferWrap::get(NPVariant src, NPVariant* result) 
{
	size_t c_src = static_cast<size_t>(src.value.doubleValue);
	
	result->type = NPVariantType_Double;
	result->value.doubleValue = data[c_src];
	return true;
}
bool BufferWrap::set(NPVariant dest,NPVariant val)
{
	size_t c_dest = static_cast<size_t>(dest.value.doubleValue);

	char c_val;

	if (val.type == NPVariantType_String)
		c_val = static_cast<char>(val.value.stringValue.UTF8Characters[0]);
	else 
		c_val = static_cast<char>(val.value.doubleValue);

	data[c_dest] = c_val;

	return true;
}
bool BufferWrap::utf8Slice(NPVariant start, NPVariant end,NPVariant* string) 
{
	size_t c_start = static_cast<size_t>(start.value.doubleValue),
		   c_end = static_cast<size_t>(end.value.doubleValue);
	size_t length = c_end-c_start;

	if (c_end > size || c_end < 0)
		c_end = size;

	// must use NPN_MemAlloc for alocating strings
	// that are being passed back to the browser
	char* result = (char*)NPN_MemAlloc(length);
	memcpy(result,data + c_start,length);

	string->type = NPVariantType_String;
	string->value.stringValue.UTF8Characters = result;
	string->value.stringValue.UTF8Length = length;

	return true;
}
bool BufferWrap::asciiSlice(NPVariant start, NPVariant end,NPVariant* string) 
{
	//not yet implemented
	return true;
}

bool BufferWrap::asciiWrite(NPVariant string, NPVariant start, NPVariant length,NPVariant* written)
{
	//not yet implemented
	return true;
}
bool BufferWrap::utf8Write(NPVariant string, NPVariant start, NPVariant length,NPVariant* written)
{
	char* source = (char*)string.value.stringValue.UTF8Characters;

	size_t ac_length = static_cast<size_t>(string.value.stringValue.UTF8Length),
		   c_start = static_cast<size_t>(start.value.doubleValue),
		   c_length = static_cast<size_t>(length.value.doubleValue),
		   i;

	for(i = 0; i < size && i < c_length && i < ac_length; i++)
	{
		data[i + c_start] = source[i];
	}
	written->type = NPVariantType_Double;
	written->value.doubleValue = i;
	return true;
}

bool BufferWrap::copy(NPVariant target, NPVariant target_start, NPVariant start, NPVariant end)
{
	BufferWrap* targetWrap = (BufferWrap*)target.value.objectValue;
	
	size_t c_target_start = static_cast<size_t>(target_start.value.doubleValue),
		   c_start = static_cast<size_t>(start.value.doubleValue),
		   c_end = static_cast<size_t>(end.value.doubleValue),
		   length = static_cast<size_t>(c_end - c_start);

	c_target_start = (size_t)(c_target_start + targetWrap->data);
	c_start = (size_t)(c_start + data);

	memmove((void*)c_target_start,(void*)c_start,length);
	return true;
}

bool BufferWrap::Invoke(NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	if(name == set_func)
	{
		return set(args[0],args[1]);
	}
	if(name == get_func) 
	{
		return get(args[0],result);
	}
	if(name == asciislice_func) 
	{
		return asciiSlice(args[0],args[1],result);
	}
	if(name == asciiwrite_func) 
	{
		return asciiWrite(args[0],args[1],args[2],result);
	}
	if(name == utf8slice_func) 
	{
		return utf8Slice(args[0],args[1],result);
	}
	if(name == utf8write_func) 
	{
		return utf8Write(args[0],args[1],args[2],result);
	}
	if(name == copy_func) 
	{
		return copy(args[0],args[1],args[2],args[3]);
	}
	if(name == destroy_func) 
	{
		NPN_ReleaseObject(this);
		return true;
	}
	return false;
}
