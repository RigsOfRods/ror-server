project.name = "RoRserver"

package = newpackage()
package.name = "PDCurses"
package.kind = "lib"
package.language = "c++"
package.includepaths = { "PDCurses-3.3" }
if windows then
	package.files = { matchfiles("PDCurses-3.3/win32/*.c") }
else
	package.files = { matchfiles("PDCurses-3.3/x11/*.c") }
end

package = newpackage()
package.name = "rorserver"
package.kind = "exe"
package.language = "c++"
package.files = { matchfiles("*.cpp"), matchfiles("*.c") }
package.objdir = "obj"

if windows then
	package.includepaths = { "SocketW/src/", "win32_pthread", "PDCurses-3.3"}
	package.libpaths = { "win32_pthread", "PDCurses-3.3/win32"}
	package.defines = { "__WIN32__", "_CRT_SECURE_NO_WARNINGS" }
	package.links = { "kernel32", "wsock32", "SocketW", "pthreadVC2", "WSOCK32", "ws2_32", "PDCurses"}
else
	package.includepaths = { "SocketW/src/", "PDCurses-3.3"}
	package.libpaths = { "PDCurses-3.3/x11"}
	package.links = { "SocketW", "pthread", "PDCurses", "XCurses"}
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
