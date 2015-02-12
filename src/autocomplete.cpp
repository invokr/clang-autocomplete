/**
* @file autocomplete.cpp
* @author Robin Dietrich <me (at) invokr (dot) org>
* @version 1.0
*
* @par License
*   clang-autocomplete
*   Copyright 2015 Robin Dietrich
*
*   Licensed under the Apache License, Version 2.0 (the "License");
*   you may not use this file except in compliance with the License.
*   You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
*   Unless required by applicable law or agreed to in writing, software
*   distributed under the License is distributed on an "AS IS" BASIS,
*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*   See the License for the specific language governing permissions and
*   limitations under the License. *
*/

#include <algorithm>
#include <sys/time.h>
#include <iostream>

#include "autocomplete.hpp"

namespace clang_autocomplete {
    Persistent<FunctionTemplate> autocomplete::constructor;

    autocomplete::autocomplete() : ObjectWrap(), mArgs(), mIndex(nullptr) {
        // create the clang index: excludeDeclarationsFromPCH = 1, displayDiagnostics = 1
        mIndex = clang_createIndex(1, 1);

        // If an object is purged from the cache, dispose it's translation unit
        mCache.set_purge_callback([](std::string K, CXTranslationUnit V) noexcept {
            clang_disposeTranslationUnit(V);
        });
    }

    autocomplete::~autocomplete() {
        clang_disposeIndex(mIndex);
    }

    Handle<Value> autocomplete::New(const Arguments &args) {
        HandleScope scope;
        autocomplete *c = new autocomplete();
        c->Wrap(args.This());
        return args.This();
    }

    void autocomplete::Init(Handle<Object> target) {
        HandleScope scope;

        // Wrap new and make it persistend
        Local<FunctionTemplate> local_function_template = FunctionTemplate::New(New);
        autocomplete::constructor = Persistent<FunctionTemplate>::New(local_function_template);

        autocomplete::constructor->InstanceTemplate()->SetInternalFieldCount(1);
        autocomplete::constructor->SetClassName(String::NewSymbol("lib"));

        // Accessor for args and cache_expiration
        autocomplete::constructor->InstanceTemplate()->SetAccessor(String::New("arguments"), GetArgs, SetArgs);
        autocomplete::constructor->InstanceTemplate()->SetAccessor(String::New("cache_expiration"), GetCacheExpiration, SetCacheExpiration);

        // Make our methods available to Node
        NODE_SET_PROTOTYPE_METHOD(autocomplete::constructor, "Version", Version);
        NODE_SET_PROTOTYPE_METHOD(autocomplete::constructor, "Complete", Complete);
        NODE_SET_PROTOTYPE_METHOD(autocomplete::constructor, "Diagnose", Diagnose);

        target->Set(String::NewSymbol("lib"), autocomplete::constructor->GetFunction());
    }

    Handle<Value> autocomplete::GetArgs(Local<String> property, const AccessorInfo& info) {
        HandleScope scope;
        autocomplete* instance = node::ObjectWrap::Unwrap<autocomplete>(info.Holder());

        // Get a list of all arguments for node
        Handle<Array> res = Array::New(instance->mArgs.size());
        for (std::size_t i = 0; i < instance->mArgs.size(); ++i)
            res->Set(i, String::New(instance->mArgs[i].c_str()));

        return scope.Close(res);
    }

    void autocomplete::SetArgs(Local<String> property, Local<Value> value, const AccessorInfo& info) {
        autocomplete* instance = node::ObjectWrap::Unwrap<autocomplete>(info.Holder());
        instance->mArgs.clear();

        if (value->IsArray()) {
            // If we get multiple arguments, clear the list and append them all
            instance->mArgs.clear();

            Handle<Array> arr = Handle<Array>::Cast(value);
            instance->mArgs.reserve(arr->Length());

            for (std::size_t i = 0; i < arr->Length(); ++i) {
                String::Utf8Value str(arr->Get(i));
                instance->mArgs.push_back( *str );
            }
        } else if (value->IsString()) {
            // If we get a single string, append it
            String::Utf8Value str(value);
            instance->mArgs.push_back(*str);
        } else {
            ThrowException(
                Exception::TypeError(String::New("First argument must be a String or an Array"))
            );
        }
    }

    Handle<Value> autocomplete::GetCacheExpiration(Local<String> property, const AccessorInfo& info) {
        HandleScope scope;
        autocomplete* instance = node::ObjectWrap::Unwrap<autocomplete>(info.Holder());

        // @TODO: Implement

        Handle<Integer> res = Integer::New(0);
        return scope.Close(res);
    }

    void autocomplete::SetCacheExpiration(Local<String> property, Local<Value> value, const AccessorInfo& info) {
        autocomplete* instance = node::ObjectWrap::Unwrap<autocomplete>(info.Holder());

        if (value->IsUint32()) {
            // @TODO: Implement
        } else {
            ThrowException(
                Exception::TypeError(String::New("First argument must be an Integer"))
            );
        }
    }

    Handle<Value> autocomplete::Version(const Arguments& args) {
        HandleScope scope;
        return scope.Close(v8::String::New("1.0"));
    }

    Handle<Value> autocomplete::Complete(const Arguments& args) {
        // Check if the fuction is called correctly
        if (args.Length() != 3)
             return ThrowException(
                Exception::SyntaxError(String::New("Usage: filename, row, collumn"))
            );

        if (!args[0]->IsString())
            return ThrowException(
                Exception::TypeError(String::New("First argument must be a String"))
            );

        if (!args[1]->IsUint32())
            return ThrowException(
                Exception::TypeError(String::New("Second argument must be an Integer"))
            );

        if (!args[2]->IsUint32())
            return ThrowException(
                Exception::TypeError(String::New("Third argument must be an Integer"))
            );

        HandleScope scope;
        autocomplete* instance = node::ObjectWrap::Unwrap<autocomplete>(args.This());

        // Convert string vector to a const char* vector for clang_parseTranslationUnit
        std::vector<const char*> cArgs;
        std::transform( instance->mArgs.begin(), instance->mArgs.end(), std::back_inserter(cArgs),
            [](const std::string &s) -> const char* {
                return s.c_str();
            }
        );

        Handle<Array> ret = Array::New();
        String::Utf8Value file(args[0]);
        uint32_t row = args[1]->ToUint32()->Value();
        uint32_t col = args[2]->ToUint32()->Value();

        // The completion options
        unsigned options = CXTranslationUnit_IncludeBriefCommentsInCodeCompletion | CXTranslationUnit_PrecompiledPreamble | CXTranslationUnit_CacheCompletionResults;

        CXTranslationUnit trans;
        std::string sFile(*file, file.length());
        if (instance->mCache.has(sFile)) {
            // reparsing saves a moderate amount of time
            trans = instance->mCache.get(sFile);
        	clang_reparseTranslationUnit(trans, 0, NULL, 0);
        } else {
            std::cout << cArgs.size() << std::endl;
            trans = clang_parseTranslationUnit(instance->mIndex, *file, &cArgs[0], cArgs.size(), NULL, 0, options);
            instance->mCache.insert(sFile, trans);
        }

        // Check if we were able to build the translation unit
        if (!trans)
            return ThrowException(Exception::Error(String::New("Unable to build translation unit")));

        // Iterate over the code completion results
        CXCodeCompleteResults *res = clang_codeCompleteAt(trans, *file, row, col, NULL, 0, 0);

        uint32_t j = 0;
        for (unsigned i = 0; i < res->NumResults; ++i) {
            // skip unessecary completion results
            if (clang_getCompletionAvailability(res->Results[i].CompletionString) == CXAvailability_NotAccessible || !instance->completeCursor(res->Results[i].CursorKind)) {
                std::cout << "Skipping " << res->Results[i].CursorKind << std::endl;
                continue;
            }

            Handle<Object> rObj = Object::New();
            Handle<Array> rArgs = Array::New();

            uint32_t results = clang_getNumCompletionChunks(res->Results[i].CompletionString);
            CXString cBrief = clang_getCompletionBriefComment(res->Results[i].CompletionString);

            std::string cDescription = (cBrief.data) ? clang_getCString(cBrief) : "";
            std::string cReturnType = instance->returnType(res->Results[i].CursorKind);

            // Populate result
            uint32_t l = 0;
            for (uint32_t k = 0; k < results; ++k) {
                CXCompletionChunkKind cKind = clang_getCompletionChunkKind(res->Results[i].CompletionString, k);

                if (!instance->completeChunk(cKind))
                    continue;

                CXString cText = clang_getCompletionChunkText(res->Results[i].CompletionString, k);

                const char *text = clang_getCString(cText);
                if (!text)
                    text = "";

                if (cKind == CXCompletionChunk_ResultType) {
                    // Better return type than the generic version
                    cReturnType = text;
                } else {
                    rArgs->Set(l++, String::New(text));
                }

                clang_disposeString(cText);
            }

            rObj->Set(String::New("function"), rArgs);
            rObj->Set(String::New("return"), String::New(cReturnType.c_str()));
            rObj->Set(String::New("brief"), String::New(cDescription.c_str()));

            ret->Set(j++, rObj);
        }

        clang_disposeCodeCompleteResults(res);
        return scope.Close(ret);
    }

    Handle<Value> autocomplete::Diagnose(const Arguments& args) {
        // Check if the fuction is called correctly
        if (args.Length() != 1)
             return ThrowException(
                Exception::SyntaxError(String::New("Usage: filename"))
            );

        if (!args[0]->IsString())
            return ThrowException(
                Exception::TypeError(String::New("First argument must be a String"))
            );

        // Create the local scope and get the instance
        HandleScope scope;
        autocomplete* instance = node::ObjectWrap::Unwrap<autocomplete>(args.This());

        // Convert string vector to a const char* vector for clang_parseTranslationUnit
        std::vector<const char*> cArgs;
        std::transform( instance->mArgs.begin(), instance->mArgs.end(), std::back_inserter(cArgs),
            [](const std::string &s) -> const char* {
                return s.c_str();
            }
        );

        Handle<Array> ret = Array::New();
        String::Utf8Value file(args[0]);

        // Don't cache diagnostics
        unsigned options = CXTranslationUnit_PrecompiledPreamble;
        CXTranslationUnit trans = clang_parseTranslationUnit(instance->mIndex, *file, &cArgs[0], cArgs.size(), NULL, 0, options);

        if (!trans)
            return ThrowException(Exception::Error(String::New("Unable to build translation unit")));

        int dOpt = CXDiagnostic_DisplaySourceLocation | CXDiagnostic_DisplayColumn;

        // iterate through diagnostics
        uint32_t j = 0;
        for (uint32_t i = 0; i < clang_getNumDiagnostics(trans); ++i) {
            CXDiagnostic d = clang_getDiagnostic(trans, i);
            CXString str = clang_formatDiagnostic(d, dOpt);
            std::string r(clang_getCString(str));

            ret->Set(j++, String::New(r.c_str()));

            clang_disposeDiagnostic(d);
        }

        return scope.Close(ret);
    }

    bool autocomplete::completeChunk(CXCompletionChunkKind c) {
        switch (c) {
            case CXCompletionChunk_Optional:
            case CXCompletionChunk_TypedText:
            case CXCompletionChunk_Text:
            case CXCompletionChunk_Placeholder:
            case CXCompletionChunk_Informative:
            case CXCompletionChunk_CurrentParameter:
            case CXCompletionChunk_ResultType:
                return true;
            default:
                return false;
        }
    }

    bool autocomplete::completeCursor(CXCursorKind c) {
        switch (c) {
            case CXCursor_CXXMethod:
            //case CXCursor_NotImplemented:
            case CXCursor_FieldDecl:
            case CXCursor_ObjCPropertyDecl:
            case CXCursor_ObjCClassMethodDecl:
            case CXCursor_ObjCInstanceMethodDecl:
            case CXCursor_ObjCIvarDecl:
            case CXCursor_FunctionTemplate:
            //case CXCursor_TypedefDecl:
            case CXCursor_Namespace:
                return true;
            default:
                return false;
        }
    }

    const char* autocomplete::returnType(CXCursorKind ck) {
        switch (ck) {
            case CXCursor_ObjCInterfaceDecl:
            case CXCursor_ClassTemplate:
            case CXCursor_ClassDecl:
                return "class";
            case CXCursor_EnumDecl:
                return "enum";
            case CXCursor_StructDecl:
                return "struct";
            case CXCursor_MacroDefinition:
                return "macro";
            case CXCursor_NamespaceAlias:
            case CXCursor_Namespace:
                return "namespace";
            case CXCursor_TypedefDecl:
                return "typedef";
            case CXCursor_Constructor:
                return "constructor";
            case CXCursor_Destructor:
                return "destructor";
            case CXCursor_UnionDecl:
                return "union";
            default:
                return "";
        }
    }

    void InitAll(Handle<Object> exports) {
        autocomplete::Init(exports);
    }
}

using namespace clang_autocomplete;
NODE_MODULE(clang_autocomplete, InitAll);