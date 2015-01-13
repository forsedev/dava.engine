attribute vec4 inPosition;
attribute vec2 inTexCoord0;

varying vec2 varTexCoord0;

uniform mat4 projectionMatrix;

void main()
{
	varTexCoord0 = inTexCoord0;
	gl_Position = projectionMatrix * inPosition;
}