// Get the native module and initialize the library
var clang_autocomplete = require("clang_autocomplete");
var ccomplete = new clang_autocomplete.lib();

// Set the compiler arguments
ccomplete.arguments = new Array("-std=c++0x", "-I/usr/include", "-I/usr/local/include");

// Print the code completion results
console.log(
    ccomplete.Complete("./demo.cpp", 5, 6)
);
