project.name = "RoRserver"

package = newpackage()
package.name = "rorserver"
package.kind = "exe"
package.language = "c++"
package.files = { matchfiles("*.cpp") }
package.objdir = "obj"

if windows then
	package.includepaths = { "SocketW/src/", "win32_pthread" }
	package.libpaths = { "win32_pthread" }
	package.defines = { "__WIN32__", "_CRT_SECURE_NO_WARNINGS" }
	package.links = { "kernel32", "wsock32", "SocketW", "pthreadVC2", "WSOCK32", "ws2_32"}
else
	package.includepaths = { "SocketW/src/"}
	package.links = { "SocketW", "pthread" }
end

package = newpackage()
package.name = "SocketW"
package.kind = "lib"
package.language = "c++"
if windows then
	package.defines = { "__WIN32__", "WIN32", "_DEBUG", "_WINDOWS", "_USRDLL", "MYSOCKETW_EXPORTS"}
end
package.files = { matchfiles("SocketW/src/*.cxx") }
package.objdir = "SocketW/obj"
