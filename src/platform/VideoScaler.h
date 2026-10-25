/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef MOG_VIDEO_SCALER_H
#define MOG_VIDEO_SCALER_H


// Scaling of the 640x400 game image to the window or screen size.
//
// nearest  integer factor, centred, black borders (version 1)
// sharp    sharp bilinear: fills the window keeping the 16:10 aspect, pixels
// stay sharp, no uneven pixel widths
// xbr      Hyllian's xBR-lv2 (smooth contours), then resampled to the window
//
// 'sourceFactor' tells a filter how many output pixels one art pixel covers in
// the source: 2 for the "original" set (320x200 art stored doubled), 1 for the
// other sets. xBR works on the art resolution, so it first undoes the
// doubling.

#include <stdint.h>
#include <vector>


// Bilinear (or sharp bilinear) resampling tables from a srcWidth x srcHeight
// image to a width x height rectangle.
class ResampleMap {
public:
								ResampleMap()
									: fSrcWidth(0), fSrcHeight(0), fWidth(0),
									fHeight(0), fSharp(false) {}
			void				Build(int srcWidth, int srcHeight, int width,
									int height, bool sharp);
			bool				Matches(int srcWidth, int srcHeight, int width,
									int height, bool sharp) const;
			// Writes the output pixels [x0, x1) x [y0, y1); dst points at
			// output pixel (0, 0).
			void				Apply(const uint32_t* src, uint32_t* dst,
									int stride, int x0, int x1, int y0,
									int y1) const;

			// First source column / row used by an output column / row (the
			// next one is used as well).
			int					ColumnSource(int x) const
									{ return fColIndex[x]; }
			int					RowSource(int y) const
									{ return fRowIndex[y]; }

private:
			int					fSrcWidth;
			int					fSrcHeight;
			int					fWidth;
			int					fHeight;
			bool				fSharp;
			std::vector<int>	fColIndex;
			std::vector<int>	fColWeight;
			std::vector<int>	fRowIndex;
			std::vector<int>	fRowWeight;
};


// Per output subpixel weights of the xBR edge rules.
struct XbrWeights {
	float	f45i[4];
	float	f45[4];
	float	f30[4];
	float	f60[4];
};


class Scaler {
public:
	virtual					~Scaler() {}
	virtual	const char*		Name() const = 0;
	// src: srcWidth x srcHeight pixels (0xffRRGGBB). dst has dstWidth x
	// dstHeight pixels, dstStride pixels per row.
	virtual	void			Process(const uint32_t* src, int srcWidth,
								int srcHeight, int sourceFactor,
								uint32_t* dst, int dstWidth, int dstHeight,
								int dstStride) = 0;
};


class NearestScaler : public Scaler {
public:
	virtual	const char*		Name() const { return "nearest"; }
	virtual	void			Process(const uint32_t* src, int srcWidth,
								int srcHeight, int sourceFactor,
								uint32_t* dst, int dstWidth, int dstHeight,
								int dstStride);

	// Integer factor and placement used for a given output size.
	static	void			Layout(int srcWidth, int srcHeight, int dstWidth,
								int dstHeight, int* factor, int* x, int* y);
};


class SharpScaler : public Scaler {
public:
	virtual	const char*		Name() const { return "sharp"; }
	virtual	void			Process(const uint32_t* src, int srcWidth,
								int srcHeight, int sourceFactor,
								uint32_t* dst, int dstWidth, int dstHeight,
								int dstStride);
};


class XbrScaler : public Scaler {
public:
	virtual	const char*		Name() const { return "xbr"; }
	virtual	void			Process(const uint32_t* src, int srcWidth,
								int srcHeight, int sourceFactor,
								uint32_t* dst, int dstWidth, int dstHeight,
								int dstStride);

	// xBR-lv2 at an integer factor (1..6): out has width*factor x
	// height*factor pixels. Exposed for tests.
	static	void			Scale(const uint32_t* in, int width, int height,
								int factor, uint32_t* out);

	// Tiles filtered again by the last Process() call and the number of
	// tiles (for the timing display and the tests).
			int				LastDirtyTiles() const { return fLastDirty; }
			int				TileCount() const { return fTilesX * fTilesY; }

	// Forgets the cache: the next frame is filtered completely.
			void			Invalidate() { fValid = false; }

	// Most xBR work per frame after the first one: -1 about 10 ms (the
	// default), 0 no limit, or a fixed number of tiles. Tiles over the
	// budget show a sharp bilinear version until they are done; once nothing
	// changes for a few frames the picture is
	// identical to filtering completely.
			void			SetTileBudget(int tiles) { fBudget = tiles; }
			int				PendingTiles() const { return fPendingCount; }

private:
			void			_Full(const uint32_t* image);
			void			_Update(const uint32_t* image);

			// Only the parts of the image that changed since the previous
			// frame are filtered again; the members below are that cache.
			std::vector<uint32_t> fArt;	// source, doubling undone
			std::vector<uint32_t> fPrevious;	// art of the previous frame
			std::vector<float> fLuma;
			std::vector<XbrWeights> fWeights;
			std::vector<uint32_t> fScaled;	// xBR result
			std::vector<uint32_t> fOutput;	// resampled, rw x rh
			ResampleMap		fMap;
			ResampleMap		fArtMap;	// art -> output, for pending tiles
			std::vector<uint8_t> fPending;
			std::vector<int> fOutColumn;	// first output column per tile column
			std::vector<int> fOutRow;	// first output row per tile row
			std::vector<uint8_t> fDirty;
			std::vector<int> fDirtyList;
			int				fWidth = 0;
			int				fHeight = 0;
			int				fFactor = 0;
			int				fOutWidth = 0;
			int				fOutHeight = 0;
			int				fTilesX = 0;
			int				fTilesY = 0;
			int				fLastDirty = 0;
			int				fPendingCount = 0;
			int				fBudget = -1;
			bool			fValid = false;
};


// Largest rectangle with the aspect of srcWidth:srcHeight that fits in
// dstWidth x dstHeight, centred.
void ScalerFit(int srcWidth, int srcHeight, int dstWidth, int dstHeight,
	int* x, int* y, int* width, int* height);

// Where a scaler puts a srcWidth x srcHeight picture in dstWidth x
// dstHeight: the rectangle x, y, width, height.
void ScalerPlacement(const Scaler* scaler, int srcWidth, int srcHeight,
	int sourceFactor, int dstWidth, int dstHeight, int* x, int* y, int* width,
	int* height);

// Paints HD tiles over a scaled picture. Per source pixel: 'tiles' the
// 32x32 tile (0xffRRGGBB) or NULL to keep what is there, 'local' its place
// in the 8x8 cell (x | y << 3). The picture fills x, y, width, height of
// dst (ScalerPlacement()). Smooth (bilinear) inside each tile.
void HdCompose(const uint32_t* const* tiles, const uint8_t* local, int srcWidth,
	int srcHeight, uint32_t* dst, int dstStride, int x, int y, int width,
	int height);

// HdCompose() for a stream of frames: keeps the result per source pixel
// and only computes again what changed (most frames: a few sprites).
class HdComposer {
public:
			void			Compose(const uint32_t* const* tiles, const uint8_t* local,
								int srcWidth, int srcHeight, uint32_t* dst,
								int dstStride, int x, int y, int width, int height);

private:
			void			_Compute(int px, int py, const uint32_t* tile, uint8_t local);

			int				fSrcWidth = 0;
			int				fSrcHeight = 0;
			int				fWidth = 0;
			int				fHeight = 0;
			std::vector<uint32_t> fCache;	// width x height
			std::vector<const uint32_t*> fTiles;	// last frame
			std::vector<uint8_t> fLocal;
			std::vector<int> fColumnStart;	// per source column, + 1 entry
			std::vector<int> fRowStart;
			std::vector<int> fColumnFraction;	// per output column
			std::vector<int> fRowFraction;
};

// Returns the scaler for a name ("nearest", "sharp", "xbr"); unknown names
// give "xbr". Never NULL.
Scaler* ScalerForName(const char* name);

// Limits the worker threads of the filters (0: number of cores, max 4).
// For tests and measurements.
void ScalerSetThreads(int threads);

// Next scaler in the cycle nearest -> sharp -> xbr -> nearest.
Scaler* ScalerNext(Scaler* current);


#endif	// MOG_VIDEO_SCALER_H
