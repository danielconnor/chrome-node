#include "buffer_wrap.h"


NPClass BufferWrap::_npclass = {
	CREATE_CLASS(BufferWrap)
};


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
	addMethod("get");
	get_func = NPN_GetStringIdentifier("get");

	addMethod("set");
	set_func = NPN_GetStringIdentifier("set");

	addMethod("asciiSlice");
	asciislice_func = NPN_GetStringIdentifier("asciiSlice");

	addMethod("asciiWrite");
	asciiwrite_func = NPN_GetStringIdentifier("asciiWrite");

	addMethod("utf8Slice");
	utf8slice_func =  NPN_GetStringIdentifier("utf8Slice");

	addMethod("utf8Write");
	utf8write_func =  NPN_GetStringIdentifier("utf8Write");

	addMethod("copy");
	copy_func =  NPN_GetStringIdentifier("copy");

	addMethod("destroy");
	destroy_func =  NPN_GetStringIdentifier("destroy");

	addProperty("length");
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
		result->value.doubleValue = size;
	}
	return true;
}

bool BufferWrap::get(NPVariant src, NPVariant* result) 
{
	size_t c_src = src.value.doubleValue;
	
	result->type = NPVariantType_Double;
	result->value.doubleValue = data[c_src];
	return true;
}
bool BufferWrap::set(NPVariant dest,NPVariant val)
{
	size_t c_dest = dest.value.doubleValue;

	char c_val;

	if (val.type == NPVariantType_String)
		c_val = val.value.stringValue.UTF8Characters[0];
	else 
		c_val = val.value.doubleValue;

	data[c_dest] = c_val;

	return true;
}
bool BufferWrap::utf8Slice(NPVariant start, NPVariant end,NPVariant* string) 
{
	int c_start = start.value.doubleValue;
	int c_end = end.value.doubleValue;
	int length = c_end-c_start;

	if (c_end > size || c_end < 0)
		c_end = size;

	//must use NPN_MemAlloc for alocating strings
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

	size_t ac_length = string.value.stringValue.UTF8Length,
		   c_start = start.value.doubleValue,
		   c_length = length.value.doubleValue,
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
	
	size_t c_target_start = target_start.value.doubleValue,
		   c_start = start.value.doubleValue,
		   c_end = end.value.doubleValue,
		   length = c_end - c_start;

	c_target_start = (size_t)(c_target_start + targetWrap->data);
	c_start = (size_t)(c_start + data);

	memmove((void*)c_target_start,(void*)c_start,length);
	return true;
}

bool BufferWrap::Invoke(NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	if(name == set_func)
	{
		return this->set(args[0],args[1]);
	}
	if(name == get_func) 
	{
		return this->get(args[0],result);
	}
	if(name == asciislice_func) 
	{
		return this->asciiSlice(args[0],args[1],result);
	}
	if(name == asciiwrite_func) 
	{
		return this->asciiWrite(args[0],args[1],args[2],result);
	}
	if(name == utf8slice_func) 
	{
		return this->utf8Slice(args[0],args[1],result);
	}
	if(name == utf8write_func) 
	{
		return this->utf8Write(args[0],args[1],args[2],result);
	}
	if(name == copy_func) 
	{
		return this->copy(args[0],args[1],args[2],args[3]);
	}
	if(name == destroy_func) 
	{
		NPN_ReleaseObject(this);
		return true;
	}
	return false;
}
