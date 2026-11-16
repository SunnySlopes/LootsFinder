#include <cstdint>

struct Generator;

extern "C" {

Generator *geode_biomes_create(uint64_t) { return nullptr; }
void geode_biomes_free(Generator *) {}
int geode_biomes_allowed(Generator *, int, int, int) { return 1; }

}
