#include "engine.hpp"
#include "utils/memory.hpp"

const int MAX_TRIANGLES = 1024;

// shared memory
int SIZE_FUNC_RETURN = 100;

const int INT_SIZE = 4, FLOAT_SIZE = 4, MATRIX_SIZE = 16 * FLOAT_SIZE;
const int VALUES_PER_VERTEX = 3;
const int VALUES_PER_COLOR = 4;
const int VALUES_PER_TEXCOORD = 2;
const int COLOR_BYTES_PER_TRIANGLE = VALUES_PER_COLOR * VALUES_PER_VERTEX * 4;
const int BYTES_PER_VERTEX = VALUES_PER_VERTEX * 4;

const int SIZE_RENDER_COLOR_BUFFER = MAX_TRIANGLES * COLOR_BYTES_PER_TRIANGLE;
const int SIZE_RENDER_VERTEX_BUFFER = MAX_TRIANGLES * BYTES_PER_VERTEX;
const int SIZE_RENDER_INDEX_BUFFER = MAX_TRIANGLES * BYTES_PER_VERTEX; //usually, it's less
const int SIZE_RENDER_TEXCOORDS_BUFFER = MAX_TRIANGLES * BYTES_PER_VERTEX / 2;

const int SIZE_PROJECTION_MATRIX = MATRIX_SIZE;
const int MAX_VIEW_MATRIX_COUNT = 20;
const int MAX_MODEL_MATRIX_COUNT = 20;
const int SIZE_VIEW_MATRICES = (MAX_VIEW_MATRIX_COUNT + 1) * MATRIX_SIZE; //stack of 20 view matrixes besides the 21th final matrix (on 0th index)
const int SIZE_MODEL_MATRICES = (MAX_MODEL_MATRIX_COUNT + 1) * MATRIX_SIZE; //stack of 20 view matrixes besides the 21th final matrix (on 0th index)

int HEAP_START;
int OFFSET_FUNC_RETURN;
int OFFSET_RENDER_COLOR_BUFFER;
int OFFSET_RENDER_VERTEX_BUFFER;
int OFFSET_RENDER_INDEX_BUFFER;
int OFFSET_RENDER_TEXCOORDS_BUFFER;
int OFFSET_PROJECTION_MATRIX;
int OFFSET_VIEW_MATRIX;
int OFFSET_MODEL_MATRIX;

// internal memory
int OFFSET_CURRENT_COLOR = 0;
const int SIZE_CURRENT_COLOR = 16 * sizeof(float);

// state
int vertexCount = 0;
int indexCount = 0;
int currentTextureId = 0;
int currentViewMatrixIndex = MAX_VIEW_MATRIX_COUNT;
int currentModelMatrixIndex = MAX_MODEL_MATRIX_COUNT;

int preinit(int heapStart) {
  HEAP_START = heapStart;
  OFFSET_FUNC_RETURN = HEAP_START;
  return OFFSET_FUNC_RETURN;
}

// set ups memory layout
void initEngine() {
  int *ret = (int *)OFFSET_FUNC_RETURN;

  OFFSET_RENDER_COLOR_BUFFER = OFFSET_FUNC_RETURN + SIZE_FUNC_RETURN;
  OFFSET_RENDER_VERTEX_BUFFER = OFFSET_RENDER_COLOR_BUFFER + SIZE_RENDER_COLOR_BUFFER;
  OFFSET_RENDER_INDEX_BUFFER = OFFSET_RENDER_VERTEX_BUFFER + SIZE_RENDER_VERTEX_BUFFER;
  OFFSET_RENDER_TEXCOORDS_BUFFER = OFFSET_RENDER_INDEX_BUFFER + SIZE_RENDER_INDEX_BUFFER;
  OFFSET_PROJECTION_MATRIX = OFFSET_RENDER_TEXCOORDS_BUFFER + SIZE_RENDER_TEXCOORDS_BUFFER;
  OFFSET_VIEW_MATRIX = OFFSET_PROJECTION_MATRIX + SIZE_PROJECTION_MATRIX;
  OFFSET_MODEL_MATRIX = OFFSET_VIEW_MATRIX + SIZE_VIEW_MATRICES;

  const int OFFSET_SHARED_MEMORY_END = OFFSET_MODEL_MATRIX + SIZE_MODEL_MATRICES;

  // internal memory
  OFFSET_CURRENT_COLOR = OFFSET_SHARED_MEMORY_END;
  const int OFFSET_DYNAMIC_MEMORY = align(OFFSET_CURRENT_COLOR + SIZE_CURRENT_COLOR, 16);

  ret[0] = VALUES_PER_COLOR;
  ret[1] = VALUES_PER_VERTEX;
  ret[2] = SIZE_RENDER_COLOR_BUFFER;
  ret[3] = SIZE_RENDER_VERTEX_BUFFER;
  ret[4] = SIZE_RENDER_INDEX_BUFFER;
  ret[5] = SIZE_RENDER_TEXCOORDS_BUFFER;
  ret[6] = OFFSET_RENDER_COLOR_BUFFER;
  ret[7] = OFFSET_RENDER_VERTEX_BUFFER;
  ret[8] = OFFSET_RENDER_INDEX_BUFFER;
  ret[9] = OFFSET_RENDER_TEXCOORDS_BUFFER;
  ret[10] = OFFSET_PROJECTION_MATRIX;
  ret[11] = OFFSET_VIEW_MATRIX;
  ret[12] = OFFSET_MODEL_MATRIX;
  ret[13] = OFFSET_DYNAMIC_MEMORY;
}

void beginFrame() {
  vertexCount = 0;
  indexCount = 0;
  currentTextureId = -1;
  currentViewMatrixIndex = MAX_VIEW_MATRIX_COUNT;
  currentModelMatrixIndex = MAX_MODEL_MATRIX_COUNT;
  mat4_identity(getViewMatrix());
  mat4_identity(getModelMatrix());
}

void endFrame() {
  if (vertexCount > 0)
    flushBuffers();
}

void flushBuffers() {
  if (indexCount == 0)
    return;

  int *ret = (int *)OFFSET_FUNC_RETURN;

  ret[0] = vertexCount;
  ret[1] = indexCount;
  ret[2] = currentTextureId;

  memcpy(
      (void *)(OFFSET_VIEW_MATRIX + currentViewMatrixIndex * MATRIX_SIZE),
      (void *)OFFSET_VIEW_MATRIX,
      MATRIX_SIZE);

   memcpy(
      (void *)(OFFSET_MODEL_MATRIX + currentModelMatrixIndex * MATRIX_SIZE),
      (void *)OFFSET_MODEL_MATRIX,
      MATRIX_SIZE);

  triggerDrawCall();

  currentTextureId = -1;
  vertexCount = 0;
  indexCount = 0;
}

void setTexture(int newTextureId) {
  if (currentTextureId != newTextureId) {
    flushBuffers();

    currentTextureId = newTextureId;
  }
}

// set 3 colors for 3 triangle vertices
void setColors3(float alpha, float r1, float g1, float b1, float r2, float g2, float b2, float r3, float g3, float b3) {
  float *color = (float *)OFFSET_CURRENT_COLOR;

  color[0] = r1;
  color[1] = g1;
  color[2] = b1;
  color[3] = alpha;
  color[4] = r2;
  color[5] = g2;
  color[6] = b2;
  color[7] = alpha;
  color[8] = r3;
  color[9] = g3;
  color[10] = b3;
  color[11] = alpha;
}

void setColors4(
    float alpha,
    float r1, float g1, float b1,
    float r2, float g2, float b2,
    float r3, float g3, float b3,
    float r4, float g4, float b4) {
  float *color = (float *)OFFSET_CURRENT_COLOR;
  setColors3(alpha, r1, g1, b1, r2, g2, b2, r3, g3, b3);

  color[12] = r4;
  color[13] = g4;
  color[14] = b4;
  color[15] = alpha;
}

// set single color for 3 triangle vertices
void setColor(float alpha, float r, float g, float b) {
  float *color = (float *)OFFSET_CURRENT_COLOR;

  int i = 0;
  for (i = 0; i < 16; i += 4) {
    color[i] = r;
    color[i + 1] = g;
    color[i + 2] = b;
    color[i + 3] = alpha;
  }
}

// for rect(), usually
void setColorLeftToRight(
    float alpha,
    float r1, float g1, float b1,
    float r2, float g2, float b2) {
  setColors4(
      alpha,
      r1, g1, b1,
      r1, g1, b1,
      r2, g2, b2,
      r2, g2, b2);
}

void set16f(
    float *a,
    float f0, float f1, float f2, float f3,
    float f4, float f5, float f6, float f7,
    float f8, float f9, float f10, float f11,
    float f12, float f13, float f14, float f15) {
  a[0] = f0;
  a[1] = f1;
  a[2] = f2;
  a[3] = f3;
  a[4] = f4;
  a[5] = f5;
  a[6] = f6;
  a[7] = f7;
  a[8] = f8;
  a[9] = f9;
  a[10] = f10;
  a[11] = f11;
  a[12] = f12;
  a[13] = f13;
  a[14] = f14;
  a[15] = f15;
}

float* getProjectionMatrix() {
  return (float *)OFFSET_PROJECTION_MATRIX;
}

float *getViewMatrix() {
  return (float *)(OFFSET_VIEW_MATRIX + currentViewMatrixIndex * MATRIX_SIZE);
}

float *getModelMatrix() {
  return (float *)(OFFSET_MODEL_MATRIX + currentModelMatrixIndex * MATRIX_SIZE);
}

void setProjectionMatrix(float* matrix) {
  flushBuffers();

  float *dst = getProjectionMatrix();
  memcpy(matrix, dst, MATRIX_SIZE);
}

void setViewMatrix(float *matrix) {
  flushBuffers();

  float *dst = getViewMatrix();
  memcpy(matrix, dst, MATRIX_SIZE);
}

void setModelMatrix(float *matrix) {
  flushBuffers();

  float *dst = getModelMatrix();
  memcpy(matrix, dst, MATRIX_SIZE);
}

float *pushViewMatrix() {
  float *src = getViewMatrix();
  currentViewMatrixIndex -= 1;
  float *dst = getViewMatrix();
  memcpy(src, dst, MATRIX_SIZE);
  return dst;
}

void popViewMatrix() {
  currentViewMatrixIndex += 1;
}

float *pushModelMatrix() {
  float *src = getModelMatrix();
  currentModelMatrixIndex -= 1;
  float *dst = getModelMatrix();
  memcpy(src, dst, MATRIX_SIZE);
  return dst;
}

void popModelMatrix() {
  currentModelMatrixIndex += 1;
}


void triangle(
    float v1x, float v1y, float v1z,
    float v2x, float v2y, float v2z,
    float v3x, float v3y, float v3z) {
  setTexture(-1);

  float *vertices = (float *)OFFSET_RENDER_VERTEX_BUFFER;
  float *colors = (float *)OFFSET_RENDER_COLOR_BUFFER;
  int *indices = (int *)OFFSET_RENDER_INDEX_BUFFER;
  float *currentColor = (float *)OFFSET_CURRENT_COLOR;

  int i = vertexCount * VALUES_PER_VERTEX;
  vertices[i] = v1x;
  vertices[i + 1] = v1y;
  vertices[i + 2] = v1z;
  vertices[i + 3] = v2x;
  vertices[i + 4] = v2y;
  vertices[i + 5] = v2z;
  vertices[i + 6] = v3x;
  vertices[i + 7] = v3y;
  vertices[i + 8] = v3z;

  i = vertexCount * VALUES_PER_COLOR;
  int j = 0;
  for (j = 0; j < 12; j += 1) {
    colors[i + j] = currentColor[j];
  }

  indices[indexCount] = vertexCount;
  indices[indexCount + 1] = vertexCount + 1;
  indices[indexCount + 2] = vertexCount + 2;

  vertexCount += 3;
  indexCount += 3;
}

void texTriangle(
    int textureId,
    float v1x, float v1y, float v1z, float u1, float v1,
    float v2x, float v2y, float v2z, float u2, float v2,
    float v3x, float v3y, float v3z, float u3, float v3) {
  setTexture(textureId);

  float *vertices = (float *)OFFSET_RENDER_VERTEX_BUFFER;
  float *texCoords = (float *)OFFSET_RENDER_TEXCOORDS_BUFFER;
  int *indices = (int *)OFFSET_RENDER_INDEX_BUFFER;

  int i = vertexCount * VALUES_PER_VERTEX;
  vertices[i] = v1x;
  vertices[i + 1] = v1y;
  vertices[i + 2] = v1z;
  vertices[i + 3] = v2x;
  vertices[i + 4] = v2y;
  vertices[i + 5] = v2z;
  vertices[i + 6] = v3x;
  vertices[i + 7] = v3y;
  vertices[i + 8] = v3z;

  i = vertexCount * VALUES_PER_TEXCOORD;
  texCoords[i] = u1;
  texCoords[i + 1] = v1;
  texCoords[i + 2] = u2;
  texCoords[i + 3] = v2;
  texCoords[i + 4] = u3;
  texCoords[i + 5] = v3;

  indices[indexCount] = vertexCount;
  indices[indexCount + 1] = vertexCount + 1;
  indices[indexCount + 2] = vertexCount + 2;

  vertexCount += 3;
  indexCount += 3;
}

void quad(
    float v1x, float v1y, float v1z,
    float v2x, float v2y, float v2z,
    float v3x, float v3y, float v3z,
    float v4x, float v4y, float v4z) {
  //1,2,3
  triangle(v1x, v1y, v1z, v2x, v2y, v2z, v3x, v3y, v3z);

  //3,4,1
  float *vertices = (float *)OFFSET_RENDER_VERTEX_BUFFER;
  float *colors = (float *)OFFSET_RENDER_COLOR_BUFFER;
  int *indices = (int *)OFFSET_RENDER_INDEX_BUFFER;
  float *currentColor = (float *)OFFSET_CURRENT_COLOR;

  int i = vertexCount * VALUES_PER_VERTEX;
  vertices[i] = v4x;
  vertices[i + 1] = v4y;
  vertices[i + 2] = v4z;

  i = vertexCount * VALUES_PER_COLOR;
  colors[i] = currentColor[12];
  colors[i + 1] = currentColor[13];
  colors[i + 2] = currentColor[14];
  colors[i + 3] = currentColor[15];

  indices[indexCount] = vertexCount - 1;
  indices[indexCount + 1] = vertexCount;
  indices[indexCount + 2] = vertexCount - 3;

  vertexCount += 1;
  indexCount += 3;
}

void texQuad(
    int textureId,
    float v1x, float v1y, float v1z, float u1, float v1,
    float v2x, float v2y, float v2z, float u2, float v2,
    float v3x, float v3y, float v3z, float u3, float v3,
    float v4x, float v4y, float v4z, float u4, float v4) {
  //1,2,3
  texTriangle(textureId,
              v1x, v1y, v1z, u1, v1,
              v2x, v2y, v2z, u2, v2,
              v3x, v3y, v3z, u3, v3);

  //3,4,1
  float *vertices = (float *)OFFSET_RENDER_VERTEX_BUFFER;
  float *texCoords = (float *)OFFSET_RENDER_TEXCOORDS_BUFFER;
  int *indices = (int *)OFFSET_RENDER_INDEX_BUFFER;

  int i = vertexCount * VALUES_PER_VERTEX;
  vertices[i] = v4x;
  vertices[i + 1] = v4y;
  vertices[i + 2] = v4z;

  i = vertexCount * VALUES_PER_TEXCOORD;
  texCoords[i] = u4;
  texCoords[i + 1] = v4;

  indices[indexCount] = vertexCount - 1;
  indices[indexCount + 1] = vertexCount;
  indices[indexCount + 2] = vertexCount - 3;

  vertexCount += 1;
  indexCount += 3;
}

int align(int x, int by) {
  int rest = x % by;
  return rest > 0
             ? x + by - rest
             : x;
}

int rgba(int r, int g, int b, int a) {
  return (a << 24) | (b << 16) | (g << 8) | r;
}

int rgb(int r, int g, int b) {
  return rgba(r, g, b, 255);
}

void rect(float x, float y, float z, float width, float height) {
  setTexture(-1);
  quad(x, y, z, x, y + height, z, x + width, y + height, z, x + width, y, z);
}

void texRect(int textureId, float x, float y, float z, float width, float height, float uLen, float vLen) {
  texQuad(
      textureId,
      x, y, z, 0, 0,
      x, y + height, z, 0, vLen,
      x + width, y + height, z, uLen, vLen,
      x + width, y, z, uLen, 0);
}