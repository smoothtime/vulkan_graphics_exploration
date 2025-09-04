#if !defined(MATH_TYPES_H)
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

// uv's interspersed to align GPU reads
// worth considering if this is better or if we want to struct of arrays this
// when parsing from file
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