from conans import ConanFile, CMake

class DBusConan(ConanFile):
    name = "DBusxpat"
    license = "Academic Free License version 2.1"
    url = "https://www.freedesktop.org/wiki/Software/dbus/"
    description = "D-Bus is a message bus system"
    settings = "os", "compiler", "build_type", "arch"
    options = {}
    default_options = ""
    generators = "cmake"
    exports_sources = "*", "!build/*"
    
    requires = (
        "libexpat/2.2.7.0@barco/healthcare"
    )
    
    def build(self):
        cmake = CMake(self)
        cmake.configure(source_folder="..\cmake")
        cmake.build()
        cmake.install()

#    def package(self):
#         nothing to do here. All is handled by cmake.install() above.
        
    def package_info(self):
        self.cpp_info.libs = ["DBus"]
