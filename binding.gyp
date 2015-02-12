{
  "targets": [
    {
      "target_name": "clang_autocomplete",
      "sources": ["src/autocomplete.cpp"],
      "cflags_cc": [
        "-g",
        "-std=c++0x",
        "-Wall",
        "-Wno-delete-incomplete",
        "-Wno-unused-variable",

        "-I/usr/local/llvm35/include/",
        "-I/usr/lib/",
        "-I/usr/lib64/",
        "-I/usr/lib/llvm",
        "-I/usr/include/",
        "-I/usr/local/include/",
      ],

      "libraries": [
        "-lclang",
        "-lLLVM-3.5",
        "-L/usr/local/llvm35/lib/",
        "-L/usr/lib/x86_64-linux-gnu/",
        "-L/usr/lib/i386-linux-gnu/"
      ]
    }
  ]
}


