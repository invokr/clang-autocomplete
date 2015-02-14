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
        //autocomplete* instance = node::ObjectWrap::Unwrap<autocomplete>(info.Holder());

        // @TODO: Implement

        Handle<Integer> res = Integer::New(0);
        return scope.Close(res);
    }

    void autocomplete::SetCacheExpiration(Local<String> property, Local<Value> value, const AccessorInfo& info) {
        //autocomplete* instance = node::ObjectWrap::Unwrap<autocomplete>(info.Holder());

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
        CXString clang_v = clang_getClangVersion();

        std::string ver = std::string("0.2.0 (clang-autocomplete); ")+std::string(clang_getCString(clang_v));
        clang_disposeString(clang_v);

        return scope.Close(v8::String::New(ver.c_str()));
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
        unsigned options = CXTranslationUnit_PrecompiledPreamble | CXTranslationUnit_CacheCompletionResults;

        CXTranslationUnit trans;
        std::string sFile(*file, file.length());
        if (instance->mCache.has(sFile)) {
            // reparsing saves a moderate amount of time
            trans = instance->mCache.get(sFile);
        	clang_reparseTranslationUnit(trans, 0, NULL, 0);
        } else {
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
            if (clang_getCompletionAvailability(res->Results[i].CompletionString) == CXAvailability_NotAccessible)
                continue;

            Handle<Object> rObj = Object::New();
            Handle<Array> rArgs = Array::New();
            Handle<Array> rQualifiers = Array::New();

            uint32_t results = clang_getNumCompletionChunks(res->Results[i].CompletionString);

            std::string cName = "";
            std::string cType = "";
            std::string cDescription = "";
            std::string cReturnType = "";
            std::string cParam = "";

            // Populate result
            uint32_t l = 0;
            uint32_t m = 0;
            for (uint32_t k = 0; k < results; ++k) {
                CXCompletionChunkKind cKind = clang_getCompletionChunkKind(res->Results[i].CompletionString, k);

                // Get the completion chunk text
                CXString cText = clang_getCompletionChunkText(res->Results[i].CompletionString, k);
                const char *text = clang_getCString(cText);
                if (!text)
                    text = "";

                // Switch on the completion type
                switch (res->Results[i].CursorKind) {
                    // class / union / struct / enum
                    case CXCursor_UnionDecl:
                    case CXCursor_ClassDecl:
                    case CXCursor_StructDecl:
                    case CXCursor_EnumDecl:
                        cType = "def";
                        cName = text; // struct only emits text once
                        cDescription = std::string(instance->returnType(res->Results[i].CursorKind)) + " " + cName;
                        break;

                    // enum completion
                    case CXCursor_EnumConstantDecl:
                        if (cKind == CXCompletionChunk_ResultType) {
                            cReturnType = ""; // parent enum
                            cDescription = std::string("enum ") + cReturnType + "::" + cName;
                        } else {
                            cName = text; // variable
                            cType = "enum_member";
                        }
                        break;

                    // a single function decleration
                    case CXCursor_FunctionDecl:
                        switch (cKind) {
                            case CXCompletionChunk_ResultType:
                                cType = "function";
                                cReturnType = text; // function return type;
                                break;

                            case CXCompletionChunk_TypedText:
                                cName = text; // function name
                                break;

                            case CXCompletionChunk_Placeholder:
                                rArgs->Set(l++, String::New(text));
                                break;

                            default:
                                break;
                        }
                        break;

                    // a variable
                    case CXCursor_VarDecl:
                        if (cKind == CXCompletionChunk_ResultType) {
                            cReturnType = text; // type
                        } else {
                            cName = text; // variable
                            cType = "variable";
                        }
                        break;

                    // typedef
                    case CXCursor_TypedefDecl:
                        cName = text;
                        cType = "typedef";
                        break;

                    // class member function
                    case CXCursor_CXXMethod:
                        switch (cKind) {
                            case CXCompletionChunk_ResultType:
                                cType = "method";
                                cReturnType = text; // function return type;
                                break;

                            case CXCompletionChunk_TypedText:
                                cName = text; // function name
                                break;

                            case CXCompletionChunk_Placeholder:
                                rArgs->Set(l++, String::New(text));
                                break;

                            case CXCompletionChunk_Informative:
                                rQualifiers->Set(m++, String::New(text)); // does not seem to propagate noexcept, etc.
                                break;

                            default:
                                break;
                        }
                        break;

                    // class member variable
                    case CXCursor_FieldDecl:
                        if (cKind == CXCompletionChunk_ResultType) {
                            cReturnType = text; // type
                        } else {
                            cName = text; // variable
                            cType = "member";
                        }
                        break;

                    // namespace
                    case CXCursor_Namespace:
                        if (cKind == CXCompletionChunk_TypedText) {
                            cType = "namespace";
                            cName = text;
                        }
                        break;

                    // class constructor
                    case CXCursor_Constructor:
                        switch (cKind) {
                            case CXCompletionChunk_TypedText:
                                cName = text; // class name
                                cType = "constructor";
                                break;

                            case CXCompletionChunk_Placeholder:
                                rArgs->Set(l++, String::New(text));
                                break;

                            case CXCompletionChunk_Informative:
                                rQualifiers->Set(m++, String::New(text)); // does not seem to propagate noexcept
                                break;

                            default:
                                break;
                        }
                        break;

                    // Sometimes points to the current parameter
                    case CXCursor_NotImplemented:
                        if (cKind == CXCompletionChunk_CurrentParameter) {
                            cType = "current";
                            cName = text;
                        }
                        break;

                    // default
                    default:
                        clang_disposeString(cText);
                        continue;
                }

                clang_disposeString(cText);
            }

            if (cType != "") {
                rObj->Set(String::New("name"), String::New(cName.c_str()));
                rObj->Set(String::New("type"), String::New(cType.c_str()));
                rObj->Set(String::New("return"), String::New(cReturnType.c_str()));
                rObj->Set(String::New("description"), String::New(cDescription.c_str()));
                rObj->Set(String::New("params"), rArgs);
                rObj->Set(String::New("qualifiers"), rQualifiers);

                ret->Set(j++, rObj);
            }
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

        int dOpt = 0;

        // iterate through diagnostics
        uint32_t j = 0;
        for (uint32_t i = 0; i < clang_getNumDiagnostics(trans); ++i) {
            CXDiagnostic d = clang_getDiagnostic(trans, i);

            // get diagnostic warning
            CXString str = clang_formatDiagnostic(d, dOpt);

            // get diagnostic location
            CXString file;
            unsigned line;
            unsigned col;
            CXSourceLocation loc = clang_getDiagnosticLocation(d);
            clang_getPresumedLocation(loc, &file, &line, &col);

            // propagate to node
            Handle<Array> diagnostics = Array::New();
            diagnostics->Set(0, String::New(clang_getCString(file)));
            diagnostics->Set(1, Number::New(line));
            diagnostics->Set(2, Number::New(col));
            diagnostics->Set(3, String::New(clang_getCString(str)));
            diagnostics->Set(4, Number::New(clang_getDiagnosticSeverity(d)));

            ret->Set(j++, diagnostics);

            clang_disposeString(file);
            clang_disposeString(str);
            clang_disposeDiagnostic(d);
        }

        return scope.Close(ret);
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
