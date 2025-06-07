/*
 * Copyright (C) 2025 tarosuke<webmaster@tarosuke.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <cmath>
#include <tb/app.h>
#include <tb/canvas.h>
#include <tb/image.h>
#include <tb/prefs.h>
#include <tb/string.h>
#include <tb/vector.h>



struct In {
	In(const tb::Image& i) : image(i) {};
	virtual ~In() {};

	// ベクタ(方向が指すテクスチャ上の色を返す)
	virtual tb::Color GetColor(const tb::Vector<3, float>&) const = 0;

protected:
	const tb::Image& image;
};

struct Out {
	Out(tb::Image& i, const In& in) : image(i), in(in) {
		for (unsigned y(0); y < image.Height(); ++y) {
			for (unsigned x(0); x < image.Width(); ++x) {
				image[y][x] = GetColor(x, y);
			}
		}
	};
	virtual ~Out() {};

protected:
	tb::Image& image;
	const In& in;

	// テクスチャ座標から空間中の方向を求めてin.GetColor
	tb::Color GetColor(unsigned x, unsigned y) const {
		const float xx((2.0f * x) / image.Width() - 1);
		const float yy((2.0f * y) / image.Height() - 1);
		const float ax(std::fabs(xx));
		const float ay(std::fabs(yy));
		const float sx(std::signbit(xx) ? -1 : 1);
		const float sy(std::signbit(yy) ? -1 : 1);

		// 高さ(折り返し部分は負になる)
		const float h(1.0f - ax - ay);

		// 投影して正規化
		if (h < 0) {
			// 下側
			tb::Vector<3, float> v{
				-sx * sy * Fold(ay, sy), sx * sy * Fold(ax, sx), h};
			v.Normalize();
			return in.GetColor(v);
		}

		// 上側
		tb::Vector<3, float> v{-xx, yy, h};
		v.Normalize();
		return in.GetColor(v);
	};

private:
	static float Fold(float a, float s) {
		// 下側なので四隅を折り返す
		return (1 - a) * s;
	};
};


// 正距円筒
struct Equirectangular : In {
	Equirectangular(const tb::Image& i) : In(i) {};

private:
	tb::Color GetColor(const tb::Vector<3, float>& v) const final {
		const float e(std::atan2(v[2], std::sqrt(v[0] * v[0] + v[1] * v[1])));
		const float d(std::atan2(v[1], v[0]));

		return tb::Color(image.Get(
			((d / std::numbers::pi_v<float>)+1.0f) * image.Width() / 2,
			((-e / std::numbers::pi_v<float>)+0.5f) * (image.Height() - 1)));
	};
};

// スカイボックス
struct Box : public In {
	Box(const tb::Image& i) :
		In(i),
		hw(i.Width() / 8.0),
		hh(i.Height() / 6.0),
		max{
			{
				uvw : {1, 2, 0},
				oss :
					{{
						 // 右側面
						 offset : {hw * 5, hh * 3},
						 scale : {-hw, hh}
					 },
					 {
						 // 左側面
						 offset : {hw, hh * 3},
						 scale : {-hw, -hh}
					 }}
			},
			{
				uvw : {0, 1, 2},
				oss :
					{{
						 // 地
						 offset : {hw * 3, hh * 5},
						 scale : {hw, -hh}
					 },
					 {
						 // 天
						 offset : {hw * 3, hh},
						 scale : {-hw, -hh}
					 }}
			},
			{
				uvw : {0, 2, 1},
				oss :
					{{
						 // 前
						 offset : {hw * 3, hh * 3},
						 scale : {hw, hh}
					 },
					 {
						 // 後
						 offset : {hw * 7, hh * 3},
						 scale : {hw, -hh}
					 }}
			},
		},
		handlers{&max[0], &max[0], 0, &max[1], &max[2], 0, &max[2], &max[1]} {};

private:
	const float hw; // 面の半分の幅(画像の1/8)
	const float hh; // 面の半分の高さ(画像の1/6)
	struct Handler {
		unsigned uvw[3];
		struct OS {
			tb::Vector<2, float> offset; // 面の中心
			tb::Vector<2, float> scale;
		} oss[2]; // 対象軸が負[0] / 正[1]
	};
	tb::Color H(float u, float v, float w, const Handler::OS& os) const {
		// u, v: -1.0 to 1.0
		// o: u, v offset
		// u, vを(-hw, -hv) - (hw, hv)に収めてoを足してその座標の色を返す

		return image.Get(
			os.offset[0] + os.scale[0] * u / w,
			os.offset[1] + os.scale[1] * v / w);
	};
	const Handler max[3];
	const Handler* const handlers[8];


	tb::Color GetColor(const tb::Vector<3, float>& v) const final {
		const float ax(std::abs(v[0]));
		const float ay(std::abs(v[1]));
		const float az(std::abs(v[2]));

		/***** a[0], a[1], a[2]で最大のものを選択
		 * ax: 00xb = 0, 1
		 * ay: 1x0b = 4, 6
		 * az: x11b = 3, 7
		 */
		auto& h(*handlers[((ax < ay) * 4) | ((ax < az) * 2) | (ay < az)]);

		const unsigned& w(h.uvw[2]);
		return H(v[h.uvw[0]], v[h.uvw[1]], v[w], h.oss[0 < v[w]]);
	};
};


static tb::Prefs<tb::String> inPath(
	"--in",
	"input file(equirectangler, skybox, etc...)",
	tb::CommonPrefs::nosave);
static tb::Prefs<tb::String>
	outPath("--out", "output file(octahedron)", tb::CommonPrefs::nosave);
static tb::Prefs<tb::String> type(
	"--type", "type: e:equirectangler / s:skpbox", tb::CommonPrefs::nosave);
static tb::Prefs<unsigned>
	pow2scale("--scale", 0, "pow2 scale", tb::CommonPrefs::nosave);
static struct App : tb::App {
	static unsigned DetermineScale(unsigned h, unsigned v) {
		unsigned s(std::max(h, v));

		// 値を二冪にする
		for (unsigned n(0); n < 21; ++n) {
			s |= s >> 1;
		}

		return (s + 1) << pow2scale;
	};
	static void PreProcess(tb::Canvas::Image& in) {
		// 対向辺で境界を埋め、下側を右下に転置
		/**
		 * ①■②③
		 * ■■■■
		 * ④■⑤⑥
		 */
#if 0
		const unsigned w(in.Width() / 4);
		const unsigned h(in.Height() / 3);

		for (unsigned n(0); n < w; ++n) {
			// ①
			in[h - 1][n] = in[n][w];
			in[n][w - 1] = in[h][n];
			// ②
			in[h - 1][2 * w + n] = in[2 * w - n][2 * w - 1];
			in[n][2 * w] = in[h][3 * w - n];
			// ③
			in[h - 1][3 * w + n] = in[0][2 * w - n]; // TODO:ギャップを消す
			// ④
			in[2 * h][n] = in[3 * h - n - 1][2 * w - 1];
			in[2 * h + n][w - 1] = in[2 * h - 1][w - n];
			// ⑤
			in[2 * h][2 * w + n] = in[2 * h + n][2 * w - 1];
			in[2 * h + n][2 * w] = in[2 * h - 1][2 * w + n];
			// ⑥
			in[2 * h + 1][3 * w + n] =
				in[3 * h - 1][2 * w - n]; // TODO:ギャップを消す
		}
#endif
	};
	int Main(uint rem, const char** argv) final {
		tb::Canvas in((std::string)inPath);
		{
			tb::Canvas::Image inImage(in);
			const unsigned scale(
				DetermineScale(inImage.Width(), inImage.Height()));

			tb::Canvas outCanvas(scale, scale);
			{
				tb::Canvas::Image outImage(outCanvas);
				switch (((std::string)type).c_str()[0]) {
				case 'e': {
					Equirectangular eq(inImage);
					Out out(outImage, eq);
				} break;
				case 's': {
					PreProcess(inImage);
					Box box(inImage);
					Out out(outImage, box);
				} break;
				default:
					// TODO:自動認識
					break;
				}
			}
			outCanvas.Save((std::string)outPath);
		}
		return 0;
	};
} app;
