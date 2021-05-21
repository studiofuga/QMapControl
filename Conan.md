# Compiling QMapControl with Conan

At the time of writing, Conan doesn't provide a reliable Qt5 recipe.

So Conan can be used to install the dependencies except for Qt5.

After having setup Conan and installed the dependencies with

```
mkdir build
cd build
conan install ..
```

launch cmake with the proper `CMAKE_PREFIX_PATH` as usual

```
cmake -DCMAKE_PREFIX_PATH=C:\Qt\5.15.2\msvc2019_64\lib\cmake\Qt5
```
