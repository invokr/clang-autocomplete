clang-autocomplete
==================

This library provides C/C++ code completion using the libclang-c bindings.

Usage disclaimer: This library is in alpha stage.

Installing
----------

The library is available via npm under the name `clang-autocomplete`.

Installation is as easy as:

    npm install clang-autocomplete

Because each and every distribution feels the need to put the headers for
libclang-c in a different place, you might need to add the correct include path
for your distribution in `bindings.gyp`.

Usage
-----

See demo/demo.js for a quick example.

API
---

Methods:

    Version();                      // Returns the current library and clang version
    Complete(filename, row, column) // Completes the statement at the given file position
    Diagnose(filename)              // Returns clang's diagnostic information
    MemoryUsage()                   // Returns the translation unit cache's memory usage in bytes for each file
    ClearCache()                    // Removes all cached translation units

Attributes:

    arguments = [];        // Arguments provided to libclang, e.g. ["-I/usr/include"]
    cache_expiration = 10; // Number of minutes after which a cache entry expires
