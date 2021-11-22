# PerFlow
Domain-specific framework for performance analysis of parallel programs

## Dependency and Build

PerFlow is dependent on
* [Dyninst](https://github.com/dyninst/dyninst)
* Boost
    Boost will be installed automatically with Dyninst.
* [PAPI](https://bitbucket.org/icl/papi/src/master/)
* [igraph](https://github.com/igraph/igraph)
* cmake >= 3.16

Dyninst and PAPI need user to build themselves. ```igraph``` has been integrated into PerFlow as submodule. 
```bash
git submodule update --init
```

There are two ways to build PerFlow. One is to build dependency from source and specify their location when building PerFlow, the other is to use spack to build these.

### Build Dependency from Source

```bash
cmake .. -DBOOST_ROOT=/path_to_your_boost_install_dir -DDyninst_DIR=/path_to_your_dyninst_install_dir/lib/cmake/Dyninst -DPAPI_PREFIX=/path_to_your_papi_install_dir

# You should make sure that there is `DyninstConfig.cmake` in /path_to_your_dyninst_install_dir/lib/cmake/Dyninst
# And there is `include` `lib` in /path_to_your_papi_install_dir
# And there is `include` `lib` in /path_to_your_boost_install_dir, `boost` in /path_to_your_boost_install_dir/include
```

Note that if dyninst is built from source, the boost will be downloaded and installed automatically with it, in the install directory of dyninst.

In this case, the cmake commands will be like
```
cmake .. -DBOOST_ROOT=/path_to_your_dyninst_install_dir  -DDyninst_DIR=/path_to_your_dyninst_install_dir/lib/cmake/Dyninst -DPAPI_PREFIX=/path_to_your_papi_install_dir
```


### Build Dependency from Spack
The recommended way to build Dyninst (with Boost) and PAPI is to use [Spack](https://github.com/spack/spack)

```bash
spack install dyninst # boost will be installed at the same time
spack install papi

# before building PerFlow
spack load dyninst # boost will be loaded at the same time
spack load papi

# build
mkdir build
cd build
cmake ..
```