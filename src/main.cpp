#include <fmt/core.h>
#include <glm/ext/vector_float3.hpp>
#include <glm/glm.hpp>


int main() 
{
  auto newVec = glm::vec3(1, 2, 3);
  fmt::print("Hello, World!\n");
  fmt::print("{} {} {}\n", newVec.x, newVec.y, newVec.z);
  return 0;
}