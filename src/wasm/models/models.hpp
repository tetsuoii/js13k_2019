#ifndef MODELS__H
#define MODELS__H

#include "../common.hpp"

struct Model3d {
  float vertexAlignX;
  float vertexAlignY;
  float vertexAlignZ;
  float scale;
  void *vertices;
  void *faces3;
  void *faces4;
  ushort vertexCount;
  ushort face3Count;
  ushort face4Count;
};

Model3d &getModel_frog();
void renderModel3d(Model3d &model);

#endif