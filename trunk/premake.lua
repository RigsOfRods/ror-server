project.name = "RoRserver"

--package = newpackage()
--package.name = "PDCurses"
--package.kind = "lib"
--package.language = "c++"
--package.includepaths = { "PDCurses-3.3" }
--if windows then
--	package.files = { matchfiles("PDCurses-3.3/win32/*.c", "PDCurses-3.3/pdcurses/*.c")}
--	package.defines = { "_CRT_SECURE_NO_WARNINGS" }
--else
--	package.files = { matchfiles("PDCurses-3.3/*.c") }
--end
--package.objdir = "PDCurses-3.3/obj"

package = newpackage()
package.name = "SocketW"
package.kind = "dll"
package.language = "c++"
if windows then
	package.config["Debug"].defines = { "__WIN32__", "WIN32", "_DEBUG", "_WINDOWS", "_USRDLL", "MYSOCKETW_EXPORTS"}
	package.config["Release"].defines = { "__WIN32__", "WIN32", "NDEBUG", "_WINDOWS", "_USRDLL", "MYSOCKETW_EXPORTS"}
	package.links = { "ws2_32" }
else
	package.config["Debug"].defines = { "_DEBUG", "MYSOCKETW_EXPORTS"}
	package.config["Release"].defines = { "NDEBUG", "MYSOCKETW_EXPORTS"}
end

package.config["Debug"].target = "SocketW_d"
package.config["Release"].target = "SocketW"


package.files = { matchfiles("SocketW/src/*.cxx") }
package.objdir = "SocketW/obj"


package = newpackage()
package.name = "rorserver"
package.kind = "exe"
package.language = "c++"
package.files = { matchfiles("source/*.cpp"), matchfiles("source/*.c") }
package.objdir = "source/obj"

if windows then
	package.includepaths = { "SocketW/src/", "win32_pthread"}
	package.libpaths = { "win32_pthread"}
	package.defines = { "__WIN32__", "_CRT_SECURE_NO_WARNINGS" }
	package.links = { "kernel32", "wsock32", "SocketW", "pthreadVC2", "WSOCK32"}
else
	package.includepaths = { "SocketW/src/"}
	package.links = { "SocketW", "pthread"}
end

