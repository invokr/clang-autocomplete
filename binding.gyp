{
    "targets": [
        {
            "target_name": "clang_autocomplete",
            "sources": ["src/autocomplete.cpp"],
            "include_dirs": [
                "<!(node -e \"require('nan')\")"
            ],
            "cflags_cc": [
                "-O2",
                "-fomit-frame-pointer",
                "-std=c++0x",
                "-Wall",
                "-Wno-unused-variable",

                "-I/usr/local/llvm38/include/",
                "-I/usr/lib/",
                "-I/usr/lib64/",
                "-I/usr/lib/llvm",
                "-I/usr/lib/llvm-3.8/include",
                "-I/usr/include/",
                "-I/usr/local/include/",
            ],

            "libraries": [
                "-lclang",
                "-lLLVM-3.8",
                "-L/usr/local/llvm35/lib/",
                "-L/usr/lib/x86_64-linux-gnu/",
                "-L/usr/lib/i386-linux-gnu/",
                "-L/usr/lib/llvm-3.8/lib"
            ]
        }
    ]
}
