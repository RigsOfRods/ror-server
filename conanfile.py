import os
from conan import ConanFile
from conan.tools.files import copy


class RoRServer(ConanFile):
    name = "RoRServer"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.requires("angelscript/2.36.1")
        self.requires("jsoncpp/1.9.5")
        self.requires("openssl/1.1.1v", override=True)
        self.requires("socketw/3.11.0@anotherfoxguy/stable")