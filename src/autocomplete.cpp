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
Nan::Persistent<v8::Function> autocomplete::constructor;

autocomplete::autocomplete() : mArgs(), mIndex(nullptr) {
        // create the clang index: excludeDeclarationsFromPCH = 1, displayDiagnostics = 1
        mIndex = clang_createIndex(1, 1);

        // If an object is purged from the cache, dispose it's translation unit
        mCache.set_purge_callback([] (std::string K, CXTranslationUnit V)noexcept {
                        clang_disposeTranslationUnit(V);
                });
}

autocomplete::~autocomplete() {
        clang_disposeIndex(mIndex);
}

NAN_METHOD(autocomplete::New) {
        if (info.IsConstructCall()) {
                autocomplete *c = new autocomplete();
                c->Wrap(info.This());
                info.GetReturnValue().Set(info.This());
        } else {
                v8::Local<v8::Function> cons = Nan::New(constructor);
                info.GetReturnValue().Set(cons->NewInstance());
        }
}

NAN_MODULE_INIT(autocomplete::Init) {
        // Wrap new and make it persistend
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);

        tpl->InstanceTemplate()->SetInternalFieldCount(1);
        tpl->SetClassName(Nan::New("lib").ToLocalChecked());

        // Accessor for args and cache_expiration
        Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("arguments").ToLocalChecked(), GetArgs, SetArgs);
        Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("cache_expiration").ToLocalChecked(), GetCacheExpiration, SetCacheExpiration);

        // Make our methods available to Node
        Nan::SetPrototypeMethod(tpl, "version", Version);
        Nan::SetPrototypeMethod(tpl, "complete", Complete);
        Nan::SetPrototypeMethod(tpl, "diagnose", Diagnose);
        Nan::SetPrototypeMethod(tpl, "memoryUsage", MemoryUsage);
        Nan::SetPrototypeMethod(tpl, "clearCache", ClearCache);

        Nan::SetPrototypeMethod(tpl, "Version", Version);
        Nan::SetPrototypeMethod(tpl, "Complete", Complete);
        Nan::SetPrototypeMethod(tpl, "Diagnose", Diagnose);
        Nan::SetPrototypeMethod(tpl, "MemoryUsage", MemoryUsage);
        Nan::SetPrototypeMethod(tpl, "ClearCache", ClearCache);

        constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
        Nan::Set(target, Nan::New("lib").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_GETTER(autocomplete::GetArgs) {
        autocomplete* instance = Nan::ObjectWrap::Unwrap<autocomplete>(info.Holder());

        // Get a list of all arguments for node
        v8::Local<v8::Array> res = Nan::New<v8::Array>(instance->mArgs.size());
        for (uint32_t i = 0; i < instance->mArgs.size(); ++i)
                res->Set(i, Nan::New(instance->mArgs[i].c_str()).ToLocalChecked());

        info.GetReturnValue().Set(res);
}

NAN_SETTER(autocomplete::SetArgs) {
        autocomplete* instance = Nan::ObjectWrap::Unwrap<autocomplete>(info.Holder());
        instance->mArgs.clear();
        instance->mCache.clear();

        if (value->IsArray()) {
                // If we get multiple arguments, clear the list and append them all
                instance->mArgs.clear();

                v8::Local<v8::Array> arr = v8::Local<v8::Array>::Cast(value);
                instance->mArgs.reserve(arr->Length());

                for (std::size_t i = 0; i < arr->Length(); ++i) {
                        v8::String::Utf8Value str(arr->Get(i));
                        instance->mArgs.push_back( *str );
                }
        } else if (value->IsString()) {
                // If we get a single string, append it
                v8::String::Utf8Value str(value);
                instance->mArgs.push_back(*str);
        } else {
                Nan::ThrowTypeError("First argument must be a String or an Array");
                return;
        }
}

NAN_GETTER(autocomplete::GetCacheExpiration) {
        autocomplete* instance = Nan::ObjectWrap::Unwrap<autocomplete>(info.Holder());
        info.GetReturnValue().Set(Nan::New(instance->mCache.get_expiration()));
}

NAN_SETTER(autocomplete::SetCacheExpiration) {
        autocomplete* instance = Nan::ObjectWrap::Unwrap<autocomplete>(info.Holder());

        if (value->IsUint32()) {
                instance->mCache.set_expiration(value->Uint32Value());
        } else {
                Nan::ThrowTypeError("First argument must be an Integer");
                return;
        }
}

NAN_METHOD(autocomplete::Version) {
        CXString clang_v = clang_getClangVersion();

        std::string ver = std::string("0.3.2 (clang-autocomplete); ")+std::string(clang_getCString(clang_v));
        clang_disposeString(clang_v);

        info.GetReturnValue().Set(Nan::New(ver.c_str()).ToLocalChecked());
}

NAN_METHOD(autocomplete::Complete) {
        // Check if the fuction is called correctly
        if (info.Length() != 3) {
                Nan::ThrowSyntaxError("Usage: filename, row, column");
                return;
        }

        if (!info[0]->IsString()) {
                Nan::ThrowSyntaxError("First argument must be a String");
                return;
        }

        if (!info[1]->IsUint32()) {
                Nan::ThrowSyntaxError("Second argument must be an Integer");
                return;
        }

        if (!info[2]->IsUint32()) {
                Nan::ThrowSyntaxError("Third argument must be an Integer");
                return;
        }

        autocomplete* instance = Nan::ObjectWrap::Unwrap<autocomplete>(info.This());

        v8::Local<v8::Array> ret = Nan::New<v8::Array>();

        // Convert string vector to a const char* vector for clang_parseTranslationUnit
        std::vector<const char*> cArgs;
        std::transform( instance->mArgs.begin(), instance->mArgs.end(), std::back_inserter(cArgs),
                        [](const std::string &s) -> const char* {
                        return s.c_str();
                }
                        );

        v8::String::Utf8Value file(info[0]);
        uint32_t row = info[1]->ToUint32()->Value();
        uint32_t col = info[2]->ToUint32()->Value();

        // The completion options
        unsigned options = CXTranslationUnit_PrecompiledPreamble | CXTranslationUnit_CacheCompletionResults;

        CXTranslationUnit trans;
        std::string sFile(*file, file.length());
        if (instance->mCache.has(sFile)) {
                // reparsing saves a moderate amount of time
                trans = instance->mCache.get(sFile);
                clang_reparseTranslationUnit(trans, 0, NULL, 0);
        } else {
                //trans = clang_parseTranslationUnit(instance->mIndex, *file, &cArgs[0], cArgs.size(), NULL, 0, options);
                if (CXErrorCode err = clang_parseTranslationUnit2(instance->mIndex, *file, &cArgs[0], cArgs.size(), NULL, 0, options, &trans)) {
                        // TODO: process error
                        Nan::ThrowError("Unable to build translation unit");
                        return;
                }

                instance->mCache.insert(sFile, trans);
        }

        // Check if we were able to build the translation unit
        if (!trans) {
                Nan::ThrowError("Unable to build translation unit");
                return;
        }

        // Iterate over the code completion results
        CXCodeCompleteResults *res = clang_codeCompleteAt(trans, *file, row, col, NULL, 0, 0);


        uint32_t j = 0;
        for (unsigned i = 0; i < res->NumResults; ++i) {
                // skip unessecary completion results
                if (clang_getCompletionAvailability(res->Results[i].CompletionString) == CXAvailability_NotAccessible)
                        continue;

                v8::Local<v8::Object> rObj = Nan::New<v8::Object>();
                v8::Local<v8::Array> rArgs = Nan::New<v8::Array>();
                v8::Local<v8::Array> rQualifiers = Nan::New<v8::Array>();

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
                                        rArgs->Set(l++, Nan::New(text).ToLocalChecked());
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
                                        rArgs->Set(l++, Nan::New(text).ToLocalChecked());
                                        break;

                                case CXCompletionChunk_Informative:
                                        rQualifiers->Set(m++, Nan::New(text).ToLocalChecked()); // does not seem to propagate noexcept, etc.
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
                                        rArgs->Set(l++, Nan::New(text).ToLocalChecked());
                                        break;

                                case CXCompletionChunk_Informative:
                                        rQualifiers->Set(m++, Nan::New(text).ToLocalChecked()); // does not seem to propagate noexcept
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
                        rObj->Set(Nan::New("name").ToLocalChecked(), Nan::New(cName.c_str()).ToLocalChecked());
                        rObj->Set(Nan::New("type").ToLocalChecked(), Nan::New(cType.c_str()).ToLocalChecked());
                        rObj->Set(Nan::New("return").ToLocalChecked(), Nan::New(cReturnType.c_str()).ToLocalChecked());
                        rObj->Set(Nan::New("description").ToLocalChecked(), Nan::New(cDescription.c_str()).ToLocalChecked());
                        rObj->Set(Nan::New("params").ToLocalChecked(), rArgs);
                        rObj->Set(Nan::New("qualifiers").ToLocalChecked(), rQualifiers);

                        ret->Set(j++, rObj);
                }
        }

        clang_disposeCodeCompleteResults(res);

        info.GetReturnValue().Set(ret);
}

NAN_METHOD(autocomplete::Diagnose) {
        // Check if the fuction is called correctly
        if (info.Length() != 1) {
                Nan::ThrowSyntaxError("Usage: filename");
                return;
        }

        if (!info[0]->IsString()) {
                Nan::ThrowSyntaxError("First argument must be an String");
                return;
        }

        // Create the local scope and get the instance
        autocomplete* instance = Nan::ObjectWrap::Unwrap<autocomplete>(info.This());

        // Convert string vector to a const char* vector for clang_parseTranslationUnit
        std::vector<const char*> cArgs;
        std::transform( instance->mArgs.begin(), instance->mArgs.end(), std::back_inserter(cArgs),
                        [](const std::string &s) -> const char* {
                        return s.c_str();
                }
                        );

        v8::Local<v8::Array> ret = Nan::New<v8::Array>();
        v8::String::Utf8Value file(info[0]);

        // Don't cache diagnostics
        unsigned options = CXTranslationUnit_PrecompiledPreamble | clang_defaultDiagnosticDisplayOptions();
        CXTranslationUnit trans = clang_parseTranslationUnit(instance->mIndex, *file, &cArgs[0], cArgs.size(), NULL, 0, options);

        if (!trans) {
                Nan::ThrowError("Unable to build translation unit");
                return;
        }

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
                v8::Local<v8::Array> diagnostics = Nan::New<v8::Array>();
                diagnostics->Set(0, Nan::New(clang_getCString(file)).ToLocalChecked());
                diagnostics->Set(1, Nan::New(line));
                diagnostics->Set(2, Nan::New(col));
                diagnostics->Set(3, Nan::New(clang_getCString(str)).ToLocalChecked());
                diagnostics->Set(4, Nan::New(clang_getDiagnosticSeverity(d)));

                ret->Set(j++, diagnostics);

                clang_disposeString(file);
                clang_disposeString(str);
                clang_disposeDiagnostic(d);
        }

        info.GetReturnValue().Set(ret);
}

NAN_METHOD(autocomplete::MemoryUsage) {
        autocomplete* instance = Nan::ObjectWrap::Unwrap<autocomplete>(info.This());

        v8::Local<v8::Array> ret = Nan::New<v8::Array>();
        uint32_t j = 0;

        for (auto &e : instance->mCache) {
                CXTranslationUnit unit = e.second.value;
                CXTUResourceUsage res = clang_getCXTUResourceUsage(unit);
                uint32_t all = 0;

                for (unsigned i = 0; i < res.numEntries; ++i ) {
                        CXTUResourceUsageEntry entry = res.entries[i];
                        if (entry.kind <= 14)
                                all += entry.amount;
                }

                v8::Local<v8::Array> entry = Nan::New<v8::Array>();
                entry->Set(0, Nan::New(e.first.c_str()).ToLocalChecked());
                entry->Set(1, Nan::New(all));
                ret->Set(j++, entry);

                clang_disposeCXTUResourceUsage(res);
        }

        info.GetReturnValue().Set(ret);
}

NAN_METHOD(autocomplete::ClearCache) {
        autocomplete* instance = Nan::ObjectWrap::Unwrap<autocomplete>(info.This());

        if (info.Length() == 1) {
                if (!info[0]->IsString()) {
                        Nan::ThrowSyntaxError("First argument must be a String or undefined");
                        return;
                }

                v8::String::Utf8Value file(info[0]);
                instance->mCache.remove(std::string(*file, file.length()));
        } else {
                instance->mCache.clear();
        }

        info.GetReturnValue().Set(Nan::Undefined());
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

NAN_MODULE_INIT(InitAll) {
        autocomplete::Init(target);
}
}

using namespace clang_autocomplete;

NODE_MODULE(clang_autocomplete, InitAll);
