// MIT License

// Copyright (c) 2019 wi-re
// Copyright 2023 The Manifold Authors.

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Modified from https://github.com/wi-re/tbtSVD, removing CUDA dependence and
// approximate inverse square roots.

#include <cmath>

#include "manifold/common.h"

namespace {
using manifold::mat3;
using manifold::vec3;
using manifold::vec4;

// Constants used for calculation of Givens quaternions
inline constexpr double _gamma = 5.82842712474619;    // sqrt(8)+3;
inline constexpr double _cStar = 0.9238795325112867;  // cos(pi/8)
inline constexpr double _sStar = 0.3826834323650898;  // sin(pi/8)
// Threshold value
inline constexpr double _SVD_EPSILON = 1e-6;
// Iteration counts for Jacobi Eigen Analysis, influences precision
inline constexpr int JACOBI_STEPS = 12;

// Helper function used to swap X with Y and Y with  X if c == true
inline void CondSwap(bool c, double& X, double& Y) {
  double Z = X;
  X = c ? Y : X;
  Y = c ? Z : Y;
}
// Helper function used to swap X with Y and Y with -X if c == true
inline void CondNegSwap(bool c, double& X, double& Y) {
  double Z = -X;
  X = c ? Y : X;
  Y = c ? Z : Y;
}
// A simple symmetric 3x3 Matrix class (contains no storage for (0, 1) (0, 2)
// and (1, 2)
struct Symmetric3x3 {
  double m_00 = 1.0;
  double m_10 = 0.0, m_11 = 1.0;
  double m_20 = 0.0, m_21 = 0.0, m_22 = 1.0;

  Symmetric3x3(double a11 = 1.0, double a21 = 0.0, double a22 = 1.0,
               double a31 = 0.0, double a32 = 0.0, double a33 = 1.0)
      : m_00(a11), m_10(a21), m_11(a22), m_20(a31), m_21(a32), m_22(a33) {}
  Symmetric3x3(mat3 o)
      : m_00(o[0][0]),
        m_10(o[0][1]),
        m_11(o[1][1]),
        m_20(o[0][2]),
        m_21(o[1][2]),
        m_22(o[2][2]) {}
};
// Helper struct to store 2 doubles to avoid OUT parameters on functions
struct Givens {
  double ch = _cStar;
  double sh = _sStar;
};
// Helper struct to store 2 Matrices to avoid OUT parameters on functions
struct QR {
  mat3 Q, R;
};
// Calculates the squared norm of the vector.
inline double Dist2(vec3 v) { return la::dot(v, v); }
// For an explanation of the math see
// http://pages.cs.wisc.edu/~sifakis/papers/SVD_TR1690.pdf Computing the
// Singular Value Decomposition of 3 x 3 matrices with minimal branching and
// elementary floating point operations See Algorithm 2 in reference. Given a
// matrix A this function returns the Givens quaternion (x and w component, y
// and z are 0)
inline Givens ApproximateGivensQuaternion(Symmetric3x3& A) {
  Givens g{2.0 * (A.m_00 - A.m_11), A.m_10};
  bool b = _gamma * g.sh * g.sh < g.ch * g.ch;
  double w = 1.0 / hypot(g.ch, g.sh);
  if (!std::isfinite(w)) b = 0;
  return Givens{b ? w * g.ch : _cStar, b ? w * g.sh : _sStar};
}
// Function used to apply a Givens rotation S. Calculates the weights and
// updates the quaternion to contain the cumulative rotation
inline void JacobiConjugation(const int32_t x, const int32_t y, const int32_t z,
                              Symmetric3x3& S, vec4& q) {
  auto g = ApproximateGivensQuaternion(S);
  double scale = 1.0 / fma(g.ch, g.ch, g.sh * g.sh);
  double a = fma(g.ch, g.ch, -g.sh * g.sh) * scale;
  double b = 2.0 * g.sh * g.ch * scale;
  Symmetric3x3 _S = S;
  // perform conjugation S = Q'*S*Q
  S.m_00 =
      fma(a, fma(a, _S.m_00, b * _S.m_10), b * (fma(a, _S.m_10, b * _S.m_11)));
  S.m_10 = fma(a, fma(-b, _S.m_00, a * _S.m_10),
               b * (fma(-b, _S.m_10, a * _S.m_11)));
  S.m_11 = fma(-b, fma(-b, _S.m_00, a * _S.m_10),
               a * (fma(-b, _S.m_10, a * _S.m_11)));
  S.m_20 = fma(a, _S.m_20, b * _S.m_21);
  S.m_21 = fma(-b, _S.m_20, a * _S.m_21);
  S.m_22 = _S.m_22;
  // update cumulative rotation qV
  vec3 tmp = g.sh * vec3(q);
  g.sh *= q[3];
  // (x,y,z) corresponds to ((0,1,2),(1,2,0),(2,0,1)) for (p,q) =
  // ((0,1),(1,2),(0,2))
  q[z] = fma(q[z], g.ch, g.sh);
  q[3] = fma(q[3], g.ch, -tmp[z]);  // w
  q[x] = fma(q[x], g.ch, tmp[y]);
  q[y] = fma(q[y], g.ch, -tmp[x]);
  // re-arrange matrix for next iteration
  _S.m_00 = S.m_11;
  _S.m_10 = S.m_21;
  _S.m_11 = S.m_22;
  _S.m_20 = S.m_10;
  _S.m_21 = S.m_20;
  _S.m_22 = S.m_00;
  S.m_00 = _S.m_00;
  S.m_10 = _S.m_10;
  S.m_11 = _S.m_11;
  S.m_20 = _S.m_20;
  S.m_21 = _S.m_21;
  S.m_22 = _S.m_22;
}
// Function used to contain the Givens permutations and the loop of the jacobi
// steps controlled by JACOBI_STEPS Returns the quaternion q containing the
// cumulative result used to reconstruct S
inline mat3 JacobiEigenAnalysis(Symmetric3x3 S) {
  vec4 q(0, 0, 0, 1);
  for (int32_t i = 0; i < JACOBI_STEPS; i++) {
    JacobiConjugation(0, 1, 2, S, q);
    JacobiConjugation(1, 2, 0, S, q);
    JacobiConjugation(2, 0, 1, S, q);
  }
  return mat3({1.0 - 2.0 * (fma(q.y, q.y, q.z * q.z)),  //
               2.0 * fma(q.x, q.y, +q.w * q.z),         //
               2.0 * fma(q.x, q.z, -q.w * q.y)},        //
              {2 * fma(q.x, q.y, -q.w * q.z),           //
               1 - 2 * fma(q.x, q.x, q.z * q.z),        //
               2 * fma(q.y, q.z, q.w * q.x)},           //
              {2 * fma(q.x, q.z, q.w * q.y),            //
               2 * fma(q.y, q.z, -q.w * q.x),           //
               1 - 2 * fma(q.x, q.x, q.y * q.y)});
}
// Implementation of Algorithm 3
inline void SortSingularValues(mat3& B, mat3& V) {
  double rho1 = Dist2(B[0]);
  double rho2 = Dist2(B[1]);
  double rho3 = Dist2(B[2]);
  bool c;
  c = rho1 < rho2;
  CondNegSwap(c, B[0][0], B[1][0]);
  CondNegSwap(c, V[0][0], V[1][0]);
  CondNegSwap(c, B[0][1], B[1][1]);
  CondNegSwap(c, V[0][1], V[1][1]);
  CondNegSwap(c, B[0][2], B[1][2]);
  CondNegSwap(c, V[0][2], V[1][2]);
  CondSwap(c, rho1, rho2);
  c = rho1 < rho3;
  CondNegSwap(c, B[0][0], B[2][0]);
  CondNegSwap(c, V[0][0], V[2][0]);
  CondNegSwap(c, B[0][1], B[2][1]);
  CondNegSwap(c, V[0][1], V[2][1]);
  CondNegSwap(c, B[0][2], B[2][2]);
  CondNegSwap(c, V[0][2], V[2][2]);
  CondSwap(c, rho1, rho3);
  c = rho2 < rho3;
  CondNegSwap(c, B[1][0], B[2][0]);
  CondNegSwap(c, V[1][0], V[2][0]);
  CondNegSwap(c, B[1][1], B[2][1]);
  CondNegSwap(c, V[1][1], V[2][1]);
  CondNegSwap(c, B[1][2], B[2][2]);
  CondNegSwap(c, V[1][2], V[2][2]);
}
// Implementation of Algorithm 4
inline Givens QRGivensQuaternion(double a1, double a2) {
  // a1 = pivot point on diagonal
  // a2 = lower triangular entry we want to annihilate
  double epsilon = _SVD_EPSILON;
  double rho = hypot(a1, a2);
  Givens g{fabs(a1) + fmax(rho, epsilon), rho > epsilon ? a2 : 0};
  bool b = a1 < 0.0;
  CondSwap(b, g.sh, g.ch);
  double w = 1.0 / hypot(g.ch, g.sh);
  g.ch *= w;
  g.sh *= w;
  return g;
}
// Implements a QR decomposition of a Matrix, see Sec 4.2
inline QR QRDecomposition(mat3& B) {
  mat3 Q, R;
  // first Givens rotation (ch,0,0,sh)
  auto g1 = QRGivensQuaternion(B[0][0], B[0][1]);
  auto a = fma(-2.0, g1.sh * g1.sh, 1.0);
  auto b = 2.0 * g1.ch * g1.sh;
  // apply B = Q' * B
  R[0][0] = fma(a, B[0][0], b * B[0][1]);
  R[1][0] = fma(a, B[1][0], b * B[1][1]);
  R[2][0] = fma(a, B[2][0], b * B[2][1]);
  R[0][1] = fma(-b, B[0][0], a * B[0][1]);
  R[1][1] = fma(-b, B[1][0], a * B[1][1]);
  R[2][1] = fma(-b, B[2][0], a * B[2][1]);
  R[0][2] = B[0][2];
  R[1][2] = B[1][2];
  R[2][2] = B[2][2];
  // second Givens rotation (ch,0,-sh,0)
  auto g2 = QRGivensQuaternion(R[0][0], R[0][2]);
  a = fma(-2.0, g2.sh * g2.sh, 1.0);
  b = 2.0 * g2.ch * g2.sh;
  // apply B = Q' * B;
  B[0][0] = fma(a, R[0][0], b * R[0][2]);
  B[1][0] = fma(a, R[1][0], b * R[1][2]);
  B[2][0] = fma(a, R[2][0], b * R[2][2]);
  B[0][1] = R[0][1];
  B[1][1] = R[1][1];
  B[2][1] = R[2][1];
  B[0][2] = fma(-b, R[0][0], a * R[0][2]);
  B[1][2] = fma(-b, R[1][0], a * R[1][2]);
  B[2][2] = fma(-b, R[2][0], a * R[2][2]);
  // third Givens rotation (ch,sh,0,0)
  auto g3 = QRGivensQuaternion(B[1][1], B[1][2]);
  a = fma(-2.0, g3.sh * g3.sh, 1.0);
  b = 2.0 * g3.ch * g3.sh;
  // R is now set to desired value
  R[0][0] = B[0][0];
  R[1][0] = B[1][0];
  R[2][0] = B[2][0];
  R[0][1] = fma(a, B[0][1], b * B[0][2]);
  R[1][1] = fma(a, B[1][1], b * B[1][2]);
  R[2][1] = fma(a, B[2][1], b * B[2][2]);
  R[0][2] = fma(-b, B[0][1], a * B[0][2]);
  R[1][2] = fma(-b, B[1][1], a * B[1][2]);
  R[2][2] = fma(-b, B[2][1], a * B[2][2]);
  // construct the cumulative rotation Q=Q1 * Q2 * Q3
  // the number of floating point operations for three quaternion
  // multiplications is more or less comparable to the explicit form of the
  // joined matrix. certainly more memory-efficient!
  auto sh12 = 2.0 * fma(g1.sh, g1.sh, -0.5);
  auto sh22 = 2.0 * fma(g2.sh, g2.sh, -0.5);
  auto sh32 = 2.0 * fma(g3.sh, g3.sh, -0.5);
  Q[0][0] = sh12 * sh22;
  Q[1][0] = fma(4.0 * g2.ch * g3.ch, sh12 * g2.sh * g3.sh,
                2.0 * g1.ch * g1.sh * sh32);
  Q[2][0] = fma(4.0 * g1.ch * g3.ch, g1.sh * g3.sh,
                -2.0 * g2.ch * sh12 * g2.sh * sh32);

  Q[0][1] = -2.0 * g1.ch * g1.sh * sh22;
  Q[1][1] =
      fma(-8.0 * g1.ch * g2.ch * g3.ch, g1.sh * g2.sh * g3.sh, sh12 * sh32);
  Q[2][1] = fma(
      -2.0 * g3.ch, g3.sh,
      4.0 * g1.sh * fma(g3.ch * g1.sh, g3.sh, g1.ch * g2.ch * g2.sh * sh32));

  Q[0][2] = 2.0 * g2.ch * g2.sh;
  Q[1][2] = -2.0 * g3.ch * sh22 * g3.sh;
  Q[2][2] = sh22 * sh32;
  return QR{Q, R};
}
}  // namespace

namespace manifold {

/**
 * The three matrices of a Singular Value Decomposition.
 */
struct SVDSet {
  mat3 U, S, V;
};

/**
 * Returns the Singular Value Decomposition of A: A = U * S * la::transpose(V).
 *
 * @param A The matrix to decompose.
 */
inline SVDSet SVD(mat3 A) {
  mat3 V = JacobiEigenAnalysis(la::transpose(A) * A);
  auto B = A * V;
  SortSingularValues(B, V);
  QR qr = QRDecomposition(B);
  return SVDSet{qr.Q, qr.R, V};
}

/**
 * Returns the largest singular value of A.
 *
 * @param A The matrix to measure.
 */
inline double SpectralNorm(mat3 A) {
  SVDSet usv = SVD(A);
  return usv.S[0][0];
}
}  // namespace manifold
