from __future__ import print_function

from distutils import sysconfig

if __name__ == "__main__":
    print()
    print("inc_dir:%s" % sysconfig.get_python_inc())
    # From testing, these interpretations of LIBRARY and LIBPL are valid for
    # both Python 2.7 and Python 3.5 on both Ubuntu and Mac OS X (with Homebrew
    # Python).
    lib_name = sysconfig.get_config_var("LIBRARY")
    if lib_name.startswith("lib"):
        lib_name = lib_name[3:]
    if lib_name.endswith(".a"):
        lib_name = lib_name[:-2]
    print("lib_name:%s" % lib_name)
    lib_ver = lib_name
    if lib_ver.startswith("python"):
        lib_ver = lib_ver[6:]
    print("lib_ver:%s" % lib_ver)
    print("lib_dir:%s" % sysconfig.get_config_var("LIBPL"))
