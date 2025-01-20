#include "avisynth.h"
#include "PyramidAVS.h"

PyramidAVS::PyramidAVS(VideoInfo vi, int _levels, bool _dither, VideoInfo* mask_for_vi, bool transpose) : dither(_dither) {
	if (vi.IsYUY2()) throw("Pyramid: YUY2 not supported; use converttoyv16");

	bits = vi.BitsPerComponent();

// set up pyramids
	if (transpose) y_pyramid = new Pyramid(vi.height, vi.width, _levels); else y_pyramid = new Pyramid(vi.width, vi.height, _levels);
	uv_pyramid = y_pyramid;

	VideoInfo* pvi = mask_for_vi ? mask_for_vi : &vi;

	if (pvi->NumComponents() > 1 && !pvi->IsRGB()) {
		sub_w = pvi->GetPlaneWidthSubsampling(PLANAR_U);
		sub_h = pvi->GetPlaneHeightSubsampling(PLANAR_U);
		if (sub_w || sub_h) {
			int w = vi.width >> sub_w;
			int h = vi.height >> sub_h;

//			uv_pyramid = new pyramid(w, h, -y_pyramid->levels_removed(), y_pyramid);
// create a pyramid for chroma, having the same difference in the number of levels from the y_pyramid as would occur using default number of levels for this image
// *** currently not true, as levels is fixed to 8
			if (transpose) std::swap(w, h);
			uv_pyramid = new Pyramid(w, h, max(1, y_pyramid->GetNLevels() - (Pyramid::DefaultNumLevels(vi.width, vi.height) - Pyramid::DefaultNumLevels(w, h))), y_pyramid);
		}
	} else {
		sub_w = 0;
		sub_h = 0;
	}

// set up planes
	const int planesYUV[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
	const int planesRGB[4] = { PLANAR_B, PLANAR_G, PLANAR_R, PLANAR_A };
	const int* _planes = vi.IsRGB() ? planesRGB : planesYUV;

	for (int pid = 0; pid < vi.NumComponents(); pid++) {
		bool uv = _planes[pid] == PLANAR_U || _planes[pid] == PLANAR_V;
		planes.push_back({
			vi.IsPlanar() ? _planes[pid] : 0,
			vi.IsPlanar() ? 0 : pid,
			vi.IsPlanar() ? 0 : vi.NumComponents(),
			!(uv || _planes[pid] == PLANAR_A), uv,
			uv ? uv_pyramid : y_pyramid,
			uv ? sub_w : 0,
			uv ? sub_h : 0
		});
	}

	if (mask_for_vi) planes.push_back({ 0, 0, 0, false, false, uv_pyramid, sub_w, sub_h }); // extra "dummy" plane to be referenced by fuse when subsampling a mask pyramid
}

PyramidAVS::~PyramidAVS() {
	if (y_pyramid != uv_pyramid) {
		delete uv_pyramid;
	}
	delete y_pyramid;
}

void PyramidAVS::Copy(int plane_n, uint8_t* src_p, int src_pitch, bool gamma) {
	if (planes[plane_n].step) {
		src_p += src_pitch * (planes[plane_n].pyramid->GetHeight() - 1);
		src_pitch = -src_pitch;
	}
	int offset = planes[plane_n].offset;
	switch (bits) {
		case 10:
		case 12:
		case 14:
		case 16: offset <<= 1; src_pitch >>= 1; break;
		case 32: offset <<= 2; src_pitch >>= 2; break;
	}

	planes[plane_n].pyramid->Copy(src_p + offset, planes[plane_n].step, src_pitch, gamma && planes[plane_n].gamma, bits);
}

void PyramidAVS::Out(int plane_n, void* dst_p, int dst_pitch, bool gamma, bool clamp, int level) {
	if (planes[plane_n].step) {
		dst_p = ((uint8_t*)dst_p) + dst_pitch * (planes[plane_n].pyramid->GetHeight() - 1);
		dst_pitch = -dst_pitch;
	}

	switch (bits) {
		case 8: planes[plane_n].pyramid->Out((uint8_t*)dst_p, dst_pitch, gamma && planes[plane_n].gamma, dither, clamp, level, planes[plane_n].step, planes[plane_n].offset, planes[plane_n].chroma); break;
		case 10:
		case 12:
		case 14:
		case 16: planes[plane_n].pyramid->Out((uint16_t*)dst_p, dst_pitch >> 1, gamma && planes[plane_n].gamma, dither, clamp, level, planes[plane_n].step, planes[plane_n].offset, planes[plane_n].chroma); break;
		case 32: planes[plane_n].pyramid->Out((float*)dst_p, dst_pitch >> 2, gamma && planes[plane_n].gamma, dither, clamp, level, planes[plane_n].step, planes[plane_n].offset, planes[plane_n].chroma); break;
	}
}
