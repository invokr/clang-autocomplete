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

#include <v8.h>
#include <node.h>

#include <clang-c/Index.h>
#include "dated_map.hpp"

using namespace v8;

namespace clang_autocomplete {
    /** Provides auto-completion functionality through clang's C interface */
    class autocomplete : public node::ObjectWrap {
    public:
        /** Persistend constructor obj for v8 */
        static Persistent<FunctionTemplate> constructor;

        /** Node's initialize function */
        static void Init(Handle<Object> target);

        /** Returns module version */
        static Handle<Value> Version(const Arguments& args);

        /** Returns the current arguments supplied to clang */
        static Handle<Value> GetArgs(Local<String> property, const AccessorInfo& info);

        /** Sets the arguments supplied to clang */
        static void SetArgs(Local<String> property, Local<Value> value, const AccessorInfo& info);

        /** Returns the cache expiration time. */
        static Handle<Value> GetCacheExpiration(Local<String> property, const AccessorInfo& info);

        /** Sets cache expiration time */
        static void SetCacheExpiration(Local<String> property, Local<Value> value, const AccessorInfo& info);

        /** Completes the code at [filename|row|col] */
        static Handle<Value> Complete(const Arguments& args);

        /** Returns code diagnostic information for [filename] */
        static Handle<Value> Diagnose(const Arguments& args);
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
        static Handle<Value> New(const Arguments& args);

        /** Returns the type of the completion function */
        const char* returnType(CXCursorKind ck);
    };
}

#endif /* _CLANG_AUTOCOMPLETE_AUTOCOMPLETE_HPP_ */