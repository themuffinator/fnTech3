// Exposure and tone mapping of the traced view, after Q2RTX's tone mapper
// (its tone_mapping_histogram, _curve and _apply shaders), which follows
// Eilertsen, Mantiuk and Unger, "Real-time noise-aware tone mapping" (ACM
// Transactions on Graphics 34(6), 2015). Written for this renderer from that
// design, with Q2RTX's default constants:
//
// - rt_tonemap.comp's first pass histograms the traced view's log luminance,
//   weighing the middle of the view most;
// - its second fits a curve from log luminance to log display luminance to
//   that histogram, giving each populated range of brightness as much of its
//   contrast as the display's range (rtx_rt_tonemap_range stops) allows and
//   none to the ranges nothing occupies, and meters the view as Q2RTX's
//   automatic exposure does, on the mean log luminance between its 70th and
//   90th percentiles; both adapt over time, as an eye does;
// - the composite maps each traced pixel's luminance through the curve,
//   eases channels past a knee toward white, and blends that with Reinhard's
//   curve behind the metered exposure (rtx_rt_tonemap_curve).
//
// The luminance mapped is the light at a pixel, not the surface it lights:
// what a mid-grey surface would return there, plus what the surface reflects
// specularly and gives off (tone_base). A curve flatter than the light's own
// contrast so compresses the light and never the textures' detail, which
// passes through as authored - the detail layer of Eilertsen et al., which
// Q2RTX leaves out, noting that tone mapping the lighting would stand in for
// it. The exposure adapts only so far (RT_TONE_MIN_LUMINANCE and _MAX): a
// view darker than that stays darker, in the curve as in the exposure.
//
// The traced colour is linear light: the path tracer decodes what the
// shaders show (RT_FRAME_LINEAR_LIGHT), or a scene-linear frame's textures
// are decoded already. The tone mapper's result is encoded for the display
// (gamma 2.2), except on a scene-linear frame (r_hdr), which is only exposed
// for the tone mapper after this pass.

#ifndef RT_TONEMAP_GLSL
#define RT_TONEMAP_GLSL

// pc.exposure.x (RT_EXPOSURE_* in rt_gpu.h)
const uint RT_EXPOSURE_NONE = 0u;
const uint RT_EXPOSURE_AUTO = 1u;
const uint RT_EXPOSURE_FIXED = 2u;

// pc.extra.w (RT_COMPOSITE_* in rt_gpu.h)
const uint RT_COMPOSITE_SHOULDER = 1u << 0;
const uint RT_COMPOSITE_KNEE = 1u << 1;
const uint RT_COMPOSITE_LINEAR = 1u << 2;

// pc.view.z in rt_tonemap.comp (RT_TONE_PASS_* in rt_gpu.h)
const uint RT_TONE_PASS_HISTOGRAM = 0u;
const uint RT_TONE_PASS_CURVE = 1u;

const float RT_TONE_LOG_MIN = -24.0;		// log2 luminance at the first bin
const float RT_TONE_LOG_MAX = 8.0;
const float RT_TONE_BIN_STOPS = (RT_TONE_LOG_MAX - RT_TONE_LOG_MIN) / float(RT_TONE_BINS);
const float RT_TONE_FIXED = 64.0;			// a pixel's histogram weight, in fixed point
const float RT_TONE_NOISE_STOPS = -12.0;	// darker light is noise to the curve
const float RT_TONE_NOISE_BLEND = 0.5;
const float RT_TONE_EXPOSURE_BIAS = -1.0;	// stops
const float RT_TONE_KNEE_START = 0.6;
const float RT_TONE_WHITE_POINT = 10.0;
const float RT_TONE_GREY = 0.18;				// the surface tone_base lights
const float RT_TONE_MIN_LUMINANCE = 0.001;		// how far the exposure adapts
const float RT_TONE_MAX_LUMINANCE = 1.0;

// The traced colour at a trace pixel: reflectance times filtered diffuse
// light, plus specular and emission
vec3 traced_color(ivec2 tp, uint final)
{
	return imageLoad(u_reflectance, tp).rgb * denoised_irradiance(tp, final) +
		imageLoad(u_filterSpecular[final], tp).rgb + imageLoad(u_emission, tp).rgb;
}

// The luminance the tone mapper maps at a trace pixel: the diffuse light as
// a mid-grey surface would return it, plus the specular light and emission
float tone_base(ivec2 tp, uint final)
{
	vec3 light = denoised_irradiance(tp, final) * RT_TONE_GREY +
		imageLoad(u_filterSpecular[final], tp).rgb + imageLoad(u_emission, tp).rgb;
	return denoise_luminance(max(light, vec3(0.0)));
}

vec3 tone_encode(vec3 c)
{
	return pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.2));
}

// A luminance's place on the histogram, in bins: bin k stands for log2
// luminance RT_TONE_LOG_MIN + k * RT_TONE_BIN_STOPS
float tone_bin(float lum)
{
	float l = log2(max(lum, exp2(RT_TONE_LOG_MIN)));
	return clamp((l - RT_TONE_LOG_MIN) / RT_TONE_BIN_STOPS, 0.0, float(RT_TONE_BINS - 1u));
}

// Linear light to the display, scaled as the curve and the metered
// exposure behind Reinhard's curve (blended by curveShare) take the pixel's
// light, base (tone_base); bias is in stops
vec3 tone_map(vec3 c, float base, float bias, float curveShare)
{
	float lum = max(base, exp2(RT_TONE_LOG_MIN));
	float x = tone_bin(lum);
	uint left = uint(x);
	uint right = min(left + 1u, RT_TONE_BINS - 1u);
	float curveLog = mix(u_tone.curve[left], u_tone.curve[right], fract(x));
	vec3 fitted = c * (exp2(curveLog + RT_TONE_EXPOSURE_BIAS + bias) / lum);
	// past the knee a channel eases toward white, which it reaches at the
	// white point: y = (w x + a) / (x + b), meeting y = x in value and slope
	// at the knee
	float s = RT_TONE_KNEE_START;
	float white = RT_TONE_WHITE_POINT;
	float w = (s * (s - 2.0) + white) / (white - 1.0);
	vec3 eased = (w * fitted - s * s) / max(fitted + (w - 2.0 * s), vec3(1e-6));
	fitted = mix(fitted, eased, step(vec3(s), fitted));
	// the metered exposure and Reinhard's curve, white at the white point
	float scaled = exp2(RT_TONE_EXPOSURE_BIAS + bias - 2.0) * lum / exp2(u_tone.adaptedLog);
	float reinhard = scaled * (1.0 + scaled / (white * white)) / (1.0 + scaled);
	vec3 exposed = c * (reinhard / lum);
	return clamp(mix(exposed, fitted, curveShare), 0.0, 1.0);
}

// A scene-linear frame's exposure: the metered luminance where Reinhard's
// curve would take it, for the tone mapper after this pass
float tone_exposure(float bias)
{
	return exp2(RT_TONE_EXPOSURE_BIAS + bias - 2.0) / exp2(u_tone.adaptedLog);
}

#endif
