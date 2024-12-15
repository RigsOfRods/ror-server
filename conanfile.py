import os
from conan import ConanFile
from conan.tools.files import copy


class RoRServer(ConanFile):
    name = "RoRServer"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def layout(self):
        self.folders.generators = os.path.join(self.folders.build, "generators")

    def requirements(self):
        self.requires("angelscript/2.37.0")
        self.requires("jsoncpp/1.9.5")
        self.requires("openssl/3.3.2", override=True)
        self.requires("socketw/3.11.0@anotherfoxguy/stable")
        self.requires("libcurl/8.10.1")