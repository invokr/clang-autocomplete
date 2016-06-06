/**
* @file autocomplete.hpp
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

#ifndef _CLANG_AUTOCOMPLETE_AUTOCOMPLETE_HPP_
#define _CLANG_AUTOCOMPLETE_AUTOCOMPLETE_HPP_

#include <string>
#include <vector>

#include <nan.h>

#include <clang-c/Index.h>
#include "dated_map.hpp"

namespace clang_autocomplete {
    /** Provides auto-completion functionality through clang's C interface */
    class autocomplete : public Nan::ObjectWrap {
    public:
        /** Persistend constructor obj for v8 */
        static Nan::Persistent<v8::Function> constructor;

        /** Node's initialize function */
        //static void Init(Handle<Object> target);
        static NAN_MODULE_INIT(Init);

        /** Returns module version */
        //static Handle<Value> Version(const FunctionCallbackInfo<Value>& args);
        static NAN_METHOD(Version);

        /** Returns the current arguments supplied to clang */
        //static Handle<Value> GetArgs(Local<String> property, const AccessorInfo& info);
        static NAN_GETTER(GetArgs);

        /** Sets the arguments supplied to clang */
        //void SetArgs(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info);
        static NAN_SETTER(SetArgs);

        /** Returns the cache expiration time. */
        //static Handle<Value> GetCacheExpiration(Local<String> property, const AccessorInfo& info);
        static NAN_GETTER(GetCacheExpiration);

        /** Sets cache expiration time */
        //static void SetCacheExpiration(Local<String> property, Local<Value> value, const AccessorInfo& info);
        static NAN_SETTER(SetCacheExpiration);

        /** Completes the code at [filename|row|col] */
        //static Handle<Value> Complete(const FunctionCallbackInfo<Value>& args);
        static NAN_METHOD(Complete);

        /** Returns code diagnostic information for [filename] */
        //static Handle<Value> Diagnose(const FunctionCallbackInfo<Value>& args);
        static NAN_METHOD(Diagnose);

        /** Returns memory usage of cached translation units in bytes */
        //static Handle<Value> MemoryUsage(const FunctionCallbackInfo<Value>& args);
        static NAN_METHOD(MemoryUsage);

        /** Purges all cached translation units */
        //static Handle<Value> ClearCache(const FunctionCallbackInfo<Value>& args);
        static NAN_METHOD(ClearCache);
    private:
        /** List of arguments passed to clang */
        std::vector<std::string> mArgs;
        /** Internal set of translation units. */
        CXIndex mIndex;
        /** Cache for the translation units. */
        dated_map<std::string, CXTranslationUnit> mCache;

        /** Constructor, initialized index */
        autocomplete();

        /** Destructor, cleans up index */
        ~autocomplete();

        /** Invoked when a new instance is created in NodeJs */
        //static Handle<Value> New(const FunctionCallbackInfo<Value>& args);
        static NAN_METHOD(New);

        /** Returns the type of the completion function */
        const char* returnType(CXCursorKind ck);
    };
}

#endif /* _CLANG_AUTOCOMPLETE_AUTOCOMPLETE_HPP_ */
