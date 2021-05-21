from conans import ConanFile, CMake, tools


class QMapControlConan(ConanFile):
    name = "qmapcontrol"
    version = "1.1.101"
    license = "BSD3"
    author = "Federico Fuga <fuga@studiofuga.com>"
    url = "https://github.com/studiofuga/QMapControl"
    description = "QMapControl is a mapping library that provides a Map control in a QT5 Widget"
    topics = ( "c++")
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = {"shared": True}
    generators = ["cmake_find_package", "cmake"]
    exports_sources = "*"

    def _configure_cmake(self):
        cmake = CMake(self)
        cmake.configure(source_folder=".", args=["-DINSTALL_EXAMPLES=OFF"])
        return cmake

    def build(self):
        cmake = self._configure_cmake()
        cmake.build()

    def build_requirements(self):
        self.build_requires("gdal/3.2.1")

    def package(self):
        cmake = self._configure_cmake()
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["qmapcontrol"]
