#pragma once
#include "Pyramid.h"

struct PyPlaneStruct {
	int plane_id;
	int offset;
	int step;
	bool gamma;
	bool chroma;
	Pyramid* pyramid;
	int sub_w;
	int sub_h;
};

class PyramidAVS {
private:
	bool dither;
	int bits;

public:
	PyramidAVS(VideoInfo vi, int _levels = 0, bool _dither = false, VideoInfo* mask_for_vi = NULL, bool transpose = false);
	PyramidAVS(PClip _clip, int _levels = 0, bool _dither = false, VideoInfo* mask_for_vi = NULL, bool transpose = false) : PyramidAVS(_clip->GetVideoInfo(), _levels, _dither, mask_for_vi, transpose) {};
	~PyramidAVS();
	void Copy(int plane_n, byte* src_p, int src_pitch) { Copy(plane_n, src_p, src_pitch, true); };
	void Copy(int plane_n, unsigned char* src_p, int src_pitch, bool gamma);
//	void Out(int plane_n, byte* dst_p, int dst_pitch, bool gamma, bool clamp) { Out(plane_n, dst_p, dst_pitch, gamma, clamp, 0, NULL); };
	void Out(int plane_n, void* dst_p, int dst_pitch, bool gamma, bool clamp, int level = 0);

	Pyramid* y_pyramid;
	Pyramid* uv_pyramid;
	std::vector<PyPlaneStruct> planes;
	int sub_w;
	int sub_h;
};

//PyramidAVS::PyramidAVS(PClip _clip, int _levels, int _dither, VideoInfo* mask_for_vi, bool transpose = false) {
//	PyramidAVS(_clip->GetVideoInfo(), _levels, _dither, mask_for_vi, transpose);
//}
