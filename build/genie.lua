PROJ_DIR = path.getabsolute("..")

function SetTarget( _configuration, _platform )
	--print(_configuration .. _platform)
	local platform = _platform
	local arch = _platform
	if _platform == "x32" then
		platform = "Win32"
		arch = "x86"
	end
	local target = string.format( "%s/bin/%s_%s/", PROJ_DIR, _configuration, platform ) 
	local obj = string.format( "%s/intermediate/%s_%s", PROJ_DIR, _configuration, platform ) 
	configuration {_configuration, _platform}
		targetdir( target )
		objdir( obj )
end

solution "fcheck_test"
	configurations { "Debug", "Release" }

	platforms { "x64", "native" }
 
	project "fcheck_test"
		kind "ConsoleApp"
		language "C++"
		
		files
		{
			path.join(PROJ_DIR, "fcheck.h"),
			path.join(PROJ_DIR, "test/*.h"),
			path.join(PROJ_DIR, "test/*.cpp"),
		}
		
		configuration "windows"
			SetTarget( "Debug", "x64" )
			SetTarget( "Release", "x64" )
			
		configuration "macosx"
			SetTarget( "Debug", "native" )
			SetTarget( "Release", "native" )
			
		configuration "Debug"
			defines { "_DEBUG" }
			flags { "Symbols" }
 
		configuration "Release"
			defines { "NDEBUG" }
			flags { "Optimize", "Symbols" }