#include "pyramidavs.h"
#include "avisynth.h"
#include <math.h>

class FastBlur : public GenericVideoFilter {
private:
	PyramidAVS* _pyramid;
	PyramidAVS* transpose;
	float blur_x;
	float blur_y;
	float blur_x_c;
	float blur_y_c;
	int iterations;
	bool dither;
	bool gamma;
	float sigma_to_box_radius(double s, int iterations);
//	PVideoFrame test;

	bool has_at_least_v8; // v8 interface frameprop copy support

public:
	FastBlur(PClip _child, double _xblur, double _yblur, int iterations, bool _dither, bool _gamma, float threads, IScriptEnvironment* env);
	~FastBlur();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};

float FastBlur::sigma_to_box_radius(double s, int iterations) {
	double q = s * s*1.0 / iterations;
	int l = (int)floor((sqrt(12 * q + 1) - 1) * 0.5);
	float a = (float)((2 * l + 1) * (l * (l + 1) - 3 * q) / (6 * (q - (l + 1) * (l + 1))));
	return l + a;
}

FastBlur::FastBlur(PClip _child, double _blur_x, double _blur_y, int _iterations, bool _dither, bool _gamma, float threads, IScriptEnvironment* env) : GenericVideoFilter(_child), iterations(_iterations), dither(_dither), gamma(_gamma) {
	has_at_least_v8 = true;
	try { env->CheckVersion(8); }
	catch (const AvisynthError&) { has_at_least_v8 = false; }

	if (threads < 0) {
		Threadpool::GetInstance((std::min)((int)std::thread::hardware_concurrency(), (std::max)(2, (int)floor(std::thread::hardware_concurrency() * 0.5 - 1))));
	} else if (threads == 0) {
		Threadpool::GetInstance();
	} else if (threads < 1) {
		Threadpool::GetInstance((std::max)(1, (int)floor(std::thread::hardware_concurrency() * threads)));
	} else {
		Threadpool::GetInstance((std::min)((int)floor(threads), (int)std::thread::hardware_concurrency()));
	}

	double _blur_x_c, _blur_y_c;
	if (_blur_y == -1) _blur_y = _blur_x;

	if (_blur_x < 0 || _blur_y < 0) env->ThrowError("FastBlur: blur radii must be non-negative");

	if (!vi.IsRGB() && vi.NumComponents() > 1) {
		_blur_x_c = _blur_x / (1 << vi.GetPlaneWidthSubsampling(PLANAR_U));
		_blur_y_c = _blur_y / (1 << vi.GetPlaneHeightSubsampling(PLANAR_U));
	} else {
		_blur_x_c = _blur_x;
		_blur_y_c = _blur_y;
	}

	blur_x = sigma_to_box_radius(_blur_x, iterations);
	blur_y = sigma_to_box_radius(_blur_y, iterations);
	blur_x_c = sigma_to_box_radius(_blur_x_c, iterations);
	blur_y_c = sigma_to_box_radius(_blur_y_c, iterations);

	try {
		_pyramid = new PyramidAVS(child, 1, dither, NULL);
//		debug("py planes: %d", _pyramid->planes.size());
		transpose = new PyramidAVS(child, 1, dither, NULL, true);
//		debug("tp planes: %d", transpose->planes.size());
	} catch (const char *e) {
		env->ThrowError("FastBlur: %s", e);
	}
}

FastBlur::~FastBlur() {
	delete _pyramid;
	delete transpose;
}

PVideoFrame __stdcall FastBlur::GetFrame(int n, IScriptEnvironment* env) {
	int i, p;
	float bx, by;
//	debug("getframe");

//	if (test) env->ThrowError("set");
//	else test = NULL;

	PVideoFrame src = child->GetFrame(n, env);
	PVideoFrame dst = has_at_least_v8 ? env->NewVideoFrameP(vi, &src) : env->NewVideoFrame(vi); // frame property support

	double x_time = 0;
	double y_time = 0;
	
//	debug("%d", (int)_pyramid->planes.size());
	for (p = 0; p < (int)_pyramid->planes.size(); p++) {
//		debug("%d", p);
		uint8_t* dst_p = dst->GetWritePtr(_pyramid->planes[p].plane_id);
		uint8_t* src_p = (uint8_t*)src->GetReadPtr(_pyramid->planes[p].plane_id);
		int dst_pitch = dst->GetPitch(_pyramid->planes[p].plane_id);
		int src_pitch = src->GetPitch(_pyramid->planes[p].plane_id);

		bool _gamma = (gamma && _pyramid->planes[p].gamma);
		_pyramid->Copy(p, src_p, src_pitch, _gamma);
		if (_pyramid->planes[p].plane_id == PLANAR_U || _pyramid->planes[p].plane_id == PLANAR_V) {
			bx = blur_x_c;
			by = blur_y_c;
		} else {
			bx = blur_x;
			by = blur_y;
		}

		Pyramid* a = _pyramid->planes[p].pyramid;
		Pyramid* b = transpose->planes[p].pyramid;

		double div = 1.0;
		double m = ((double)bx * 2 + 1)*((double)by * 2 + 1);

		for (i = 0; i < iterations; i++) {
			a->BlurX(bx, b);
			b->BlurX(by, a);

			div *= m;
			if (div > 1000000) {
				a->Multiply(0, float(1.0 / div));
				div = 1.0;
			}
		}

		if (div > 1) a->Multiply(0, float(1.0 / div));

		_pyramid->Out(p, dst_p, dst_pitch, _gamma, (vi.BitsPerComponent() == 32) | true); // clamping required for float output to avoid accumulation errors resulting in negative values
	}

	return dst;
}

AVSValue __cdecl Create_FastBlur(AVSValue args, void* user_data, IScriptEnvironment* env) {
	PClip clip = args[0].AsClip();
	bool yuy2 = clip->GetVideoInfo().IsYUY2();

	if (yuy2) clip = env->Invoke("ConverttoYV16", clip).AsClip();

	AVSValue out = new FastBlur(
		clip,
		args[1].AsFloat(),
		args[2].AsFloat(-1),
		args[3].AsInt(3),
		args[4].AsBool(false),
		args[5].AsBool(true),
		args[6].AsFloatf(-1),
		env
	);

	if (yuy2) out = env->Invoke("ConverttoYUY2", out.AsClip()).AsClip();

	return out;
}

const AVS_Linkage *AVS_linkage = 0;

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors) {
	AVS_linkage = vectors;

	env->AddFunction("FastBlur", "cf[y_blur]f[iterations]i[dither]b[gamma]b[threads]f", Create_FastBlur, 0);

	return "FastBlur";
}
