#pragma once
#include <include/geom.h>
#include <vector>
#include <cassert>

template<typename _rz_in, typename _rz_out>
struct Rasterizer {
protected:
	uint32_t wnd_min_x, wnd_max_x, wnd_min_y, wnd_max_y;
public:
	using rz_in  = _rz_in;
	using rz_out = _rz_out;

	void set_Window(Window const &wnd)
	{
		wnd_min_x = wnd.x;
		wnd_min_y = wnd.y;
		wnd_max_x = wnd.x + wnd.w - 1;
		wnd_max_y = wnd.y + wnd.h - 1;
	}

	virtual void rasterize(rz_in const &, uint32_t,
			       std::vector<std::vector<rz_out>> &) = 0;
};

struct tr_rz_out {
	union {
		struct {
			float bc[3];
			float depth;
		};
		Vec4 sse_data;
	};
	uint32_t x;
	uint32_t pid;
};

struct TrRasterizer final: public Rasterizer<Vec3[3], tr_rz_out> {
public:

	void rasterize(rz_in const &tr, uint32_t pid,
		       std::vector<std::vector<rz_out>> &buf) override
	{
		Vec3 d1_3 = tr[1] - tr[0];
		Vec3 d2_3 = tr[2] - tr[0];
		Vec2 d1 = { d1_3.x, d1_3.y };
		Vec2 d2 = { d2_3.x, d2_3.y };

		float det = d1.x * d2.y - d1.y * d2.x;
		if (det == 0)
			return;

	 	Vec2 min_r{std::min(tr[0].x, std::min(tr[1].x, tr[2].x)),
			   std::min(tr[0].y, std::min(tr[1].y, tr[2].y))};
		Vec2 max_r{std::max(tr[0].x, std::max(tr[1].x, tr[2].x)),
			   std::max(tr[0].y, std::max(tr[1].y, tr[2].y))};
		uint32_t min_x = std::max(int32_t(min_r.x), int32_t(wnd_min_x));
		uint32_t min_y = std::max(int32_t(min_r.y), int32_t(wnd_min_y));
		uint32_t max_x = std::max(std::min(int32_t(max_r.x), int32_t(wnd_max_x)), 0);
		uint32_t max_y = std::max(std::min(int32_t(max_r.y), int32_t(wnd_max_y)), 0);

		Vec2 r0 = Vec2 { tr[0].x, tr[0].y };
		rz_out out;

#define RAST_HACK
#ifdef RAST_HACK
		Vec4 depth_vec = { tr[0].z, tr[1].z, tr[2].z, 0 };
		Vec2 rel_0 = Vec2{float(min_x), float(min_y)} - r0;
		float bc_d1_x =  d2.y / det;
		float bc_d1_y = -d2.x / det;
		float bc_d2_x = -d1.y / det;
		float bc_d2_y =  d1.x / det;
		float bc_d0_x = -bc_d1_x - bc_d2_x;
		float bc_d0_y = -bc_d1_y - bc_d2_y;

		Vec4 pack_dx = { bc_d0_x, bc_d1_x, bc_d2_x, 0 };
		pack_dx[3] = DotProd(pack_dx, depth_vec);
		Vec4 pack_dy = { bc_d0_y, bc_d1_y, bc_d2_y, 0 };
		pack_dy[3] = DotProd(pack_dy, depth_vec);

		Vec4 pack_0 { 1, 0, 0, tr[0].z };
		pack_0 = pack_0 + rel_0.x * pack_dx + rel_0.y * pack_dy;

		for (uint32_t y = min_y; y <= max_y; ++y) {
			Vec4 pack = pack_0;
			for (uint32_t x = min_x; x <= max_x; ++x) {
				if (pack[0] >= 0 && pack[1] >= 0 &&
				    pack[2] >= 0 && pack[3] >= 0) {
					out.x = x;
					out.pid = pid;
					out.sse_data = pack;
					buf[y].push_back(out);
				}
				pack = pack + pack_dx;
			}
			pack_0 = pack_0 + pack_dy;
		}
#else
		Vec3 depth_vec = { tr[0].z, tr[1].z, tr[2].z };
		for (uint32_t y = min_y; y <= max_y; ++y) {
			for (uint32_t x = min_x; x <= max_x; ++x) {
				Vec2 rel = Vec2{float(x), float(y)} - r0;
				float det1 = rel.x * d2.y - rel.y * d2.x;
				float det2 = d1.x * rel.y - d1.y * rel.x;
				out.bc[0] = (det - det1 - det2) / det;
				out.bc[1] = det1 / det;
				out.bc[2] = det2 / det;
				if (out.bc[0] < 0 || out.bc[1] < 0 ||
				     out.bc[2] < 0)
					continue;
				out.depth = DotProd((reinterpret_cast<Vec3&>
						(out.bc)), depth_vec);
				out.x = x;
				out.pid = pid;
				buf[y].push_back(out);
			}
		}
#endif
	}
};
