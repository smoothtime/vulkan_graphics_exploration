#if !defined(MATH_TYPES_H)
#include <glm/glm.hpp>

// uv's interspersed to align GPU reads
struct Vertex
{
	glm::vec3 position;
	real32 uvX;
	glm::vec3 normal;
	real32 uvY;
	glm::vec4 color;
};
#define MATH_TYPES_H
#endif