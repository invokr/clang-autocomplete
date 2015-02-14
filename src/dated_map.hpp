/**
* @file dated_map.hpp
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

#ifndef _CLANG_AUTOCOMPLETE_DATED_MAP_HPP_
#define _CLANG_AUTOCOMPLETE_DATED_MAP_HPP_

#include <unordered_map>
#include <functional>
#include <type_traits>

#include <cassert>
#include <cstddef>
#include <ctime>

namespace clang_autocomplete {
    namespace detail {
        /** Utility function that calls delete on pointers */
        template <typename T, typename std::enable_if<std::is_pointer<T>::value>::type* = nullptr>
        void delete_if_pointer(T ptr) {
            delete ptr;
        }

        template <typename T, typename std::enable_if<!std::is_pointer<T>::value>::type* = nullptr>
        void delete_if_pointer(T ptr) {}
    }

    /** Class providing key expiration based on a specified time interval or maximum size */
    template <typename K, typename V>
    class dated_map {
    public:
        /** Structure for a single map entry */
        struct entry {
            /** Time when the entry was inserted */
            time_t time_inserted;
            /** Time when the entry was last accessed */
            time_t time_accessed;
            /** Value */
            V value;
        };

        /** Type for the callback-function run on each deleted entry */
        typedef std::function<void(K, V) noexcept> callback_type;
        /** Map that keeps track of all insertes values */
        typedef std::unordered_map<K, entry> container;
        /** Iterator type forwarded from contaier */
        typedef typename container::iterator iterator;
        /** Size type forward */
        typedef typename container::size_type size_type;
        /** Value type forward */
        typedef typename container::value_type value_type;

        /** Constructor */
        dated_map() : mCheckInterval(10), mExpirationTime(30), mLastCheck(time(NULL)), mCb() {

        }

        /** Destructor, calls purge function on remaining entries. */
        ~dated_map() {
            for (auto &it : mEntries) {
                mCb(it.first, it.second.value);
            }
        }

        /** Removed copy constructor */
        dated_map(const dated_map&) = delete;

        /** Removed copy assignment operator */
        dated_map& operator=(const dated_map&) = delete;

        /** Default move operator */
        dated_map(dated_map&&) = default;

        /** Points to begining of entry map */
        iterator begin() noexcept {
            return mEntries.begin();
        }

        /** Points to end of entry map */
        iterator end() noexcept {
            return mEntries.end();
        }

        /** Returns iterator pointing at element or end() if not found */
        iterator find(const K& key) noexcept {
            return mEntries.find(key);
        }

        /** Checks if an entry exists */
        bool has(const K& key) noexcept {
            return (mEntries.find(key) != mEntries.end());
        }

        /** Returns the value of key in question */
        V get(const K& key) {
            auto it = mEntries.find(key);
            assert(it != mEntries.end());

            // bump access time
            it->second.time_accessed = time(NULL);

            // check if we need to purge stuff
            time_t time_cur = time(NULL);
            if ((mLastCheck + (mCheckInterval*60)) < time_cur) {
                for (auto it = mEntries.begin(); it != mEntries.end(); ++it) {
                    if (it->second.time_accessed < (time_cur - (mExpirationTime*60))) {
                        mCb(it->first, it->second.value);
                        mEntries.erase(it++);
                    }
                }
            }

            return it->second.value;
        }

        /** Pushes a new entry */
        void insert(K key, V value) {
            mEntries.insert({key, {time(NULL), time(NULL), value}});
        }

        /** Sets the minimum time before an entry expires, use 0 for indefinite storage. */
        void set_expiration(uint32_t expiration_time) noexcept {
            mExpirationTime = expiration_time;
        }

        /** Sets the time between expiration checks */
        void set_frequency(uint32_t check_frequency) noexcept {
            mCheckInterval = check_frequency;
        }

        /** Sets the callback function invoked on each deleted entry */
        void set_purge_callback(callback_type fcn) {
            mCb = fcn;
        }
    private:
        /** Time between expiration checks */
        uint32_t mCheckInterval;
        /** Time before an item expires */
        uint32_t mExpirationTime;
        /** Time of the last check */
        time_t mLastCheck;

        /** Map of entries */
        container mEntries;
        /** Deletion callback */
        callback_type mCb;
    };
}

#endif /* _CLANG_AUTOCOMPLETE_DATED_MAP_HPP_ */