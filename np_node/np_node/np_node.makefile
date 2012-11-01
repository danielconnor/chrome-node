# Include paths...
Debug_Include_Path=-I"../deps/uv/include" -I"../deps/http_parser" 
Release_Include_Path=-I"../deps/uv/include" -I"../deps/http_parser" 

# Library paths...
Debug_Library_Path=-L"../deps/http_parser/gccDebug" -L"../deps/uv/Debug/gcclib" 
Release_Library_Path=-L"../deps/http_parser/gccRelease" -L"../deps/uv/Release/gcclib" 

# Additional libraries...
Debug_Libraries=-lhttp_parser -luv -lws2_32 -lwldap32 -lPSAPI -lIphlpapi 
Release_Libraries=-lhttp_parser -luv -lws2_32 -lwldap32 -lPSAPI -lIphlpapi 

# Preprocessor definitions...
Debug_Preprocessor_Definitions=-D GCC_BUILD -D _DEBUG -D _WINDOWS -D _USRDLL -D NPRUNTIME_EXPORTS -D XP_WIN32 -D MOZILLA_STRICT_API -D XPCOM_GLUE -D XP_WIN -D _X86_ -D NPSIMPLE_EXPORTS -D _CRT_SECURE_NO_WARNINGS 
Release_Preprocessor_Definitions=-D GCC_BUILD -D _WINDOWS -D _USRDLL -D NPRUNTIME_EXPORTS -D XP_WIN32 -D MOZILLA_STRICT_API -D XPCOM_GLUE -D XP_WIN -D _X86_ -D NPSIMPLE_EXPORTS -D _CRT_SECURE_NO_WARNINGS 

# Implictly linked object files...
Debug_Implicitly_Linked_Objects=
Release_Implicitly_Linked_Objects=

# Compiler flags...
Debug_Compiler_Flags=-fPIC -O0 -g 
Release_Compiler_Flags=-fPIC -O3 

# Builds all configurations for this project...
.PHONY: build_all_configurations
build_all_configurations: Debug Release 

# Builds the Debug configuration...
.PHONY: Debug
Debug: create_folders gccDebug/buffer_wrap.o gccDebug/np_entry.o gccDebug/npn_gate.o gccDebug/npp_gate.o gccDebug/parser_wrap.o gccDebug/plugin.o gccDebug/ScriptableObject.o gccDebug/shutdown_req.o gccDebug/tcp_wrap.o gccDebug/write_req.o 
	g++ -fPIC -shared -Wl,-soname,libnp_node.so -o ../../gccdll/libnp_node.so gccDebug/buffer_wrap.o gccDebug/np_entry.o gccDebug/npn_gate.o gccDebug/npp_gate.o gccDebug/parser_wrap.o gccDebug/plugin.o gccDebug/ScriptableObject.o gccDebug/shutdown_req.o gccDebug/tcp_wrap.o gccDebug/write_req.o  $(Debug_Implicitly_Linked_Objects)

# Compiles file buffer_wrap.cpp for the Debug configuration...
-include gccDebug/buffer_wrap.d
gccDebug/buffer_wrap.o: buffer_wrap.cpp
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c buffer_wrap.cpp $(Debug_Include_Path) -o gccDebug/buffer_wrap.o
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM buffer_wrap.cpp $(Debug_Include_Path) > gccDebug/buffer_wrap.d

# Compiles file np_entry.cpp for the Debug configuration...
-include gccDebug/np_entry.d
gccDebug/np_entry.o: np_entry.cpp
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c np_entry.cpp $(Debug_Include_Path) -o gccDebug/np_entry.o
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM np_entry.cpp $(Debug_Include_Path) > gccDebug/np_entry.d

# Compiles file npn_gate.cpp for the Debug configuration...
-include gccDebug/npn_gate.d
gccDebug/npn_gate.o: npn_gate.cpp
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c npn_gate.cpp $(Debug_Include_Path) -o gccDebug/npn_gate.o
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM npn_gate.cpp $(Debug_Include_Path) > gccDebug/npn_gate.d

# Compiles file npp_gate.cpp for the Debug configuration...
-include gccDebug/npp_gate.d
gccDebug/npp_gate.o: npp_gate.cpp
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c npp_gate.cpp $(Debug_Include_Path) -o gccDebug/npp_gate.o
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM npp_gate.cpp $(Debug_Include_Path) > gccDebug/npp_gate.d

# Compiles file parser_wrap.cpp for the Debug configuration...
-include gccDebug/parser_wrap.d
gccDebug/parser_wrap.o: parser_wrap.cpp
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c parser_wrap.cpp $(Debug_Include_Path) -o gccDebug/parser_wrap.o
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM parser_wrap.cpp $(Debug_Include_Path) > gccDebug/parser_wrap.d

# Compiles file plugin.cpp for the Debug configuration...
-include gccDebug/plugin.d
gccDebug/plugin.o: plugin.cpp
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c plugin.cpp $(Debug_Include_Path) -o gccDebug/plugin.o
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM plugin.cpp $(Debug_Include_Path) > gccDebug/plugin.d

# Compiles file ScriptableObject.cpp for the Debug configuration...
-include gccDebug/ScriptableObject.d
gccDebug/ScriptableObject.o: ScriptableObject.cpp
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c ScriptableObject.cpp $(Debug_Include_Path) -o gccDebug/ScriptableObject.o
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM ScriptableObject.cpp $(Debug_Include_Path) > gccDebug/ScriptableObject.d

# Compiles file shutdown_req.cpp for the Debug configuration...
-include gccDebug/shutdown_req.d
gccDebug/shutdown_req.o: shutdown_req.cpp
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c shutdown_req.cpp $(Debug_Include_Path) -o gccDebug/shutdown_req.o
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM shutdown_req.cpp $(Debug_Include_Path) > gccDebug/shutdown_req.d

# Compiles file tcp_wrap.cpp for the Debug configuration...
-include gccDebug/tcp_wrap.d
gccDebug/tcp_wrap.o: tcp_wrap.cpp
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c tcp_wrap.cpp $(Debug_Include_Path) -o gccDebug/tcp_wrap.o
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM tcp_wrap.cpp $(Debug_Include_Path) > gccDebug/tcp_wrap.d

# Compiles file write_req.cpp for the Debug configuration...
-include gccDebug/write_req.d
gccDebug/write_req.o: write_req.cpp
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c write_req.cpp $(Debug_Include_Path) -o gccDebug/write_req.o
	g++ $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM write_req.cpp $(Debug_Include_Path) > gccDebug/write_req.d

# Builds the Release configuration...
.PHONY: Release
Release: create_folders gccRelease/buffer_wrap.o gccRelease/np_entry.o gccRelease/npn_gate.o gccRelease/npp_gate.o gccRelease/parser_wrap.o gccRelease/plugin.o gccRelease/ScriptableObject.o gccRelease/shutdown_req.o gccRelease/tcp_wrap.o gccRelease/write_req.o 
	g++ -fPIC -shared -Wl,-soname,libnp_node.so -o ../../gccdll/libnp_node.so gccRelease/buffer_wrap.o gccRelease/np_entry.o gccRelease/npn_gate.o gccRelease/npp_gate.o gccRelease/parser_wrap.o gccRelease/plugin.o gccRelease/ScriptableObject.o gccRelease/shutdown_req.o gccRelease/tcp_wrap.o gccRelease/write_req.o  $(Release_Implicitly_Linked_Objects)

# Compiles file buffer_wrap.cpp for the Release configuration...
-include gccRelease/buffer_wrap.d
gccRelease/buffer_wrap.o: buffer_wrap.cpp
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c buffer_wrap.cpp $(Release_Include_Path) -o gccRelease/buffer_wrap.o
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM buffer_wrap.cpp $(Release_Include_Path) > gccRelease/buffer_wrap.d

# Compiles file np_entry.cpp for the Release configuration...
-include gccRelease/np_entry.d
gccRelease/np_entry.o: np_entry.cpp
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c np_entry.cpp $(Release_Include_Path) -o gccRelease/np_entry.o
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM np_entry.cpp $(Release_Include_Path) > gccRelease/np_entry.d

# Compiles file npn_gate.cpp for the Release configuration...
-include gccRelease/npn_gate.d
gccRelease/npn_gate.o: npn_gate.cpp
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c npn_gate.cpp $(Release_Include_Path) -o gccRelease/npn_gate.o
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM npn_gate.cpp $(Release_Include_Path) > gccRelease/npn_gate.d

# Compiles file npp_gate.cpp for the Release configuration...
-include gccRelease/npp_gate.d
gccRelease/npp_gate.o: npp_gate.cpp
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c npp_gate.cpp $(Release_Include_Path) -o gccRelease/npp_gate.o
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM npp_gate.cpp $(Release_Include_Path) > gccRelease/npp_gate.d

# Compiles file parser_wrap.cpp for the Release configuration...
-include gccRelease/parser_wrap.d
gccRelease/parser_wrap.o: parser_wrap.cpp
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c parser_wrap.cpp $(Release_Include_Path) -o gccRelease/parser_wrap.o
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM parser_wrap.cpp $(Release_Include_Path) > gccRelease/parser_wrap.d

# Compiles file plugin.cpp for the Release configuration...
-include gccRelease/plugin.d
gccRelease/plugin.o: plugin.cpp
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c plugin.cpp $(Release_Include_Path) -o gccRelease/plugin.o
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM plugin.cpp $(Release_Include_Path) > gccRelease/plugin.d

# Compiles file ScriptableObject.cpp for the Release configuration...
-include gccRelease/ScriptableObject.d
gccRelease/ScriptableObject.o: ScriptableObject.cpp
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c ScriptableObject.cpp $(Release_Include_Path) -o gccRelease/ScriptableObject.o
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM ScriptableObject.cpp $(Release_Include_Path) > gccRelease/ScriptableObject.d

# Compiles file shutdown_req.cpp for the Release configuration...
-include gccRelease/shutdown_req.d
gccRelease/shutdown_req.o: shutdown_req.cpp
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c shutdown_req.cpp $(Release_Include_Path) -o gccRelease/shutdown_req.o
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM shutdown_req.cpp $(Release_Include_Path) > gccRelease/shutdown_req.d

# Compiles file tcp_wrap.cpp for the Release configuration...
-include gccRelease/tcp_wrap.d
gccRelease/tcp_wrap.o: tcp_wrap.cpp
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c tcp_wrap.cpp $(Release_Include_Path) -o gccRelease/tcp_wrap.o
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM tcp_wrap.cpp $(Release_Include_Path) > gccRelease/tcp_wrap.d

# Compiles file write_req.cpp for the Release configuration...
-include gccRelease/write_req.d
gccRelease/write_req.o: write_req.cpp
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c write_req.cpp $(Release_Include_Path) -o gccRelease/write_req.o
	g++ $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM write_req.cpp $(Release_Include_Path) > gccRelease/write_req.d

# Creates the intermediate and output folders for each configuration...
.PHONY: create_folders
create_folders:
	mkdir -p gccDebug
	mkdir -p ../../gccdll
	mkdir -p gccRelease
	mkdir -p ../../gccdll

# Cleans intermediate and output files (objects, libraries, executables)...
.PHONY: clean
clean:
	rm -f gccDebug/*.o
	rm -f gccDebug/*.d
	rm -f ../../gccdll/*.a
	rm -f ../../gccdll/*.so
	rm -f ../../gccdll/*.dll
	rm -f ../../gccdll/*.exe
	rm -f gccRelease/*.o
	rm -f gccRelease/*.d
	rm -f ../../gccdll/*.a
	rm -f ../../gccdll/*.so
	rm -f ../../gccdll/*.dll
	rm -f ../../gccdll/*.exe

