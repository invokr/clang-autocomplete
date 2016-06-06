// Get the native module and initialize the library
var clang_autocomplete = require("../.").lib();

// Set the compiler arguments
clang_autocomplete.arguments = new Array("-std=c++0x", "-I/usr/include");

// Print the code completion results
console.log(
    clang_autocomplete.complete("./demo.cpp", 5, 7)
);
