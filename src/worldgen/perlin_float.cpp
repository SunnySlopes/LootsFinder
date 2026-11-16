#include <cmath>
#include <cstdint>

extern "C" {

static inline float gradientFloat(uint8_t index, float a, float b, float c)
{
    static const float g[16][3] = {
        {1,1,0}, {-1,1,0}, {1,-1,0}, {-1,-1,0},
        {1,0,1}, {-1,0,1}, {1,0,-1}, {-1,0,-1},
        {0,1,1}, {0,-1,1}, {0,1,-1}, {0,-1,-1},
        {1,1,0}, {0,-1,1}, {-1,1,0}, {0,-1,-1}
    };
    const float *v = g[index & 15];
    return (v[0]*a + v[1]*b) + v[2]*c;
}

static inline float lerpFloat(float t, float a, float b)
{
    return a + t * (b - a);
}

static inline double wrapFloatNoise(double x)
{
    return x - floor(x / 33554432.0 + 0.5) * 33554432.0;
}

float samplePerlinFloatRaw(const uint8_t permutation[256], double ox, double oy, double oz,
                           double x, double y, double z, double smear)
{
    double inputY = y;
    x = wrapFloatNoise(x) + ox;
    y = wrapFloatNoise(y) + oy;
    z = wrapFloatNoise(z) + oz;
    int ix = (int)floor(x), iy = (int)floor(y), iz = (int)floor(z);
    float dx = (float)(x - ix), dy = (float)(y - iy), dz = (float)(z - iz);
    float tx = dx*dx*dx * (dx * (dx*6.0F - 15.0F) + 10.0F);
    float ty = dy*dy*dy * (dy * (dy*6.0F - 15.0F) + 10.0F);
    float tz = dz*dz*dz * (dz * (dz*6.0F - 15.0F) + 10.0F);
    if (smear != 0) {
        double relative = y - iy;
        double limit = inputY >= 0 && inputY < relative ? inputY : relative;
        dy = (float)(relative - floor(limit / smear + 1.0e-7) * smear);
    }
    float corners[2][2][2];
    for (int a = 0; a < 2; a++)
        for (int b = 0; b < 2; b++)
            for (int c = 0; c < 2; c++) {
                int h = permutation[(permutation[(permutation[(ix+a)&255]+iy+b)&255]+iz+c)&255];
                corners[a][b][c] = gradientFloat(h, dx-a, dy-b, dz-c);
            }
    return lerpFloat(tz,
        lerpFloat(ty, lerpFloat(tx, corners[0][0][0], corners[1][0][0]),
                      lerpFloat(tx, corners[0][1][0], corners[1][1][0])),
        lerpFloat(ty, lerpFloat(tx, corners[0][0][1], corners[1][0][1]),
                      lerpFloat(tx, corners[0][1][1], corners[1][1][1])));
}

}
