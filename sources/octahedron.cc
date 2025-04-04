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
	Equirectangular(tb::Image& i) : In(i) {};

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
	Box(tb::Image& i) : In(i) {};

private:
	tb::Color GetColor(const tb::Vector<3, float>& v) const final {

		return tb::Color();
	};
};


static tb::Prefs<tb::String> inPath("--in",
	"input file(equirectangler, skybox, etc...)",
	tb::CommonPrefs::nosave);
static tb::Prefs<tb::String> outPath(
	"--out", "output file(octahedron)", tb::CommonPrefs::nosave);
static struct App : tb::App {
	int Main(uint rem, const char** argv) final {
		tb::Canvas in((std::string)inPath);

		tb::Canvas::Image inImage(in);
		Equirectangular eq(inImage);
		const unsigned length(inImage.Width() < inImage.Height()
								  ? inImage.Height()
								  : inImage.Width());

		tb::Canvas outCanvas(length, length);
		{
			tb::Canvas::Image outImage(outCanvas);
			Out out(outImage, eq);
		}
		outCanvas.Save((std::string)outPath);

		return 0;
	};
} app;
