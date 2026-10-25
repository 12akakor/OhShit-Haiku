/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// Scalers, see video_scaler.h.
//
// XbrScaler::Scale() is a C++ port of Hyllian's xBR-lv2 shader
// (libretro glsl-shaders, xbr/shaders/xbr-lv2.glsl, MIT licence, see
// src/third_party/LICENSE.xbr.txt), with the default parameters of that
// shader: CORNER_C, SMOOTH_TIPS, XBR_SCALE 3 (edge width), EQ threshold 15,
// LV2 coefficient 2, small details off.
//
// The shader evaluates everything per output pixel. Here the edge rules,
// which only depend on the input pixel and its neighbours, are computed once
// per input pixel; the per-subpixel line inequations come from a table.
#include "VideoScaler.h"

#include <algorithm>
#include <chrono>
#include <math.h>
#include <string.h>
#include <thread>


namespace {

int sThreadLimit = 0;	// 0: use the number of cores (max 4)


int
WorkerThreads()
{
	unsigned int cores = std::thread::hardware_concurrency();
	int threads = cores == 0 ? 2 : static_cast<int>(std::min(cores, 4u));
	if (sThreadLimit > 0)
		threads = std::min(threads, sThreadLimit);
	return threads;
}


// Runs body(begin, end) over [0, count) on a few threads.
template<typename Body>
void
ParallelRows(int count, Body body)
{
	int threads = WorkerThreads();
	if (threads <= 1 || count < 64) {
		body(0, count);
		return;
	}
	std::thread workers[4];
	int chunk = (count + threads - 1) / threads;
	for (int t = 1; t < threads; t++) {
		int begin = t * chunk;
		int end = std::min(count, begin + chunk);
		if (begin < end)
			workers[t] = std::thread(body, begin, end);
	}
	body(0, std::min(count, chunk));
	for (int t = 1; t < threads; t++) {
		if (workers[t].joinable())
			workers[t].join();
	}
}


// Runs body(begin, end) over [0, count) items, using threads only when
// there are at least 'minimum' items.
template<typename Body>
void
ParallelItems(int count, int minimum, Body body)
{
	int threads = WorkerThreads();
	if (threads <= 1 || count < minimum) {
		body(0, count);
		return;
	}
	std::thread workers[4];
	int chunk = (count + threads - 1) / threads;
	for (int t = 1; t < threads; t++) {
		int begin = t * chunk;
		int end = std::min(count, begin + chunk);
		if (begin < end)
			workers[t] = std::thread(body, begin, end);
	}
	body(0, std::min(count, chunk));
	for (int t = 1; t < threads; t++) {
		if (workers[t].joinable())
			workers[t].join();
	}
}


inline void
ClearRows(uint32_t* dst, int dstWidth, int stride, int fromRow, int toRow)
{
	for (int y = fromRow; y < toRow; y++)
		memset(dst + static_cast<size_t>(y) * stride, 0,
			sizeof(uint32_t) * static_cast<size_t>(dstWidth));
}


// Black borders around the rectangle (rx, ry, rw, rh).
void
ClearBorders(uint32_t* dst, int dstWidth, int dstHeight, int stride, int rx,
	int ry, int rw, int rh)
{
	ClearRows(dst, dstWidth, stride, 0, ry);
	ClearRows(dst, dstWidth, stride, ry + rh, dstHeight);
	for (int y = ry; y < ry + rh; y++) {
		uint32_t* row = dst + static_cast<size_t>(y) * stride;
		memset(row, 0, sizeof(uint32_t) * static_cast<size_t>(rx));
		memset(row + rx + rw, 0,
			sizeof(uint32_t) * static_cast<size_t>(dstWidth - rx - rw));
	}
}


inline uint32_t
LerpPixel(uint32_t a, uint32_t b, int t /* 0..256 */)
{
	uint32_t rb = (((a & 0xff00ff) * (256 - t) + (b & 0xff00ff) * t) >> 8)
		& 0xff00ff;
	uint32_t g = (((a & 0x00ff00) * (256 - t) + (b & 0x00ff00) * t) >> 8)
		& 0x00ff00;
	return 0xff000000u | rb | g;
}


}	// namespace


void
ResampleMap::Build(int srcWidth, int srcHeight, int width, int height,
	bool sharp)
{
	fSrcWidth = srcWidth;
	fSrcHeight = srcHeight;
	fWidth = width;
	fHeight = height;
	fSharp = sharp;
	fColIndex.resize(width);
	fColWeight.resize(width);
	fRowIndex.resize(height);
	fRowWeight.resize(height);
	for (int pass = 0; pass < 2; pass++) {
		int count = pass == 0 ? width : height;
		int size = pass == 0 ? srcWidth : srcHeight;
		double scale = static_cast<double>(count) / size;
		std::vector<int>& index = pass == 0 ? fColIndex : fRowIndex;
		std::vector<int>& weight = pass == 0 ? fColWeight : fRowWeight;
		for (int i = 0; i < count; i++) {
			double texel = (i + 0.5) / scale;
			double pos;
			if (sharp && scale > 1.0) {
				// libretro sharp-bilinear-simple
				double floored = floor(texel);
				double s = texel - floored;
				double range = 0.5 - 0.5 / scale;
				double dist = s - 0.5;
				double clamped = std::min(std::max(dist, -range), range);
				pos = floored + (dist - clamped) * scale + 0.5;
			} else
				pos = texel;
			double q = pos - 0.5;
			int i0 = static_cast<int>(floor(q));
			double t = q - i0;
			if (i0 < 0) {
				i0 = 0;
				t = 0;
			}
			if (i0 >= size - 1) {
				i0 = size - 1;
				t = 0;
			}
			index[i] = i0;
			weight[i] = static_cast<int>(t * 256.0 + 0.5);
		}
	}
}


bool
ResampleMap::Matches(int srcWidth, int srcHeight, int width, int height,
	bool sharp) const
{
	return fSrcWidth == srcWidth && fSrcHeight == srcHeight && fWidth == width
		&& fHeight == height && fSharp == sharp;
}


void
ResampleMap::Apply(const uint32_t* src, uint32_t* dst, int stride, int x0,
	int x1, int y0, int y1) const
{
	const int sw = fSrcWidth;
	const int sh = fSrcHeight;
	for (int y = y0; y < y1; y++) {
		int sy = fRowIndex[y];
		int wy = fRowWeight[y];
		const uint32_t* r0 = src + static_cast<size_t>(sy) * sw;
		const uint32_t* r1 = sy + 1 < sh ? r0 + sw : r0;
		uint32_t* out = dst + static_cast<size_t>(y) * stride;
		for (int x = x0; x < x1; x++) {
			int sx = fColIndex[x];
			int wx = fColWeight[x];
			int sx1 = sx + 1 < sw ? sx + 1 : sx;
			uint32_t top = LerpPixel(r0[sx], r0[sx1], wx);
			uint32_t bottom = LerpPixel(r1[sx], r1[sx1], wx);
			out[x] = LerpPixel(top, bottom, wy);
		}
	}
}


namespace {

// Bilinear resample of src (sw x sh) into the rectangle at dst. 'sharp'
// makes it sharp bilinear: pixels keep a flat centre.
void
Resample(const uint32_t* src, int sw, int sh, uint32_t* dst, int stride,
	int rx, int ry, int rw, int rh, bool sharp)
{
	ResampleMap map;
	map.Build(sw, sh, rw, rh, sharp);
	uint32_t* origin = dst + static_cast<size_t>(ry) * stride + rx;
	ParallelRows(rh, [&](int begin, int end) {
		map.Apply(src, origin, stride, 0, rw, begin, end);
	});
}


// Copies every 'factor'-th pixel (undoes pixel doubling).
const uint32_t*
ArtImage(const uint32_t* src, int srcWidth, int srcHeight, int factor,
	std::vector<uint32_t>& buffer, int* width, int* height)
{
	if (factor < 2) {
		*width = srcWidth;
		*height = srcHeight;
		return src;
	}
	int w = srcWidth / factor;
	int h = srcHeight / factor;
	buffer.resize(static_cast<size_t>(w) * h);
	for (int y = 0; y < h; y++) {
		const uint32_t* in = src + static_cast<size_t>(y) * factor * srcWidth;
		uint32_t* out = buffer.data() + static_cast<size_t>(y) * w;
		for (int x = 0; x < w; x++)
			out[x] = in[x * factor];
	}
	*width = w;
	*height = h;
	return buffer.data();
}


// xBR-lv2 ------------------------------------------------------------------

const float kAo[4] = { 1.0f, -1.0f, -1.0f, 1.0f };
const float kBo[4] = { 1.0f, 1.0f, -1.0f, -1.0f };
const float kCo[4] = { 1.5f, 0.5f, -0.5f, 0.5f };
const float kAx[4] = { 1.0f, -1.0f, -1.0f, 1.0f };
const float kBx[4] = { 0.5f, 2.0f, -0.5f, -2.0f };
const float kCx[4] = { 1.0f, 1.0f, -0.5f, 0.0f };
const float kAy[4] = { 1.0f, -1.0f, -1.0f, 1.0f };
const float kBy[4] = { 2.0f, 0.5f, -2.0f, -0.5f };
const float kCy[4] = { 2.0f, 0.0f, -1.0f, 0.5f };
const float kCi = 0.25f;
const float kXbrScale = 3.0f;
const float kEqThreshold = 15.0f;
const float kLv2Coefficient = 2.0f;

typedef XbrWeights SubpixelWeights;


inline float
Clamp01(float value)
{
	return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}


void
ComputeSubpixelWeights(int factor, std::vector<SubpixelWeights>& table)
{
	table.resize(static_cast<size_t>(factor) * factor);
	const float delta = 1.0f / kXbrScale;
	const float deltaL[4] = { 0.5f / kXbrScale, 1.0f / kXbrScale,
		0.5f / kXbrScale, 1.0f / kXbrScale };
	const float deltaU[4] = { deltaL[1], deltaL[0], deltaL[3], deltaL[2] };
	for (int sy = 0; sy < factor; sy++) {
		for (int sx = 0; sx < factor; sx++) {
			float fpx = (sx + 0.5f) / factor;
			float fpy = (sy + 0.5f) / factor;
			SubpixelWeights& w = table[static_cast<size_t>(sy) * factor + sx];
			for (int k = 0; k < 4; k++) {
				float fx = kAo[k] * fpy + kBo[k] * fpx;
				float fxl = kAx[k] * fpy + kBx[k] * fpx;
				float fxu = kAy[k] * fpy + kBy[k] * fpx;
				w.f45i[k] = Clamp01((fx + delta - kCo[k] - kCi) / (2 * delta));
				w.f45[k] = Clamp01((fx + delta - kCo[k]) / (2 * delta));
				w.f30[k] = Clamp01((fxl + deltaL[k] - kCx[k]) / (2 * deltaL[k]));
				w.f60[k] = Clamp01((fxu + deltaU[k] - kCy[k]) / (2 * deltaU[k]));
			}
		}
	}
}


struct Rgb {
	float	r, g, b;
};


inline Rgb
ToRgb(uint32_t c)
{
	Rgb out = { static_cast<float>((c >> 16) & 0xff), static_cast<float>((c >> 8) & 0xff),
		static_cast<float>(c & 0xff) };
	return out;
}


inline Rgb
Mix(const Rgb& a, const Rgb& b, float t)
{
	Rgb out = { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
		a.b + (b.b - a.b) * t };
	return out;
}


inline float
ColorDistance(const Rgb& a, const Rgb& b)
{
	return fabsf(a.r - b.r) + fabsf(a.g - b.g) + fabsf(a.b - b.b);
}


inline uint32_t
ToPixel(const Rgb& c)
{
	int r = static_cast<int>(c.r + 0.5f);
	int g = static_cast<int>(c.g + 0.5f);
	int b = static_cast<int>(c.b + 0.5f);
	return 0xff000000u | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8)
		| static_cast<uint32_t>(b);
}


inline float
LumaOf(uint32_t c)
{
	// As in the shader: dot(rgb / 255, (14.352, 28.176, 5.472)).
	return ((c >> 16) & 0xff) / 255.0f * 14.352f
		+ ((c >> 8) & 0xff) / 255.0f * 28.176f
		+ (c & 0xff) / 255.0f * 5.472f;
}


// xBR-lv2 for the input pixels in [x0, x1) x [y0, y1). Each input pixel
// writes its own factor x factor block of out (width * factor wide).
void
XbrRect(const uint32_t* in, const float* luma, int width, int height,
	int factor, const SubpixelWeights* weights, uint32_t* out, int x0, int x1,
	int y0, int y1)
{
	const int outWidth = width * factor;
	for (int y = y0; y < y1; y++) {
		int ym2 = std::max(y - 2, 0), ym1 = std::max(y - 1, 0);
		int yp1 = std::min(y + 1, height - 1), yp2 = std::min(y + 2, height - 1);
		for (int x = x0; x < x1; x++) {
			int xm2 = std::max(x - 2, 0), xm1 = std::max(x - 1, 0);
			int xp1 = std::min(x + 1, width - 1);
			int xp2 = std::min(x + 2, width - 1);
#define L(px, py) luma[static_cast<size_t>(py) * width + (px)]
#define P(px, py) in[static_cast<size_t>(py) * width + (px)]
			float A1 = L(xm1, ym2), B1 = L(x, ym2), C1 = L(xp1, ym2);
			float A = L(xm1, ym1), B = L(x, ym1), C = L(xp1, ym1);
			float D = L(xm1, y), E = L(x, y), F = L(xp1, y);
			float G = L(xm1, yp1), H = L(x, yp1), I = L(xp1, yp1);
			float G5 = L(xm1, yp2), H5 = L(x, yp2), I5 = L(xp1, yp2);
			float A0 = L(xm2, ym1), D0 = L(xm2, y), G0 = L(xm2, yp1);
			float C4 = L(xp2, ym1), F4 = L(xp2, y), I4 = L(xp2, yp1);

			uint32_t* block = out + static_cast<size_t>(y) * factor * outWidth
				+ static_cast<size_t>(x) * factor;
			uint32_t centre = P(x, y);

			// The four rotations (bottom-right, top-right, top-left,
			// bottom-left), as the vec4 components of the shader.
			const float b[4] = { B, D, H, F };
			const float c[4] = { C, A, G, I };
			const float d[4] = { D, H, F, B };
			const float f[4] = { F, B, D, H };
			const float g[4] = { G, I, C, A };
			const float h[4] = { H, F, B, D };
			const float i[4] = { I, C, A, G };
			const float i4[4] = { I4, C1, A0, G5 };
			const float i5[4] = { I5, C4, A1, G0 };
			const float h5[4] = { H5, F4, B1, D0 };
			const float f4[4] = { F4, B1, D0, H5 };

			bool edri[4], edr[4], edrL[4], edrU[4], px[4];
			bool any = false;
			for (int k = 0; k < 4; k++) {
				auto df = [](float p, float q) { return fabsf(p - q); };
				auto eq = [&](float p, float q) { return df(p, q) <= kEqThreshold; };
				bool irlv0 = E != f[k] && E != h[k];
				edri[k] = edr[k] = edrL[k] = edrU[k] = false;
				px[k] = df(E, h[k]) >= df(E, f[k]);
				if (!irlv0)
					continue;
				// CORNER_C
				float irlv1 = static_cast<float>(!eq(f[k], b[k]) && !eq(f[k], c[k]))
					+ static_cast<float>(!eq(h[k], d[k]) && !eq(h[k], g[k]))
					+ static_cast<float>(eq(E, i[k]))
						* (static_cast<float>(!eq(f[k], f4[k]) && !eq(f[k], i4[k]))
							+ static_cast<float>(!eq(h[k], h5[k]) && !eq(h[k], i5[k])))
					+ static_cast<float>(eq(E, g[k])) + static_cast<float>(eq(E, c[k]));
				bool irlv2l = E != g[k] && d[k] != g[k];
				bool irlv2u = E != c[k] && b[k] != c[k];
				float wd1 = df(E, c[k]) + df(E, g[k]) + df(i[k], h5[k])
					+ df(i[k], f4[k]) + 4.0f * df(h[k], f[k]);
				float wd2 = df(h[k], d[k]) + df(h[k], i5[k]) + df(f[k], i4[k])
					+ df(f[k], b[k]) + 4.0f * df(E, i[k]);
				edri[k] = wd2 >= wd1;
				edr[k] = wd2 >= wd1 + 0.1f && irlv1 >= 0.5f;
				edrL[k] = edr[k] && irlv2l
					&& df(h[k], c[k]) >= kLv2Coefficient * df(f[k], g[k]);
				edrU[k] = edr[k] && irlv2u
					&& df(f[k], g[k]) >= kLv2Coefficient * df(h[k], c[k]);
				any = true;
			}

			if (!any) {
				for (int sy = 0; sy < factor; sy++) {
					uint32_t* row = block + static_cast<size_t>(sy) * outWidth;
					for (int sx = 0; sx < factor; sx++)
						row[sx] = centre;
				}
				continue;
			}

			Rgb cE = ToRgb(centre);
			Rgb cB = ToRgb(P(x, ym1)), cD = ToRgb(P(xm1, y));
			Rgb cF = ToRgb(P(xp1, y)), cH = ToRgb(P(x, yp1));
			Rgb pick0 = px[0] ? cF : cH;
			Rgb pick1 = px[1] ? cB : cF;
			Rgb pick2 = px[2] ? cD : cB;
			Rgb pick3 = px[3] ? cH : cD;
#undef L
#undef P
			for (int sy = 0; sy < factor; sy++) {
				uint32_t* row = block + static_cast<size_t>(sy) * outWidth;
				for (int sx = 0; sx < factor; sx++) {
					const SubpixelWeights& w = weights[static_cast<size_t>(sy) * factor + sx];
					float m[4];
					for (int k = 0; k < 4; k++) {
						float v30 = edrL[k] ? w.f30[k] : 0.0f;
						float v60 = edrU[k] ? w.f60[k] : 0.0f;
						float v45 = edr[k] ? w.f45[k] : 0.0f;
						float v45i = edri[k] ? w.f45i[k] : 0.0f;
						m[k] = std::max(std::max(v30, v60), std::max(v45, v45i));
					}
					Rgb res1 = Mix(Mix(cE, pick0, m[0]), pick2, m[2]);
					Rgb res2 = Mix(Mix(cE, pick1, m[1]), pick3, m[3]);
					Rgb res = ColorDistance(cE, res2) >= ColorDistance(cE, res1)
						? res2 : res1;
					row[sx] = ToPixel(res);
				}
			}
		}
	}
}

}	// namespace


void
XbrScaler::Scale(const uint32_t* in, int width, int height, int factor,
	uint32_t* out)
{
	if (factor < 1)
		factor = 1;
	std::vector<float> luma(static_cast<size_t>(width) * height);
	for (size_t i = 0; i < luma.size(); i++)
		luma[i] = LumaOf(in[i]);

	std::vector<SubpixelWeights> weights;
	ComputeSubpixelWeights(factor, weights);

	ParallelRows(height, [&](int begin, int end) {
		XbrRect(in, luma.data(), width, height, factor, weights.data(), out,
			0, width, begin, end);
	});
}


// Scalers ------------------------------------------------------------------

void
ScalerFit(int srcWidth, int srcHeight, int dstWidth, int dstHeight, int* x,
	int* y, int* width, int* height)
{
	int w = dstWidth;
	int h = static_cast<int>(static_cast<long long>(dstWidth) * srcHeight / srcWidth);
	if (h > dstHeight) {
		h = dstHeight;
		w = static_cast<int>(static_cast<long long>(dstHeight) * srcWidth / srcHeight);
	}
	if (w < 1)
		w = 1;
	if (h < 1)
		h = 1;
	*width = w;
	*height = h;
	*x = (dstWidth - w) / 2;
	*y = (dstHeight - h) / 2;
}


void
NearestScaler::Layout(int srcWidth, int srcHeight, int dstWidth,
	int dstHeight, int* factor, int* x, int* y)
{
	int f = dstWidth / srcWidth;
	if (dstHeight / srcHeight < f)
		f = dstHeight / srcHeight;
	if (f < 1)
		f = 1;
	*factor = f;
	*x = (dstWidth - srcWidth * f) / 2;
	*y = (dstHeight - srcHeight * f) / 2;
}


void
NearestScaler::Process(const uint32_t* src, int srcWidth, int srcHeight,
	int /*sourceFactor*/, uint32_t* dst, int dstWidth, int dstHeight,
	int dstStride)
{
	int factor, offsetX, offsetY;
	Layout(srcWidth, srcHeight, dstWidth, dstHeight, &factor, &offsetX,
		&offsetY);

	for (int y = 0; y < dstHeight; y++) {
		uint32_t* row = dst + static_cast<size_t>(y) * dstStride;
		int sy = y - offsetY;
		if (sy < 0 || sy >= srcHeight * factor) {
			memset(row, 0, sizeof(uint32_t) * static_cast<size_t>(dstWidth));
			continue;
		}
		const uint32_t* in = src + static_cast<size_t>(sy / factor) * srcWidth;
		int x = 0;
		for (; x < offsetX && x < dstWidth; x++)
			row[x] = 0;
		// Visible part (may be clipped if the output is smaller).
		for (int sx = 0; sx < srcWidth && x < dstWidth; sx++) {
			uint32_t pixel = in[sx];
			for (int i = 0; i < factor && x < dstWidth; i++) {
				if (x >= 0)
					row[x] = pixel;
				x++;
			}
		}
		for (; x < dstWidth; x++)
			row[x] = 0;
	}
}


void
SharpScaler::Process(const uint32_t* src, int srcWidth, int srcHeight,
	int sourceFactor, uint32_t* dst, int dstWidth, int dstHeight,
	int dstStride)
{
	std::vector<uint32_t> art;
	int width, height;
	const uint32_t* image = ArtImage(src, srcWidth, srcHeight, sourceFactor,
		art, &width, &height);
	int rx, ry, rw, rh;
	ScalerFit(width, height, dstWidth, dstHeight, &rx, &ry, &rw, &rh);
	ClearBorders(dst, dstWidth, dstHeight, dstStride, rx, ry, rw, rh);
	Resample(image, width, height, dst, dstStride, rx, ry, rw, rh, true);
}


namespace {

// Art pixels per side of a cache tile.
const int kTile = 16;

// Default budget: xBR work per frame when the picture changes a lot (a
// frame lasts 40 ms), and the least number of tiles per worker.
const double kFrameBudgetMicros = 10000.0;
const int kMinimumTiles = 16;

}	// namespace


void
XbrScaler::Process(const uint32_t* src, int srcWidth, int srcHeight,
	int sourceFactor, uint32_t* dst, int dstWidth, int dstHeight,
	int dstStride)
{
	int width, height;
	const uint32_t* image = ArtImage(src, srcWidth, srcHeight, sourceFactor,
		fArt, &width, &height);
	int rx, ry, rw, rh;
	ScalerFit(width, height, dstWidth, dstHeight, &rx, &ry, &rw, &rh);
	ClearBorders(dst, dstWidth, dstHeight, dstStride, rx, ry, rw, rh);

	// Integer xBR factor that reaches the output size, then resample.
	int factor = static_cast<int>(ceil(static_cast<double>(rw) / width - 1e-6));
	factor = std::min(std::max(factor, 1), 6);
	if (factor == 1) {
		// Output not larger than the art: nothing for xBR to do.
		fValid = false;
		fLastDirty = 0;
		Resample(image, width, height, dst, dstStride, rx, ry, rw, rh, true);
		return;
	}

	if (!fValid || width != fWidth || height != fHeight || factor != fFactor
		|| rw != fOutWidth || rh != fOutHeight) {
		fWidth = width;
		fHeight = height;
		fFactor = factor;
		fOutWidth = rw;
		fOutHeight = rh;
		_Full(image);
	} else
		_Update(image);

	for (int y = 0; y < rh; y++) {
		memcpy(dst + static_cast<size_t>(ry + y) * dstStride + rx,
			fOutput.data() + static_cast<size_t>(y) * rw,
			sizeof(uint32_t) * static_cast<size_t>(rw));
	}
}


void
XbrScaler::_Full(const uint32_t* image)
{
	const int width = fWidth;
	const int height = fHeight;
	const int factor = fFactor;
	const int scaledWidth = width * factor;
	const int scaledHeight = height * factor;

	fPrevious.assign(image, image + static_cast<size_t>(width) * height);
	fLuma.resize(static_cast<size_t>(width) * height);
	for (size_t i = 0; i < fLuma.size(); i++)
		fLuma[i] = LumaOf(image[i]);
	ComputeSubpixelWeights(factor, fWeights);

	fScaled.resize(static_cast<size_t>(scaledWidth) * scaledHeight);
	ParallelRows(height, [&](int begin, int end) {
		XbrRect(image, fLuma.data(), width, height, factor, fWeights.data(),
			fScaled.data(), 0, width, begin, end);
	});

	if (!fMap.Matches(scaledWidth, scaledHeight, fOutWidth, fOutHeight, false))
		fMap.Build(scaledWidth, scaledHeight, fOutWidth, fOutHeight, false);
	fOutput.resize(static_cast<size_t>(fOutWidth) * fOutHeight);
	ParallelRows(fOutHeight, [&](int begin, int end) {
		fMap.Apply(fScaled.data(), fOutput.data(), fOutWidth, 0, fOutWidth,
			begin, end);
	});

	// Output columns / rows belong to the tile of their first source
	// pixel.
	fTilesX = (width + kTile - 1) / kTile;
	fTilesY = (height + kTile - 1) / kTile;
	fOutColumn.resize(fTilesX + 1);
	fOutRow.resize(fTilesY + 1);
	int x = 0;
	for (int tx = 0; tx < fTilesX; tx++) {
		while (x < fOutWidth && fMap.ColumnSource(x) < tx * kTile * factor)
			x++;
		fOutColumn[tx] = x;
	}
	fOutColumn[fTilesX] = fOutWidth;
	int y = 0;
	for (int ty = 0; ty < fTilesY; ty++) {
		while (y < fOutHeight && fMap.RowSource(y) < ty * kTile * factor)
			y++;
		fOutRow[ty] = y;
	}
	fOutRow[fTilesY] = fOutHeight;

	fDirty.assign(static_cast<size_t>(fTilesX) * fTilesY, 0);
	fPending.assign(static_cast<size_t>(fTilesX) * fTilesY, 0);
	fPendingCount = 0;
	if (!fArtMap.Matches(width, height, fOutWidth, fOutHeight, true))
		fArtMap.Build(width, height, fOutWidth, fOutHeight, true);
	fLastDirty = fTilesX * fTilesY;
	fValid = true;
}


void
XbrScaler::_Update(const uint32_t* image)
{
	const int width = fWidth;
	const int height = fHeight;
	const int factor = fFactor;
	const int tilesX = fTilesX;
	const int tilesY = fTilesY;
	std::fill(fDirty.begin(), fDirty.end(), 0);

	// Find the changed pixels per tile. xBR reads two pixels around each
	// input pixel, so everything within two pixels of a change is filtered
	// again.
	for (int ty = 0; ty < tilesY; ty++) {
		int y0 = ty * kTile;
		int y1 = std::min(height, y0 + kTile);
		for (int tx = 0; tx < tilesX; tx++) {
			int x0 = tx * kTile;
			int x1 = std::min(width, x0 + kTile);
			int minX = x1, maxX = -1, minY = y1, maxY = -1;
			for (int y = y0; y < y1; y++) {
				const uint32_t* now = image + static_cast<size_t>(y) * width;
				const uint32_t* before = fPrevious.data() + static_cast<size_t>(y) * width;
				if (memcmp(now + x0, before + x0,
						sizeof(uint32_t) * static_cast<size_t>(x1 - x0)) == 0)
					continue;
				for (int x = x0; x < x1; x++) {
					if (now[x] != before[x]) {
						minX = std::min(minX, x);
						maxX = std::max(maxX, x);
					}
				}
				minY = std::min(minY, y);
				maxY = std::max(maxY, y);
			}
			if (maxX < 0)
				continue;

			for (int y = minY; y <= maxY; y++) {
				size_t row = static_cast<size_t>(y) * width;
				for (int x = minX; x <= maxX; x++) {
					fPrevious[row + x] = image[row + x];
					fLuma[row + x] = LumaOf(image[row + x]);
				}
			}
			int fromX = std::max(minX - 2, 0) / kTile;
			int toX = std::min(maxX + 2, width - 1) / kTile;
			int fromY = std::max(minY - 2, 0) / kTile;
			int toY = std::min(maxY + 2, height - 1) / kTile;
			for (int dy = fromY; dy <= toY; dy++) {
				for (int dx = fromX; dx <= toX; dx++)
					fDirty[static_cast<size_t>(dy) * tilesX + dx] = 1;
			}
		}
	}

	// Tiles to filter: the changed ones first, then the ones left over
	// from earlier frames. With a budget, the rest is shown with a quick
	// bilinear version for now and filtered in the next frames.
	fDirtyList.clear();
	for (int i = 0; i < tilesX * tilesY; i++) {
		if (fDirty[i] != 0u)
			fDirtyList.push_back(i);
	}
	for (int i = 0; i < tilesX * tilesY; i++) {
		if ((fPending[i] != 0u) && (fDirty[i] == 0u))
			fDirtyList.push_back(i);
	}
	// Everything to do is pending until it is filtered.
	for (size_t i = 0; i < fDirtyList.size(); i++)
		fPending[fDirtyList[i]] = 1;
	if (fBudget > 0 && static_cast<int>(fDirtyList.size()) > fBudget)
		fDirtyList.resize(fBudget);

	// 1: xBR for the selected tiles (each writes its own blocks). With the
	// adaptive budget every worker stops at the deadline; what is not done
	// stays pending. From here fDirty means: filtered in this frame.
	std::fill(fDirty.begin(), fDirty.end(), 0);
	if (!fDirtyList.empty()) {
		typedef std::chrono::steady_clock Clock;
		const bool timed = fBudget < 0;
		const Clock::time_point deadline = Clock::now()
			+ std::chrono::microseconds(static_cast<long long>(kFrameBudgetMicros));
		ParallelItems(static_cast<int>(fDirtyList.size()), 8, [&](int begin, int end) {
			for (int i = begin; i < end; i++) {
				if (timed && i - begin >= kMinimumTiles && (i & 3) == 0
					&& Clock::now() > deadline)
					break;
				int tx = fDirtyList[i] % tilesX;
				int ty = fDirtyList[i] / tilesX;
				XbrRect(image, fLuma.data(), width, height, factor,
					fWeights.data(), fScaled.data(), tx * kTile,
					std::min(width, (tx + 1) * kTile), ty * kTile,
					std::min(height, (ty + 1) * kTile));
				fDirty[fDirtyList[i]] = 1;
			}
		});
	}
	for (int i = 0; i < tilesX * tilesY; i++) {
		if (fDirty[i] != 0u)
			fPending[i] = 0;
	}
	fPendingCount = 0;
	fLastDirty = 0;
	for (int i = 0; i < tilesX * tilesY; i++) {
		fPendingCount += fPending[i];
		fLastDirty += fDirty[i];
	}

	if (fLastDirty > 0) {

		// 2: resample the output areas that read a filtered tile. An
		// output area reads its own tile and the tiles to the right and
		// below.
		fDirtyList.clear();
		for (int ty = 0; ty < tilesY; ty++) {
			for (int tx = 0; tx < tilesX; tx++) {
				bool right = tx + 1 < tilesX;
				bool below = ty + 1 < tilesY;
				const uint8_t* d = fDirty.data() + static_cast<size_t>(ty) * tilesX + tx;
				if ((d[0] != 0u) || (right && (d[1] != 0u)) || (below && (d[tilesX] != 0u))
					|| (right && below && (d[tilesX + 1] != 0u)))
					fDirtyList.push_back(ty * tilesX + tx);
			}
		}
		ParallelItems(static_cast<int>(fDirtyList.size()), 8, [&](int begin, int end) {
			for (int i = begin; i < end; i++) {
				int tx = fDirtyList[i] % tilesX;
				int ty = fDirtyList[i] / tilesX;
				fMap.Apply(fScaled.data(), fOutput.data(), fOutWidth,
					fOutColumn[tx], fOutColumn[tx + 1], fOutRow[ty],
					fOutRow[ty + 1]);
			}
		});
	}

	// 3: tiles still waiting for xBR: quick sharp bilinear version straight
	// from the art.
	if (fPendingCount > 0) {
		fDirtyList.clear();
		for (int i = 0; i < tilesX * tilesY; i++) {
			if (fPending[i] != 0u)
				fDirtyList.push_back(i);
		}
		ParallelItems(static_cast<int>(fDirtyList.size()), 8, [&](int begin, int end) {
			for (int i = begin; i < end; i++) {
				int tx = fDirtyList[i] % tilesX;
				int ty = fDirtyList[i] / tilesX;
				fArtMap.Apply(image, fOutput.data(), fOutWidth,
					fOutColumn[tx], fOutColumn[tx + 1], fOutRow[ty],
					fOutRow[ty + 1]);
			}
		});
	}
}


void
ScalerSetThreads(int threads)
{
	sThreadLimit = threads;
}


Scaler*
ScalerForName(const char* name)
{
	static NearestScaler sNearest;
	static SharpScaler sSharp;
	static XbrScaler sXbr;
	if (name != NULL && strcmp(name, "nearest") == 0)
		return &sNearest;
	if (name != NULL && strcmp(name, "sharp") == 0)
		return &sSharp;
	return &sXbr;
}


Scaler*
ScalerNext(Scaler* current)
{
	const char* name = current != NULL ? current->Name() : "xbr";
	if (strcmp(name, "nearest") == 0)
		return ScalerForName("sharp");
	if (strcmp(name, "sharp") == 0)
		return ScalerForName("xbr");
	return ScalerForName("nearest");
}


void
ScalerPlacement(const Scaler* scaler, int srcWidth, int srcHeight, int sourceFactor,
	int dstWidth, int dstHeight, int* x, int* y, int* width, int* height)
{
	if (strcmp(scaler->Name(), "nearest") == 0) {
		int factor;
		NearestScaler::Layout(srcWidth, srcHeight, dstWidth, dstHeight, &factor, x, y);
		*width = srcWidth * factor;
		*height = srcHeight * factor;
		return;
	}
	int factor = sourceFactor < 1 ? 1 : sourceFactor;
	ScalerFit(srcWidth / factor, srcHeight / factor, dstWidth, dstHeight, x, y, width,
		height);
}


void
HdCompose(const uint32_t* const* tiles, const uint8_t* local, int srcWidth, int srcHeight,
	uint32_t* dst, int dstStride, int x, int y, int width, int height)
{
	HdComposer composer;
	composer.Compose(tiles, local, srcWidth, srcHeight, dst, dstStride, x, y, width, height);
}


namespace {

// Bilinear sample of a 32x32 tile at texel position tx, ty (x 256),
// kept inside the tile.
inline uint32_t
SampleTile(const uint32_t* tile, int tx, int ty)
{
	const int kSize = 32;
	if (tx < 0)
		tx = 0;
	if (ty < 0)
		ty = 0;
	if (tx > (kSize - 1) * 256)
		tx = (kSize - 1) * 256;
	if (ty > (kSize - 1) * 256)
		ty = (kSize - 1) * 256;
	int x0 = tx >> 8, y0 = ty >> 8;
	uint32_t fx = static_cast<uint32_t>(tx & 255), fy = static_cast<uint32_t>(ty & 255);
	int x1 = x0 < kSize - 1 ? x0 + 1 : x0;
	int y1 = y0 < kSize - 1 ? y0 + 1 : y0;
	uint32_t a = tile[y0 * kSize + x0], b = tile[y0 * kSize + x1];
	uint32_t c = tile[y1 * kSize + x0], d = tile[y1 * kSize + x1];
	// red and blue together, then green
	uint32_t topRB = ((a & 0xff00ff) * (256 - fx) + (b & 0xff00ff) * fx) >> 8 & 0xff00ff;
	uint32_t bottomRB = ((c & 0xff00ff) * (256 - fx) + (d & 0xff00ff) * fx) >> 8 & 0xff00ff;
	uint32_t topG = ((a & 0x00ff00) * (256 - fx) + (b & 0x00ff00) * fx) >> 8 & 0x00ff00;
	uint32_t bottomG = ((c & 0x00ff00) * (256 - fx) + (d & 0x00ff00) * fx) >> 8 & 0x00ff00;
	uint32_t rb = (topRB * (256 - fy) + bottomRB * fy) >> 8 & 0xff00ff;
	uint32_t g = (topG * (256 - fy) + bottomG * fy) >> 8 & 0x00ff00;
	return 0xff000000u | rb | g;
}

}	// namespace


void
HdComposer::_Compute(int px, int py, const uint32_t* tile, uint8_t local)
{
	const int kScale = 4;
	int baseX = (local & 7) * kScale * 256 - 128;
	int baseY = (local >> 3) * kScale * 256 - 128;
	for (int j = fRowStart[py]; j < fRowStart[py + 1]; j++) {
		uint32_t* out = fCache.data() + static_cast<size_t>(j) * fWidth;
		int ty = baseY + fRowFraction[j];
		for (int i = fColumnStart[px]; i < fColumnStart[px + 1]; i++)
			out[i] = SampleTile(tile, baseX + fColumnFraction[i], ty);
	}
}


void
HdComposer::Compose(const uint32_t* const* tiles, const uint8_t* local, int srcWidth,
	int srcHeight, uint32_t* dst, int dstStride, int x, int y, int width, int height)
{
	const int kScale = 4;
	if (width < 1 || height < 1)
		return;
	size_t count = static_cast<size_t>(srcWidth) * srcHeight;
	if (srcWidth != fSrcWidth || srcHeight != fSrcHeight || width != fWidth
		|| height != fHeight) {
		fSrcWidth = srcWidth;
		fSrcHeight = srcHeight;
		fWidth = width;
		fHeight = height;
		fCache.assign(static_cast<size_t>(width) * height, 0);
		fTiles.assign(count, NULL);
		fLocal.assign(count, 0);
		// Output columns of each source column (by the output pixel
		// centre), and the place inside the source pixel.
		fColumnStart.assign(srcWidth + 1, width);
		fColumnFraction.resize(width);
		for (int i = width - 1; i >= 0; i--) {
			double u = (i + 0.5) * srcWidth / width;
			int p = std::min(static_cast<int>(u), srcWidth - 1);
			fColumnStart[p] = i;
			fColumnFraction[i] = static_cast<int>((u - p) * kScale * 256);
		}
		for (int p = srcWidth - 1; p >= 0; p--)
			fColumnStart[p] = std::min(fColumnStart[p], fColumnStart[p + 1]);
		fRowStart.assign(srcHeight + 1, height);
		fRowFraction.resize(height);
		for (int j = height - 1; j >= 0; j--) {
			double v = (j + 0.5) * srcHeight / height;
			int p = std::min(static_cast<int>(v), srcHeight - 1);
			fRowStart[p] = j;
			fRowFraction[j] = static_cast<int>((v - p) * kScale * 256);
		}
		for (int p = srcHeight - 1; p >= 0; p--)
			fRowStart[p] = std::min(fRowStart[p], fRowStart[p + 1]);
	}

	// What changed is computed again (on several threads when it is much,
	// a new room).
	std::vector<int> changed;
	for (size_t p = 0; p < count; p++) {
		if (tiles[p] != NULL && (tiles[p] != fTiles[p] || local[p] != fLocal[p]))
			changed.push_back(static_cast<int>(p));
		fTiles[p] = tiles[p];
		fLocal[p] = local[p];
	}
	auto work = [&](size_t from, size_t to) {
		for (size_t k = from; k < to; k++) {
			int p = changed[k];
			_Compute(p % srcWidth, p / srcWidth, tiles[p], local[p]);
		}
	};
	if (changed.size() > count / 8) {
		const size_t kThreads = 4;
		std::vector<std::thread> threads;
		size_t step = (changed.size() + kThreads - 1) / kThreads;
		for (size_t t = 1; t < kThreads; t++) {
			size_t from = std::min(changed.size(), t * step);
			size_t to = std::min(changed.size(), from + step);
			threads.emplace_back(work, from, to);
		}
		work(0, std::min(changed.size(), step));
		for (std::thread& thread : threads)
			thread.join();
	} else
		work(0, changed.size());

	// Copy the HD pixels over the picture, a source pixel at a time.
	for (int py = 0; py < srcHeight; py++) {
		const uint32_t* const* tileRow = tiles + static_cast<size_t>(py) * srcWidth;
		for (int j = fRowStart[py]; j < fRowStart[py + 1]; j++) {
			const uint32_t* from = fCache.data() + static_cast<size_t>(j) * fWidth;
			uint32_t* out = dst + static_cast<size_t>(y + j) * dstStride + x;
			int px = 0;
			while (px < srcWidth) {
				if (tileRow[px] == NULL) {
					px++;
					continue;
				}
				int end = px + 1;
				while (end < srcWidth && tileRow[end] != NULL)
					end++;
				int c0 = fColumnStart[px], c1 = fColumnStart[end];
				memcpy(out + c0, from + c0, sizeof(uint32_t) * static_cast<size_t>(c1 - c0));
				px = end;
			}
		}
	}
}
