def FlagsForFile(filename, **kwargs):
    return {
            "flags": [
                    "-x", "c++",
                    "-std=c++11",
                    "-I", ".",
                    "-I", "/usr/include/python2.7",
                    "-Wall",
            ],
            "do_cache": True,
    }
