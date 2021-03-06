//
//Copyright (C) 2002-2005  3Dlabs Inc. Ltd.
//Copyright (C) 2013 LunarG, Inc.
//
//All rights reserved.
//
//Redistribution and use in source and binary forms, with or without
//modification, are permitted provided that the following conditions
//are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of 3Dlabs Inc. Ltd. nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
//COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
//ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//POSSIBILITY OF SUCH DAMAGE.
//

// this only applies to the standalone wrapper, not the front end in general
#define _CRT_SECURE_NO_WARNINGS

#include "Worklist.h"
#include "./../glslang/Include/ShHandle.h"
#include "./../glslang/Public/ShaderLang.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../glslang/OSDependent/osinclude.h"

extern "C" {
    SH_IMPORT_EXPORT void ShOutputHtml();
}

// Command-line options
enum TOptions {
	EOptionNone               = 0x000,
	EOptionIntermediate       = 0x001,
	EOptionSuppressInfolog    = 0x002,
	EOptionMemoryLeakMode     = 0x004,
    EOptionRelaxedErrors      = 0x008,
    EOptionGiveWarnings       = 0x010,
    EOptionsLinkProgram       = 0x020,
    EOptionMultiThreaded      = 0x040,
    EOptionDumpConfig         = 0x080,
};

//
// Return codes from main.
//
enum TFailCode {
    ESuccess = 0,
    EFailUsage,
    EFailCompile,
    EFailLink,
    EFailCompilerCreate,
    EFailThreadCreate,
    EFailLinkerCreate
};

//
// Just placeholders for testing purposes.  The stand-alone environment
// can't actually do a full link without something specifying real
// attribute bindings.
//
ShBinding FixedAttributeBindings[] = { 
    { "gl_Vertex", 15 },
    { "gl_Color", 10 },
    { "gl_Normal", 7 },
};

ShBindingTable FixedAttributeTable = { 3, FixedAttributeBindings };

EShLanguage FindLanguage(const std::string& name);
bool CompileFile(const char *fileName, ShHandle, int options);
void usage();
void FreeFileData(char **data);
char** ReadFileData(const char *fileName);
void InfoLogMsg(const char* msg, const char* name, const int num);

// Use to test breaking up a single shader file into multiple strings.
int NumShaderStrings = 1;

TBuiltInResource Resources;
std::string ConfigFile;

//
// These are the default resources for TBuiltInResources, used for both
//  - parsing this string for the case where the user didn't supply one
//  - dumping out a template for user construction of a config file
//
const char* DefaultConfig = 
"MaxLights 32\n"
"MaxClipPlanes 6\n"
"MaxTextureUnits 32\n"
"MaxTextureCoords 32\n"
"MaxVertexAttribs 64\n"
"MaxVertexUniformComponents 4096\n"
"MaxVaryingFloats 64\n"
"MaxVertexTextureImageUnits 32\n"
"MaxCombinedTextureImageUnits 32\n"
"MaxTextureImageUnits 32\n"
"MaxFragmentUniformComponents 4096\n"
"MaxDrawBuffers 32\n"
"MaxVertexUniformVectors 128\n"
"MaxVaryingVectors 8\n"
"MaxFragmentUniformVectors 16\n"
"MaxVertexOutputVectors 16\n"
"MaxFragmentInputVectors 15\n"
"MinProgramTexelOffset -8\n"
"MaxProgramTexelOffset 7\n"
;

//
// Parse either a .conf file provided by the user or the default string above.
//
void ProcessConfigFile()
{
    char** configStrings = 0;
    char *config = 0;
    if (ConfigFile.size() > 0) {
        char** configStrings = ReadFileData(ConfigFile.c_str());
        if (configStrings)
            config = *configStrings;
        else {
            printf("Error opening configuration file; will instead use the default configuration\n");
            usage();
        }
    }

    if (config == 0) {
        config = new char[strlen(DefaultConfig)];
        strcpy(config, DefaultConfig);
    }

    const char* delims = " \t\n\r";
    const char* token = strtok(config, delims);
    while (token) {
        const char* valueStr = strtok(0, delims);
        if (valueStr == 0 || ! (valueStr[0] == '-' || (valueStr[0] >= '0' && valueStr[0] <= '9'))) {
            printf("Error: '%s' bad .conf file.  Each name must be followed by one number.\n", valueStr ? valueStr : "");
            return;
        }
        int value = atoi(valueStr);

        if (strcmp(token, "MaxLights") == 0)
            Resources.maxLights = value;
        else if (strcmp(token, "MaxClipPlanes") == 0)
            Resources.maxClipPlanes = value;
        else if (strcmp(token, "MaxTextureUnits") == 0)
            Resources.maxTextureUnits = value;
        else if (strcmp(token, "MaxTextureCoords") == 0)
            Resources.maxTextureCoords = value;
        else if (strcmp(token, "MaxVertexAttribs") == 0)
            Resources.maxVertexAttribs = value;
        else if (strcmp(token, "MaxVertexUniformComponents") == 0)
            Resources.maxVertexUniformComponents = value;
        else if (strcmp(token, "MaxVaryingFloats") == 0)
            Resources.maxVaryingFloats = value;
        else if (strcmp(token, "MaxVertexTextureImageUnits") == 0)
            Resources.maxVertexTextureImageUnits = value;
        else if (strcmp(token, "MaxCombinedTextureImageUnits") == 0)
            Resources.maxCombinedTextureImageUnits = value;
        else if (strcmp(token, "MaxTextureImageUnits") == 0)
            Resources.maxTextureImageUnits = value;
        else if (strcmp(token, "MaxFragmentUniformComponents") == 0)
            Resources.maxFragmentUniformComponents = value;
        else if (strcmp(token, "MaxDrawBuffers") == 0)
            Resources.maxDrawBuffers = value;
        else if (strcmp(token, "MaxVertexUniformVectors") == 0)
            Resources.maxVertexUniformVectors = value;
        else if (strcmp(token, "MaxVaryingVectors") == 0)
            Resources.maxVaryingVectors = value;
        else if (strcmp(token, "MaxFragmentUniformVectors") == 0)
            Resources.maxFragmentUniformVectors = value;
        else if (strcmp(token, "MaxVertexOutputVectors") == 0)
            Resources.maxVertexOutputVectors = value;
        else if (strcmp(token, "MaxFragmentInputVectors") == 0)
            Resources.maxFragmentInputVectors = value;
        else if (strcmp(token, "MinProgramTexelOffset") == 0)
            Resources.minProgramTexelOffset = value;
        else if (strcmp(token, "MaxProgramTexelOffset") == 0)
            Resources.maxProgramTexelOffset = value;
        else
            printf("Warning: unrecognized limit (%s) in configuration file.\n", token);

        token = strtok(0, delims);
    }
    if (configStrings)
        FreeFileData(configStrings);
}

// thread-safe list of shaders to asynchronously grab and compile
glslang::TWorklist Worklist;

// array of unique places to leave the shader names and infologs for the asynchronous compiles
glslang::TWorkItem **Work = 0;
int NumWorkItems = 0;

int Options = 0;
bool Delay = false;
const char* ExecutableName;

//
// *.conf => this is a config file that can set limits/resources
//
bool SetConfigFile(const std::string& name)
{
    if (name.size() < 5)
        return false;

    if (name.substr(name.size() - 5, std::string::npos) == ".conf") {
        ConfigFile = name;
        return true;
    }

    return false;
}

bool ProcessArguments(int argc, char* argv[])
{
    ExecutableName = argv[0];
    NumWorkItems = argc;  // will include some empties where the '-' options were, but it doesn't matter, they'll be 0
    Work = new glslang::TWorkItem*[NumWorkItems];    
    Work[0] = 0;

    argc--;
    argv++;    
    for (; argc >= 1; argc--, argv++) {
        Work[argc] = 0;
        if (argv[0][0] == '-') {
            switch (argv[0][1]) {
            case 'c':
                Options |= EOptionDumpConfig;
                break;
            case 'd':
                Delay = true;                        
                break;
            case 'i':
                Options |= EOptionIntermediate;
                break;
            case 'l':
                Options |= EOptionsLinkProgram;
                break;
            case 'm':
                Options |= EOptionMemoryLeakMode;
                break;
            case 'r':
                Options |= EOptionRelaxedErrors;
                break;
            case 's':
                Options |= EOptionSuppressInfolog;
                break;
            case 't':
                #ifdef _WIN32
                Options |= EOptionMultiThreaded;
                #endif
                break;
            default:
                return false;
            }
        } else {
            std::string name(argv[0]);
            if (! SetConfigFile(name)) {
                Work[argc] = new glslang::TWorkItem(name);
                Worklist.add(Work[argc]);
            }
        }
    }

    return true;
}

// Thread entry point, for non-linking asynchronous mode.
unsigned int
#ifdef _WIN32
    __stdcall
#endif
CompileShaders(void*)
{
    glslang::TWorkItem* workItem;
    while (Worklist.remove(workItem)) {
        ShHandle compiler = ShConstructCompiler(FindLanguage(workItem->name), Options);
        if (compiler == 0)
            return false;

        CompileFile(workItem->name.c_str(), compiler, Options);

        if (! (Options & EOptionSuppressInfolog))
            workItem->results = ShGetInfoLog(compiler);

        ShDestruct(compiler);
    }

    return 0;
}

//
// For linking mode: Will independently parse each item in the worklist, but then put them
// in the same program and link them together.
//
// Uses the new C++ interface instead of the old handle-based interface.
//
void CompileAndLinkShaders()
{
    // keep track of what to free
    std::list<glslang::TShader*> shaders;

    EShMessages messages = EShMsgDefault;
    if (Options & EOptionRelaxedErrors)
        messages = (EShMessages)(messages | EShMsgRelaxedErrors);
    if (Options & EOptionIntermediate)
        messages = (EShMessages)(messages | EShMsgAST);

    //
    // Per-shader processing...
    //

    glslang::TProgram program;
    glslang::TWorkItem* workItem;
    while (Worklist.remove(workItem)) {
        EShLanguage stage = FindLanguage(workItem->name);
        glslang::TShader* shader = new glslang::TShader(stage);
        shaders.push_back(shader);
    
        char** shaderStrings = ReadFileData(workItem->name.c_str());
        if (! shaderStrings) {
            usage();
            return;
        }

        shader->setStrings(shaderStrings, 1);

        shader->parse(&Resources, 100, false, messages);
        
        program.addShader(shader);

        if (! (Options & EOptionSuppressInfolog)) {
            puts(workItem->name.c_str());
            puts(shader->getInfoLog());
            puts(shader->getInfoDebugLog());
        }

        FreeFileData(shaderStrings);
    }

    //
    // Program-level processing...
    //

    program.link(messages);
    if (! (Options & EOptionSuppressInfolog)) {
        puts(program.getInfoLog());
        puts(program.getInfoDebugLog());
    }

    // free everything up
    while (shaders.size() > 0) {
        delete shaders.back();
        shaders.pop_back();
    }

    // TODO: memory: for each compile, need a GetThreadPoolAllocator().pop();
}

int C_DECL main(int argc, char* argv[])
{
    bool compileFailed = false;
    bool linkFailed = false;
    
    // Init for front-end proper
    ShInitialize();

    // Init for standalone
    glslang::InitGlobalLock();

    if (! ProcessArguments(argc, argv)) {
        usage();
        return EFailUsage;
    }

    if (Options & EOptionDumpConfig) {
        printf("%s", DefaultConfig);
        if (Worklist.empty())
            return ESuccess;
    }

    if (Worklist.empty()) {
        usage();
        return EFailUsage;
    }

    ProcessConfigFile();


    //
    // Two modes:
    // 1) linking all arguments together, single-threaded, new C++ interface
    // 2) independent arguments, can be tackled by multiple asynchronous threads, for testing thread safety, using the old handle interface
    //
    if (Options & EOptionsLinkProgram)
        CompileAndLinkShaders();
    else {
        bool printShaderNames = Worklist.size() > 1;

        if (Options & EOptionMultiThreaded) {
            const int NumThreads = 16;
            void* threads[NumThreads];
            for (int t = 0; t < NumThreads; ++t) {
                threads[t] = glslang::OS_CreateThread(&CompileShaders);
                if (! threads[t]) {
                    printf("Failed to create thread\n");
                    return EFailThreadCreate;
                }
            }
            glslang::OS_WaitForAllThreads(threads, NumThreads);
        } else {
            if (! CompileShaders(0))
                compileFailed = true;
        }

        // Print out all the resulting infologs
        for (int w = 0; w < NumWorkItems; ++w) {
            if (Work[w]) {
                if (printShaderNames)
                    puts(Work[w]->name.c_str());
                puts(Work[w]->results.c_str());
                delete Work[w];
            }
        }
    }

    if (Delay)
        glslang::OS_Sleep(1000000);

    if (compileFailed)
        return EFailCompile;
    if (linkFailed)
        return EFailLink;

    return 0;
}

//
//   Deduce the language from the filename.  Files must end in one of the
//   following extensions:
//
//   .vert = vertex
//   .tesc = tessellation control
//   .tese = tessellation evaluation
//   .geom = geometry
//   .frag = fragment
//   .comp = compute
//
EShLanguage FindLanguage(const std::string& name)
{
    size_t ext = name.rfind('.');
    if (ext == std::string::npos) {
        usage();
        return EShLangVertex;
    }

    std::string suffix = name.substr(ext + 1, std::string::npos);
    if (suffix == "vert")
        return EShLangVertex;
    else if (suffix == "tesc")
        return EShLangTessControl;
    else if (suffix == "tese")
        return EShLangTessEvaluation;
    else if (suffix == "geom")
        return EShLangGeometry;
    else if (suffix == "frag")
        return EShLangFragment;
    else if (suffix == "comp")
        return EShLangCompute;

    usage();
    return EShLangVertex;
}

//
// Read a file's data into a string, and compile it using the old interface ShCompile, 
// for non-linkable results.
//
bool CompileFile(const char *fileName, ShHandle compiler, int Options)
{
    int ret;
    char** shaderStrings = ReadFileData(fileName);
    if (! shaderStrings) {
        usage();
        return false;
    }

    int* lengths = new int[NumShaderStrings];

    // move to length-based strings, rather than null-terminated strings
    for (int s = 0; s < NumShaderStrings; ++s)
        lengths[s] = static_cast<int>(strlen(shaderStrings[s]));

    if (! shaderStrings)
        return false;

    EShMessages messages = EShMsgDefault;
    if (Options & EOptionRelaxedErrors)
        messages = (EShMessages)(messages | EShMsgRelaxedErrors);
    if (Options & EOptionIntermediate)
        messages = (EShMessages)(messages | EShMsgAST);
    
    for (int i = 0; i < ((Options & EOptionMemoryLeakMode) ? 100 : 1); ++i) {
        for (int j = 0; j < ((Options & EOptionMemoryLeakMode) ? 100 : 1); ++j) {
            //ret = ShCompile(compiler, shaderStrings, NumShaderStrings, lengths, EShOptNone, &Resources, Options, 100, false, messages);
            ret = ShCompile(compiler, shaderStrings, NumShaderStrings, 0, EShOptNone, &Resources, Options, 100, false, messages);
            //const char* multi[4] = { "# ve", "rsion", " 300 e", "s" };
            //const char* multi[7] = { "/", "/", "\\", "\n", "\n", "#", "version 300 es" };
            //ret = ShCompile(compiler, multi, 4, 0, EShOptNone, &Resources, Options, 100, false, messages);
        }

        if (Options & EOptionMemoryLeakMode)
            glslang::OS_DumpMemoryCounters();
    }

    delete [] lengths;
    FreeFileData(shaderStrings);

    return ret ? true : false;
}

//
//   print usage to stdout
//
void usage()
{
    printf("Usage: glslangValidator [ options ] filename\n"
           "Where: filename is a name ending in\n"
           "    .conf provides an optional config file that replaces the default configuration\n"
           "          (see -c option below for generating a template)\n"
           "    .vert for a vertex shader\n"
           "    .tesc for a tessellation control shader\n"
           "    .tese for a tessellation evaluation shader\n"
           "    .geom for a geometry shader\n"
           "    .frag for a fragment shader\n"
           "    .comp for a compute shader\n\n"
           "Compilation warnings and errors will be printed to stdout.\n"
           "To get other information, use one of the following options:\n"
           "-c: configuration dump; use to create default configuration file (redirect to a .conf file)\n"
           "-i: intermediate tree (glslang AST) is printed out\n"
           "-d: delay exit\n"
           "-l: link validation of all input files\n"
           "-m: memory leak mode\n"
           "-r: relaxed semantic error-checking mode\n"
           "-s: silent mode\n"
           "-t: multi-threaded mode\n");
}

#ifndef _WIN32

#include <errno.h>

int fopen_s(
   FILE** pFile,
   const char *filename,
   const char *mode
)
{
   if (!pFile || !filename || !mode) {
      return EINVAL;
   }

   FILE* f = fopen(filename, mode);
   if (! f) {
      if (errno != 0) {
         return errno;
      } else {
         return ENOENT;
      }
   }
   *pFile = f;

   return 0;
}

#endif

//
//   Malloc a string of sufficient size and read a string into it.
//
char** ReadFileData(const char *fileName) 
{
    FILE *in;
	int errorCode = fopen_s(&in, fileName, "r");
    char *fdata;
    int count = 0;
    const int maxSourceStrings = 5;
    char** return_data = (char**)malloc(maxSourceStrings+1);

    //return_data[MAX_SOURCE_STRINGS]=NULL;
	if (errorCode) {
        printf("Error: unable to open input file: %s\n", fileName);
        return 0;
    }
    
    while (fgetc(in) != EOF)
        count++;

	fseek(in, 0, SEEK_SET);
	
	
	if (!(fdata = (char *)malloc(count+2))) {
            printf("Error allocating memory\n");
            return 0;
    }
	if (fread(fdata,1,count, in)!=count) {
            printf("Error reading input file: %s\n", fileName);
            return 0;
    }
    fdata[count] = '\0';
    fclose(in);
    if(count==0){
        return_data[0]=(char*)malloc(count+2);
        return_data[0][0]='\0';
        NumShaderStrings=0;
        return return_data;       
    }

	int len = (int)(ceil)((float)count/(float)NumShaderStrings);
    int ptr_len=0,i=0;
	while(count>0){
		return_data[i]=(char*)malloc(len+2);
		memcpy(return_data[i],fdata+ptr_len,len);
		return_data[i][len]='\0';
		count-=(len);
		ptr_len+=(len);
		if(count<len){
            if(count==0){
               NumShaderStrings=(i+1);
               break;
            }
           len = count;
		}  
		++i;
	}
    return return_data;
}

void FreeFileData(char **data)
{
    for(int i=0;i<NumShaderStrings;i++)
        free(data[i]);
}

void InfoLogMsg(const char* msg, const char* name, const int num)
{
    printf(num >= 0 ? "#### %s %s %d INFO LOG ####\n" :
           "#### %s %s INFO LOG ####\n", msg, name, num);
}
