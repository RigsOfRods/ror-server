
COLOUR_BLACK    = "#000000"
COLOUR_GREY     = "#999999"
COLOUR_RED      = "#FF0000"
COLOUR_BLUE   = "#FFFF00"
COLOUR_WHITE    = "#FFFFFF"
COLOUR_CYAN     = "#00FFFF"
COLOUR_BLUE     = "#0000FF"
COLOUR_GREEN    = "#00FF00"
COLOUR_MAGENTA  = "#FF00FF"
COLOUR_COMMAND  = "#941e8d"
COLOUR_NORMAL   = "#000000"
COLOUR_WHISPER  = "#967417"
COLOUR_SCRIPT   = "#32436f"

playerColours = [
	"#00CC00",
	"#0066B3",
	"#FF8000",
	"#FFCC00",
	"#330099",
	"#990099",
	"#CCFF00",
	"#FF0000",
	"#808080",
	"#008F00",
	"#00487D",
	"#B35A00",
	"#B38F00",
	"#6B006B",
	"#8FB300",
	"#B30000",
	"#BEBEBE",
	"#80FF80",
	"#80C9FF",
	"#FFC080",
	"#FFE680",
	"#AA80FF",
	"#EE00CC",
	"#FF8080",
	"#666600",
	"#FFBFFF",
	"#00FFCC",
	"#CC6699",
	"#999900"
];
	
class vector3:
	def __init__(self, x = 0.0, y = 0.0, z = 0.0):
		self.x = x
		self.y = y
		self.z = z	

class vector4:
	def __init__(self, x = 0.0, y = 0.0, z = 0.0, w = 0.0):
		self.x = x
		self.y = y
		self.z = z
		self.w = w
		
def isPointIn2DSquare(p, s):
	ABP = triangleAreaDouble(s[0], s[1], p)
	BCP = triangleAreaDouble(s[1], s[2], p)
	CDP = triangleAreaDouble(s[2], s[3], p)
	DAP = triangleAreaDouble(s[3], s[0], p)
	return ( ABP >= 0 and BCP >= 0 and CDP >= 0 and DAP >= 0 ) or ( ABP < 0 and BCP < 0 and CDP < 0 and DAP < 0 )

def triangleAreaDouble(a, b, c):
	return (c.x*b.y-b.x*c.y) - (c.x*a.y-a.x*c.y) + (b.x*a.y-a.x*b.y)
	
def squaredLengthBetween2Points(a, b):
	return ((a.x-b.x)**2 + (a.y-b.y)**2 + (a.z-b.z)**2)