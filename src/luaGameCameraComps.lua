require "math"

Camera = --Camera components for boundings of map
{
	name = "CameraOrtho",
	angle = math.pi / 2.0,
	aspect = 16.0 / 9.0,
	z_near = 0.1,
	z_far = 100.0,
	left = -10.0,
	right = 10.0,
	top = 6.5,
	bot = -6.5,
	eyePos = -20,
}